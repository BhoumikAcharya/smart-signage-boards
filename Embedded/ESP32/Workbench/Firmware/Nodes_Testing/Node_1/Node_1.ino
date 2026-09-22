/*
 *
 * These are the values for the test PCB_1 (Marked with masking tape)
 * ESP32 Enterprise Signage Controller — Firmware v4.1.0-p2
 * ========================================================
 * Target : PCB V3, ESP32 + LAN8720 + MCP23017 (10 MOSFETs) + 4x ACS712 + battery
 * Pairs with : RaspberryPi/Gateway/Pi.py v2.0.0
 * Contract   : ESP32/esp32_contract.md
 *
 * PHASE 1 · PART 2 — HARDCODED BENCH BUILD, NOW WITH BATTERY.
 *   v4.0.0-p1 (Part 1) plus a coarse battery gauge. Still fully hardcoded:
 *   edit the constants below and reflash; no runtime persistence (NVS is Phase 2).
 *
 * WHAT'S NEW SINCE v4.0.0-p1:
 *   1. BATTERY GAUGE. GPIO39 via a 100k/18k divider, read with the factory ADC
 *      calibration (analogReadMilliVolts) like the calibration tool, heavily
 *      filtered, and published as a COARSE 0/50/100 on battery_pct. A precise %
 *      is not achievable on this hardware (flat LFP curve + load sag + ADC noise)
 *      — see Workbench/Docs/Phase1-Part2_Battery-Calibration-Report.md. On mains the pack
 *      sits high and reads 100; when the PSU fails the number just follows the
 *      pack down (no charging-detection branch — voltage does it).
 *   2. WATCHDOG FIX. arduino-esp32 3.x already starts the Task WDT, so our old
 *      esp_task_wdt_init() silently failed ("TWDT already initialized") and the
 *      intended 15 s never applied — a blocking network call then tripped the
 *      short default and rebooted the node, dropping Ethernet. We now RECONFIGURE
 *      the existing WDT, and bound the MQTT connect/read so the loop can't stall.
 *   3. Serial menu adds 'b' — a separate battery stream (raw ADC, mV, Vbatt, %),
 *      distinct from 's', for calibrating the battery while on the PSU.
 *
 * CALIBRATION HONESTY: SENS/ZERO/divider values are per-board and edited by hand.
 * The battery thresholds are PROVISIONAL (Option B, coarse) — refine after a full
 * discharge-to-cutoff test. Current-health for the arrows stays intent-echo.
 *
 * WHAT CARRIED OVER FROM v3.0.0 (unchanged, deliberate — see esp32_contract.md §8):
 *   - I2C on 4/13 (GPIO17 is the LAN8720 50MHz clock).
 *   - 4 ACS712 channels: current1/2 = LHS/RHS arrows (intent-echo), current3/4 =
 *     static zones 1/2 (measured 4-state, hardwired always-on).
 *   - Modes 0-6 plus the 10000-11023 raw MOSFET bitmask. 2-LED wrapping chase.
 *   - 'state' topic verified after the MOSFET write; MCP unreachable => 65535.
 *   - Validated command parse; network-first boot; hold-last-command animation.
 */

#include <ETH.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// ========================================================
//  USER CONFIGURATION — CHANGE PER UNIT
// ========================================================
const char* mqtt_server_ip   = "192.168.1.10";     // Pi, ESP32-side static IP
const int   mqtt_port        = 1883;
const int   ASSIGNED_REGISTER = 40001;              // node 1 = 40001, node 2 = 40002 ...

IPAddress local_IP  (192, 168, 1, 101);             // must be unique per unit
IPAddress gateway_ip(192, 168, 1, 1);
IPAddress subnet    (255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);

// ========================================================
//  SENSOR CALIBRATION — HARDCODED (per-board, hand-calibrated)
// ========================================================
/*
 * ACS712 SENSITIVITY: all four are 5A parts (0.185 V/A nominal). The datasheet
 * value is not trusted; SENS/ZERO are hand-calibrated per board and reflashed.
 */
const float SENS_LEFT = 0.185f, ZERO_LEFT = 2.20f;    // arrows  — 5A part
const float SENS_RGHT = 0.185f, ZERO_RGHT = 2.450f;   // arrows  — 5A part
const float SENS_STA1 = 0.185f, ZERO_STA1 = 2.437f;   // static  — 5A part
const float SENS_STA2 = 0.185f, ZERO_STA2 = 2.28f;   // static  — 5A part

// PSU voltage divider: Vbus = Vadc * ratio. Per-board — set with a multimeter.
const float PSU_DIV_RATIO = 6.9f;

/*
 * PSU FAIL DETECTION — hysteresis + debounce.
 * 'power=FAIL' from an ONLINE node is the system's critical alarm. Fail below
 * FAIL_VOLTS, recover only above OK_VOLTS, hold in between; require N agreeing
 * reads before flipping so a dip/inrush can't publish a one-cycle FAIL.
 */
const float PSU_FAIL_VOLTS = 10.0f;   // below this => FAIL
const float PSU_OK_VOLTS   = 11.0f;   // above this => OK (must be > PSU_FAIL_VOLTS)
const int   PWR_DEBOUNCE_COUNT = 3;   // consecutive agreeing reads before flipping

// ========================================================
//  BATTERY  (Phase 1 · Part 2)  — coarse 0/50/100 gauge
// ========================================================
/*
 * Battery: 4S LiFePO4, 12.8 V nominal, 6 Ah — Pro-range "orange pack", with BMS.
 * Sense: GPIO39 via 100k (batt+ -> pin) / 18k (pin -> GND) -> ratio (100+18)/18.
 *
 * Read with analogReadMilliVolts() (factory eFuse ADC calibration) exactly like
 * the bench tool, so the thresholds transfer. HEAVILY filtered — the signal is
 * slow and noisy (~±0.3 V raw). Reported COARSE 0/50/100 only; a precise % is not
 * achievable (flat LFP + load sag + noise). Thresholds are on the LOADED voltage
 * with hysteresis to stop boundary chatter. PROVISIONAL — refine after a full
 * discharge test. See Workbench/Docs/Phase1-Part2_Battery-Calibration-Report.md.
 *
 * No charging-detection branch: on mains the pack sits high (~13.8 V) -> reads
 * 100; on PSU failure the number follows the pack down. 65535 (unknown) is the
 * absence of a reading — we never publish 0 except as a genuine near-empty band.
 */
const int   PIN_VOLT_BATT   = 39;       // ADC1_CH3 — battery sense
const float BATT_DIV_RATIO  = 6.556f;   // (100+18)/18 nominal; calibrate per board (menu 'b')
const float BATT_FULL_V     = 11.5f;    // >= this            -> 100
const float BATT_LOW_V      = 10.8f;    // >= this (and <FULL) -> 50 ; below -> 0
const float BATT_HYST_V     = 0.15f;    // hysteresis around each threshold
const float BATT_ALPHA      = 0.10f;    // IIR weight — heavy filter for a slow signal

// ========================================================
//  DISCREPANCY THRESHOLDS
// ========================================================
const float THRESH_PER_STRIP = 0.060f;  // min acceptable current for ONE strip
const float THRESHOLD_STATIC = 0.200f;  // static zones: load never changes

/*
 * Expected-OFF side gets a LOW threshold to catch a stuck-on MOSFET. ORDERING:
 * NOISE_FLOOR must stay BELOW THRESH_OFF_DETECT, or the noise gate would zero out
 * exactly the currents FAIL_SHORT is meant to catch.
 */
const float THRESH_OFF_DETECT = 0.030f;  // expected-OFF side: above this => FAIL_SHORT
const float NOISE_FLOOR       = 0.020f;  // below this => treat as 0 A

const float ADC_REF = 3.3f;
const float ALPHA   = 0.15f;   // IIR low-pass (current channels)

/*
 * MODE-CHANGE SETTLE — the current IIR is slow, so snap it on a mode change and
 * suppress state evaluation briefly while the load physically settles.
 */
const unsigned long MODE_SETTLE_MS = 1200;
volatile unsigned long lastModeChangeMs = 0;

// ========================================================
//  HARDWARE PIN MAP — finalized 2026-07-13
// ========================================================
#define I2C_SDA 4      // was 16
#define I2C_SCL 13     // was 17 — collided with ETH_CLOCK_GPIO17_OUT
#define MCP_ADDR 0x20
Adafruit_MCP23X17 mcp;

const int PIN_VOLT_PSU  = 36;
const int PIN_CURR_LEFT = 34;   // ACS712 #1  LHS arrows    (switched)
const int PIN_CURR_RGHT = 35;   // ACS712 #2  RHS arrows    (switched)
const int PIN_CURR_STA1 = 32;   // ACS712 #3  Static Zone 1 (always on)
const int PIN_CURR_STA2 = 33;   // ACS712 #4  Static Zone 2 (always on)
// PIN_VOLT_BATT (GPIO39) is defined in the BATTERY section above.

#define ETH_PHY_ADDR  1
#define ETH_PHY_POWER -1
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_CLK_MODE  ETH_CLOCK_GPIO17_OUT

/*
 * MCP23017 HEALTH PATTERN — drive a fixed pattern into the unused A5-A7 / B5-B7
 * bits and read it back, so a dead bus (which reads 0x00) can't masquerade as a
 * legitimate "all MOSFETs off" write.
 */
const uint8_t MCP_HEALTH_PATTERN = 0xA0;   // bits 7 and 5 of the unused nibble

// ========================================================
//  MQTT TOPICS
// ========================================================
char topic_cmd[64], topic_status[64], topic_power[64], topic_state[64];
char topic_curr1[64], topic_curr2[64], topic_curr3[64], topic_curr4[64];
char topic_batt[64];
const char* topic_scan = "metro/signage/scan";

WiFiClient ethClient;
PubSubClient client(ethClient);
bool eth_connected = false;

// ========================================================
//  STATE
// ========================================================
volatile int  commandedValue = 0;    // last VALID command received
volatile int  activeCommand  = 0;    // what we have actually, verifiably executed
volatile bool mcpHealthy     = false;

volatile bool  expectLeftOn  = false;
volatile bool  expectRightOn = false;
volatile float dynamicThreshLeft  = THRESH_OFF_DETECT;
volatile float dynamicThreshRight = THRESH_OFF_DETECT;

uint8_t lastPortA = 0x00, lastPortB = 0x00;   // intent, for re-apply after recovery

struct NodeStateMsg {
  bool power_ok;
  const char* state_left;
  const char* state_right;
  const char* state_sta1;
  const char* state_sta2;
  int battery_pct;          // coarse 0/50/100; -1 = not sampled yet (=> not published)
};
QueueHandle_t sensorQueue;
NodeStateMsg networkState = {true, "OFF", "OFF", "OFF", "OFF", -1};
volatile bool haveSensorData = false;  // written on core 0, read on core 1

/*
 * Chase 1 (wrapping), 2-LED arrow, direction A4 -> A0. Five frames, repeating.
 * Bits 4->0: 11000, 01100, 00110, 00011, 10001 (last = wrap frame, intentional).
 * Reference: Workbench/Firmware_Test/demo_mode_v2.0.0/CHASE_ANIMATION_SPEC.md.
 */
const uint8_t chaseFrames[5] = {0x18, 0x0C, 0x06, 0x03, 0x11};
const int numFrames = 5;
const unsigned long FRAME_MS = 250;   // 5 frames x 250 ms = 1.25 s per sweep

// A named handler, not a lambda: WiFi.onEvent() is overloaded and a lambda can
// make the overload ambiguous.
void eth_event_handler(arduino_event_id_t event) {
  if (event == ARDUINO_EVENT_ETH_GOT_IP)            eth_connected = true;
  else if (event == ARDUINO_EVENT_ETH_DISCONNECTED) eth_connected = false;
}

// ========================================================
//  MCP23017 — WRITE, VERIFY, RECOVER
// ========================================================
bool writePortsVerified(uint8_t a, uint8_t b) {
  uint8_t wantA = a | MCP_HEALTH_PATTERN;
  uint8_t wantB = b | MCP_HEALTH_PATTERN;

  mcp.writeGPIOA(wantA);
  mcp.writeGPIOB(wantB);

  uint8_t gotA = mcp.readGPIOA();
  uint8_t gotB = mcp.readGPIOB();

  return (gotA == wantA && gotB == wantB);
}

void setMcpHealth(bool healthy);   // fwd

// Non-blocking re-init attempt. Called from loop() whenever the MCP is unhealthy.
void ensureMcpHealthy() {
  if (mcpHealthy) return;

  static unsigned long lastTry = 0;
  if (millis() - lastTry < 2000) return;
  lastTry = millis();

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!mcp.begin_I2C(MCP_ADDR, &Wire)) return;

  for (int i = 0; i < 16; i++) mcp.pinMode(i, OUTPUT);

  if (writePortsVerified(lastPortA, lastPortB)) {
    Serial.println(F("[MCP] Recovered. Output state re-applied."));
    setMcpHealth(true);
  }
}

// ========================================================
//  ADC HELPERS
// ========================================================
int getMedianADC(int pin) {
  int s[51];
  for (int i = 0; i < 51; i++) s[i] = analogRead(pin);
  for (int i = 1; i < 51; i++) {
    int key = s[i], j = i - 1;
    while (j >= 0 && s[j] > key) { s[j + 1] = s[j]; j--; }
    s[j + 1] = key;
  }
  return s[25];
}

// Median of 51 calibrated-millivolt reads (factory eFuse cal). Used for BATTERY,
// which needs the accuracy the naive raw/4095*3.3 cannot give.
uint32_t getMedianMv(int pin) {
  uint32_t s[51];
  for (int i = 0; i < 51; i++) s[i] = analogReadMilliVolts(pin);
  for (int i = 1; i < 51; i++) {
    uint32_t key = s[i]; int j = i - 1;
    while (j >= 0 && s[j] > key) { s[j + 1] = s[j]; j--; }
    s[j + 1] = key;
  }
  return s[25];
}

float adcVolts(int pin) { return (getMedianADC(pin) / 4095.0f) * ADC_REF; }

float readCurrent(int pin, float zero, float sens) {
  return fabs((adcVolts(pin) - zero) / sens);
}

float readBatteryVolts() {
  return (getMedianMv(PIN_VOLT_BATT) / 1000.0f) * BATT_DIV_RATIO;
}

/*
 * Coarse battery band with hysteresis. Returns 100 / 50 / 0.
 * `cur` is the previous band (-1 for a fresh, no-hysteresis evaluation). The
 * hysteresis band (BATT_HYST_V) around each threshold stops the output chattering
 * when the filtered voltage sits right on a boundary.
 */
int batteryBand(float v, int cur) {
  const float H = BATT_HYST_V;
  if (cur != 100 && cur != 50 && cur != 0) {      // first eval — plain thresholds
    if (v >= BATT_FULL_V) return 100;
    if (v >= BATT_LOW_V)  return 50;
    return 0;
  }
  if (cur == 100) {
    if (v < BATT_FULL_V - H) return (v < BATT_LOW_V - H) ? 0 : 50;
    return 100;
  }
  if (cur == 50) {
    if (v >= BATT_FULL_V + H) return 100;
    if (v <  BATT_LOW_V  - H) return 0;
    return 50;
  }
  // cur == 0
  if (v >= BATT_FULL_V + H) return 100;
  if (v >= BATT_LOW_V  + H) return 50;
  return 0;
}

// ========================================================
//  TELEMETRY
// ========================================================
void publishState() {
  if (!client.connected()) return;

  /*
   * MCP unreachable -> we don't know the outputs -> publish "FAULT" (gateway maps
   * to 65535, an un-clearable fault). Otherwise publish the real active command.
   */
  if (!mcpHealthy) {
    client.publish(topic_state, "FAULT", true);
    return;
  }
  char buf[12];
  snprintf(buf, sizeof(buf), "%d", activeCommand);
  client.publish(topic_state, buf, true);
}

void setMcpHealth(bool healthy) {
  if (mcpHealthy == healthy) return;
  mcpHealthy = healthy;
  if (!healthy) Serial.println(F("[MCP] UNHEALTHY — outputs unverifiable, publishing FAULT."));
  publishState();
}

void publish_state_msg(NodeStateMsg msg) {
  if (!client.connected()) return;

  char onlineMsg[40];
  snprintf(onlineMsg, sizeof(onlineMsg), "ONLINE:%s", ETH.localIP().toString().c_str());
  client.publish(topic_status, onlineMsg, true);
  client.publish(topic_power, msg.power_ok ? "OK" : "FAIL", true);

  // Suppress readings until the first real sensor cycle, so the gateway never
  // sees a startup default that looks like a reading.
  if (haveSensorData) {
    client.publish(topic_curr1, msg.state_left,  true);
    client.publish(topic_curr2, msg.state_right, true);
    client.publish(topic_curr3, msg.state_sta1,  true);
    client.publish(topic_curr4, msg.state_sta2,  true);

    // Battery: coarse 0/50/100. Publish only once sampled (>=0). A missing value
    // stays 65535 (unknown) at the gateway — we never send 65535 ourselves and
    // never send a bare 0 unless the pack is genuinely in the near-empty band.
    if (msg.battery_pct >= 0) {
      char b[8];
      snprintf(b, sizeof(b), "%d", msg.battery_pct);
      client.publish(topic_batt, b, true);
    }
  }

  publishState();
}

// ========================================================
//  COMMAND DECODE
// ========================================================
bool isValidCommand(long v) {
  if (v >= 0 && v <= 6) return true;
  if (v >= 10000 && v <= 11023) return true;
  return false;
}

void callback(char* topic, byte* payload, unsigned int length) {
  char msgBuffer[16];
  unsigned int copyLength = (length < sizeof(msgBuffer) - 1) ? length : (sizeof(msgBuffer) - 1);
  memcpy(msgBuffer, payload, copyLength);
  msgBuffer[copyLength] = '\0';

  if (strcmp(topic, topic_scan) == 0 && strcmp(msgBuffer, "PING") == 0) {
    publish_state_msg(networkState);
    return;
  }

  // Validated parse: strtol tells "the number zero" from "not a number", so a
  // corrupt payload is rejected instead of silently blanking the sign.
  char* endp = nullptr;
  long v = strtol(msgBuffer, &endp, 10);

  if (endp == msgBuffer || *endp != '\0') {
    Serial.printf("[CMD] REJECTED malformed payload: '%s' (still running %d)\n",
                  msgBuffer, activeCommand);
    publishState();
    return;
  }
  if (!isValidCommand(v)) {
    Serial.printf("[CMD] REJECTED out-of-range: %ld (still running %d)\n",
                  v, activeCommand);
    publishState();
    return;
  }

  commandedValue = (int)v;
  Serial.printf("[CMD] Accepted: %d\n", commandedValue);
}

int countActiveStrips(uint8_t portMask) {
  int count = 0;
  portMask &= 0x1F;
  while (portMask) { count += portMask & 1; portMask >>= 1; }
  return count;
}

/*
 * Set the discrepancy threshold for BOTH sides on every mode change. Only setting
 * the side turning ON leaves stale thresholds; modes 4/5 are the trap.
 */
void setThresholds(int nLeft, int nRight) {
  if (nLeft > 0) {
    expectLeftOn = true;
    dynamicThreshLeft = THRESH_PER_STRIP * (nLeft - 0.5f);
  } else {
    expectLeftOn = false;
    dynamicThreshLeft = THRESH_OFF_DETECT;
  }
  if (nRight > 0) {
    expectRightOn = true;
    dynamicThreshRight = THRESH_PER_STRIP * (nRight - 0.5f);
  } else {
    expectRightOn = false;
    dynamicThreshRight = THRESH_OFF_DETECT;
  }
}

/*
 * The animation state machine. Runs UNCONDITIONALLY from loop() — keeps animating
 * through a broker/link outage (hold-last-command); if the MCP is dead the send
 * fails harmlessly but bookkeeping ticks so the sign resumes when I2C recovers.
 */
void runAnimationStateMachine() {
  static unsigned long previousMillis = 0;
  static int currentFrame = 0;
  static int lastExecutedCommand = -1;

  unsigned long now = millis();
  int cmd = commandedValue;
  bool modeChanged = (cmd != lastExecutedCommand);

  uint8_t portA = 0x00, portB = 0x00;
  bool doWrite = false;

  if (cmd >= 0 && cmd <= 6) {
    bool chasing = (cmd == 1 || cmd == 2 || cmd == 3);

    if (modeChanged || (chasing && now - previousMillis >= FRAME_MS)) {
      if (chasing) previousMillis = now;
      doWrite = true;

      switch (cmd) {
        case 0: portA = 0x00;                    portB = 0x00;                    break;
        case 1: portA = chaseFrames[currentFrame]; portB = 0x00;                  break;
        case 2: portA = 0x00;                    portB = chaseFrames[currentFrame]; break;
        case 3: portA = chaseFrames[currentFrame]; portB = chaseFrames[currentFrame]; break;
        case 4: portA = 0x1F;                    portB = 0x00;                    break;  // LEFT only
        case 5: portA = 0x00;                    portB = 0x1F;                    break;  // RIGHT only
        case 6: portA = 0x1F;                    portB = 0x1F;                    break;  // ALL
      }

      if (modeChanged) {
        switch (cmd) {
          case 0: setThresholds(0, 0); break;
          case 1: setThresholds(2, 0); break;   // chase lights 2 strips per frame
          case 2: setThresholds(0, 2); break;
          case 3: setThresholds(2, 2); break;
          case 4: setThresholds(5, 0); break;
          case 5: setThresholds(0, 5); break;
          case 6: setThresholds(5, 5); break;
        }
      }
      if (chasing) currentFrame = (currentFrame + 1) % numFrames;
    }
  }
  else if (cmd >= 10000 && cmd <= 11023) {
    if (modeChanged) {
      int bitmask = cmd - 10000;
      portA = bitmask & 0x1F;
      portB = (bitmask >> 5) & 0x1F;
      doWrite = true;
      setThresholds(countActiveStrips(portA), countActiveStrips(portB));
    }
  }

  if (!doWrite) return;

  lastPortA = portA;
  lastPortB = portB;

  if (!mcpHealthy) {          // do the bookkeeping, skip the write (recovery owns retries)
    lastExecutedCommand = cmd;
    return;
  }

  if (!writePortsVerified(portA, portB)) {
    setMcpHealth(false);
    return;                   // do NOT advance activeCommand — we did not do it
  }

  lastExecutedCommand = cmd;

  if (activeCommand != cmd) { // publish 'state' only AFTER a verified write
    activeCommand = cmd;
    lastModeChangeMs = millis();
    publishState();
  }
}

// ========================================================
//  DISCREPANCY LOGIC
// ========================================================
// Spelling is load-bearing: the gateway maps these four strings to 1/0/2/3.
const char* getDiscrepancyState(bool expectedOn, float actual, float threshold) {
  if (expectedOn  && actual >= threshold) return "ON";
  if (!expectedOn && actual <  threshold) return "OFF";
  if (expectedOn  && actual <  threshold) return "FAIL_OPEN";
  return "FAIL_SHORT";
}

void SensorTask(void* parameter) {
  esp_task_wdt_add(NULL);
  NodeStateMsg st = {true, "OFF", "OFF", "OFF", "OFF", -1};
  float f1 = 0, f2 = 0;   // filtered STATIC currents; arrows are intent-echo, unmeasured
  float battFilt = 0; bool battInit = false; int battBandCur = -1;
  unsigned long lastRead = 0;

  bool pwrCandidate = true;
  int  pwrCandidateCount = 0;
  unsigned long seenModeChange = 0;

  for (;;) {
    esp_task_wdt_reset();

    if (millis() - lastRead >= 500) {
      lastRead = millis();
      bool changed = false;

      // ---- PSU: hysteresis band, then debounce ----
      float psuV = adcVolts(PIN_VOLT_PSU) * PSU_DIV_RATIO;
      bool rawPwr;
      if      (psuV <= PSU_FAIL_VOLTS) rawPwr = false;
      else if (psuV >= PSU_OK_VOLTS)   rawPwr = true;
      else                             rawPwr = st.power_ok;   // in the band: hold

      if (rawPwr != st.power_ok) {
        if (rawPwr == pwrCandidate) pwrCandidateCount++;
        else { pwrCandidate = rawPwr; pwrCandidateCount = 1; }

        if (pwrCandidateCount >= PWR_DEBOUNCE_COUNT) {
          st.power_ok = rawPwr;
          pwrCandidateCount = 0;
          changed = true;
          Serial.printf("[PWR] %s  (%.2f V)\n", rawPwr ? "OK" : "FAIL", psuV);
        }
      } else {
        pwrCandidateCount = 0;
      }

      // ---- BATTERY: heavy IIR on the calibrated voltage, then coarse band ----
      float bv = readBatteryVolts();
      if (!battInit) { battFilt = bv; battInit = true; }
      else           { battFilt = bv * BATT_ALPHA + battFilt * (1 - BATT_ALPHA); }
      int nb = batteryBand(battFilt, battBandCur);
      battBandCur = nb;
      if (nb != st.battery_pct) { st.battery_pct = nb; changed = true; }

      // ---- Static-zone currents (arrows are intent-echo, not measured) ----
      float c1 = readCurrent(PIN_CURR_STA1, ZERO_STA1, SENS_STA1);
      float c2 = readCurrent(PIN_CURR_STA2, ZERO_STA2, SENS_STA2);

      unsigned long mc = lastModeChangeMs;
      if (mc != seenModeChange) {           // snap past a step WE caused
        seenModeChange = mc;
        f1 = c1; f2 = c2;
      } else {
        f1 = (c1 < NOISE_FLOOR) ? 0 : (c1 * ALPHA) + (f1 * (1 - ALPHA));
        f2 = (c2 < NOISE_FLOOR) ? 0 : (c2 * ALPHA) + (f2 * (1 - ALPHA));
      }

      // Arrows: intent-echo (the 5A ACS712 can't resolve the ~2-strip chase
      // current). Only ON/OFF, never FAIL_*. Real arrow health -> Phase-3 test.
      const char* sl = expectLeftOn  ? "ON" : "OFF";
      const char* sr = expectRightOn ? "ON" : "OFF";
      if (strcmp(st.state_left, sl) || strcmp(st.state_right, sr)) {
        st.state_left = sl; st.state_right = sr;
        changed = true;
      }

      // Static zones ARE measured; verdict held while the load settles after a
      // mode change. Hardwired always-on: only ON and FAIL_OPEN are legal.
      if (millis() - mc >= MODE_SETTLE_MS) {
        const char* s1 = getDiscrepancyState(true, f1, THRESHOLD_STATIC);
        const char* s2 = getDiscrepancyState(true, f2, THRESHOLD_STATIC);
        if (strcmp(st.state_sta1, s1) || strcmp(st.state_sta2, s2)) {
          st.state_sta1 = s1; st.state_sta2 = s2;
          changed = true;
        }
      }

      if (!haveSensorData) { haveSensorData = true; changed = true; }
      if (changed) xQueueOverwrite(sensorQueue, &st);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ========================================================
//  SERIAL MENU — bench observation (h / i / s / b)
// ========================================================
void printMenu() {
  Serial.println(F("\n=============================================="));
  Serial.printf ("  SIGNAGE NODE v4.1.0-p2  —  register %d\n", ASSIGNED_REGISTER);
  Serial.println(F("=============================================="));
  Serial.println(F("  h -> this menu"));
  Serial.println(F("  i -> node info (net, MCP health, active mode, battery)"));
  Serial.println(F("  s -> toggle current-sensor STREAM (1 Hz)"));
  Serial.println(F("  b -> toggle BATTERY stream (1 Hz, raw + mV + Vbatt + %)"));
  Serial.println(F("==============================================\n"));
}

void readSensorsVerbose() {
  int rl = getMedianADC(PIN_CURR_LEFT), rr = getMedianADC(PIN_CURR_RGHT);
  int r1 = getMedianADC(PIN_CURR_STA1), r2 = getMedianADC(PIN_CURR_STA2);
  int rp = getMedianADC(PIN_VOLT_PSU);

  float vl = (rl / 4095.0f) * ADC_REF, vr = (rr / 4095.0f) * ADC_REF;
  float v1 = (r1 / 4095.0f) * ADC_REF, v2 = (r2 / 4095.0f) * ADC_REF;

  // Arrows: raw shown for observation, but STATE is the published intent-echo.
  Serial.println(F("\n  CH     rawADC   volts    amps    thresh   state"));
  Serial.printf("  LEFT   %5d   %.4f  %.4f  %.4f  %s (intent)\n", rl, vl,
                fabs((vl - ZERO_LEFT) / SENS_LEFT), dynamicThreshLeft,
                expectLeftOn ? "ON" : "OFF");
  Serial.printf("  RGHT   %5d   %.4f  %.4f  %.4f  %s (intent)\n", rr, vr,
                fabs((vr - ZERO_RGHT) / SENS_RGHT), dynamicThreshRight,
                expectRightOn ? "ON" : "OFF");
  Serial.printf("  STA1   %5d   %.4f  %.4f  %.4f  %s\n", r1, v1,
                fabs((v1 - ZERO_STA1) / SENS_STA1), THRESHOLD_STATIC,
                getDiscrepancyState(true, fabs((v1 - ZERO_STA1) / SENS_STA1), THRESHOLD_STATIC));
  Serial.printf("  STA2   %5d   %.4f  %.4f  %.4f  %s\n", r2, v2,
                fabs((v2 - ZERO_STA2) / SENS_STA2), THRESHOLD_STATIC,
                getDiscrepancyState(true, fabs((v2 - ZERO_STA2) / SENS_STA2), THRESHOLD_STATIC));
  Serial.printf("  PSU    %5d   %.4f  ->  %.2f V\n", rp, (rp / 4095.0f) * ADC_REF,
                (rp / 4095.0f) * ADC_REF * PSU_DIV_RATIO);
}

// Separate battery stream — during calibration the readings are taken on the PSU.
void readBatteryVerbose() {
  int raw = getMedianADC(PIN_VOLT_BATT);
  uint32_t mv = getMedianMv(PIN_VOLT_BATT);
  float vb = (mv / 1000.0f) * BATT_DIV_RATIO;
  Serial.printf("  BATT   rawADC=%4d   pin=%4u mV   Vbatt=%.3f V   ratio=%.3f   -> %d%%\n",
                raw, mv, vb, BATT_DIV_RATIO, batteryBand(vb, -1));
}

void printInfo() {
  Serial.println(F("\n---------------- NODE INFO ----------------"));
  Serial.printf("  Register      : %d\n", ASSIGNED_REGISTER);
  Serial.printf("  Ethernet      : %s\n", eth_connected ? ETH.localIP().toString().c_str() : "DOWN");
  Serial.printf("  MQTT          : %s (%s:%d)\n", client.connected() ? "connected" : "DISCONNECTED",
                mqtt_server_ip, mqtt_port);
  Serial.printf("  MCP23017      : %s\n", mcpHealthy ? "healthy" : "UNHEALTHY");
  Serial.printf("  Commanded     : %d\n", commandedValue);
  char act[24];
  if (!mcpHealthy) snprintf(act, sizeof(act), "UNKNOWN (65535)");
  else             snprintf(act, sizeof(act), "%d", activeCommand);
  Serial.printf("  Actually doing: %s\n", act);
  Serial.printf("  Ports  A=0x%02X  B=0x%02X\n", lastPortA, lastPortB);
  if (networkState.battery_pct < 0)
    Serial.printf("  Battery       : --- (%.3f V, warming up)\n", readBatteryVolts());
  else
    Serial.printf("  Battery       : %d%% (%.3f V)\n", networkState.battery_pct, readBatteryVolts());
  Serial.println(F("-------------------------------------------"));
}

bool sensorStream = false;
bool battStream   = false;
unsigned long lastStream = 0;
unsigned long lastBattStream = 0;

void handleSerial() {
  if (!Serial.available()) return;
  char c = Serial.read();
  while (Serial.available() && (Serial.peek() == '\n' || Serial.peek() == '\r')) Serial.read();

  switch (c) {
    case 'h': printMenu();          break;
    case 'i': printInfo();          break;
    case 's': sensorStream = !sensorStream;
              Serial.printf(">> Sensor stream %s\n", sensorStream ? "ON" : "OFF"); break;
    case 'b': battStream = !battStream;
              Serial.printf(">> Battery stream %s\n", battStream ? "ON" : "OFF"); break;
    default: break;
  }
}

// ========================================================
//  SETUP & LOOP
// ========================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(PIN_VOLT_PSU, INPUT);  pinMode(PIN_VOLT_BATT, INPUT);
  pinMode(PIN_CURR_LEFT, INPUT); pinMode(PIN_CURR_RGHT, INPUT);
  pinMode(PIN_CURR_STA1, INPUT); pinMode(PIN_CURR_STA2, INPUT);

  snprintf(topic_cmd,    sizeof(topic_cmd),    "metro/signage/register/%d/value",       ASSIGNED_REGISTER);
  snprintf(topic_status, sizeof(topic_status), "metro/signage/register/%d/status",      ASSIGNED_REGISTER);
  snprintf(topic_power,  sizeof(topic_power),  "metro/signage/register/%d/power",       ASSIGNED_REGISTER);
  snprintf(topic_state,  sizeof(topic_state),  "metro/signage/register/%d/state",       ASSIGNED_REGISTER);
  snprintf(topic_curr1,  sizeof(topic_curr1),  "metro/signage/register/%d/current1",    ASSIGNED_REGISTER);
  snprintf(topic_curr2,  sizeof(topic_curr2),  "metro/signage/register/%d/current2",    ASSIGNED_REGISTER);
  snprintf(topic_curr3,  sizeof(topic_curr3),  "metro/signage/register/%d/current3",    ASSIGNED_REGISTER);
  snprintf(topic_curr4,  sizeof(topic_curr4),  "metro/signage/register/%d/current4",    ASSIGNED_REGISTER);
  snprintf(topic_batt,   sizeof(topic_batt),   "metro/signage/register/%d/battery_pct", ASSIGNED_REGISTER);

  /*
   * NETWORK FIRST — before the MCP23017, so an I2C fault never blocks the PHY.
   */
  WiFi.onEvent(eth_event_handler);
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
  ETH.config(local_IP, gateway_ip, subnet, primaryDNS);
  client.setServer(mqtt_server_ip, mqtt_port);
  client.setCallback(callback);

  /*
   * BOUND THE BLOCKING (watchdog safety).
   *   - setSocketTimeout(2): caps how long client.loop()/connect() wait on the
   *     MQTT byte stream.
   *   - ethClient.setConnectionTimeout(5000): caps the underlying TCP connect,
   *     which setSocketTimeout does NOT govern — an unreachable broker would
   *     otherwise let client.connect() stall on the TCP handshake for ~30 s.
   * Both sit well under the 15 s task WDT, so a network flap self-clears instead
   * of tripping a reboot.
   */
  client.setSocketTimeout(2);
  ethClient.setConnectionTimeout(5000);

  // MCP second, non-fatal.
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.print(F("\nProbing MCP23017 @0x20 on SDA=4/SCL=13 ... "));
  if (mcp.begin_I2C(MCP_ADDR, &Wire)) {
    for (int i = 0; i < 16; i++) mcp.pinMode(i, OUTPUT);
    mcpHealthy = writePortsVerified(0x00, 0x00);
    Serial.println(mcpHealthy ? F("OK") : F("FOUND BUT READBACK FAILED"));
  } else {
    Serial.println(F("NOT FOUND — continuing anyway, will retry in background."));
    Serial.println(F("Check: SDA=GPIO4, SCL=GPIO13, 4.7k pull-ups, RESET->3V3, A0/A1/A2->GND."));
  }

  sensorQueue = xQueueCreate(1, sizeof(NodeStateMsg));

  /*
   * TASK WATCHDOG — reconfigure, do NOT re-init.
   *
   * arduino-esp32 3.x starts the TWDT during core init, so esp_task_wdt_init()
   * returns ESP_ERR_INVALID_STATE ("TWDT already initialized") and our 15 s
   * timeout never applies — leaving the short default, which a blocking network
   * call trips (the loopTask WDT reboot loop). Reconfigure the existing TWDT so
   * 15 s actually takes effect. idle_core_mask = 0: watch only the tasks we add
   * (loopTask + SensorTask), not the idle tasks, so a momentarily-busy core can't
   * cause a spurious panic.
   */
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 15000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  if (esp_task_wdt_init(&wdt_config) == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_reconfigure(&wdt_config);
  }
  esp_task_wdt_add(NULL);

  xTaskCreatePinnedToCore(SensorTask, "SensorTask", 10000, NULL, 1, NULL, 0);

  printMenu();
}

void loop() {
  esp_task_wdt_reset();

  handleSerial();
  ensureMcpHealthy();

  // Unconditional: keep animating through a broker/link outage.
  runAnimationStateMachine();

  if (sensorStream && millis() - lastStream >= 1000) {
    lastStream = millis();
    readSensorsVerbose();
  }
  if (battStream && millis() - lastBattStream >= 1000) {
    lastBattStream = millis();
    readBatteryVerbose();
  }

  if (eth_connected) {
    if (!client.connected()) {
      static unsigned long lastReconnect = 0;
      if (millis() - lastReconnect > 5000) {
        lastReconnect = millis();
        char clientId[40];
        snprintf(clientId, sizeof(clientId), "ESP32-%s", ETH.macAddress().c_str());
        // LWT: retained OFFLINE on .../status, QoS 1 — lets the gateway scrub a
        // node that drops without a clean disconnect.
        if (client.connect(clientId, topic_status, 1, true, "OFFLINE")) {
          client.subscribe(topic_cmd, 1);
          client.subscribe(topic_scan, 0);
          publish_state_msg(networkState);
        }
      }
    } else {
      client.loop();
      NodeStateMsg tmp;
      if (xQueueReceive(sensorQueue, &tmp, 0) == pdTRUE) {
        networkState = tmp;
        publish_state_msg(networkState);
      }
    }
  }
}
