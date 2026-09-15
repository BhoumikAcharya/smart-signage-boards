/*
 * Battery Calibration — bench tool  (Phase 1 · Part 2 groundwork)
 * ==============================================================
 * Target     : PCB V3, ESP32 battery sense on GPIO39 via a 100k/18k divider,
 *              MCP23017 @0x20 (SDA=4/SCL=13) driving the arrow MOSFETs.
 * Standalone : serial only. No Ethernet, no MQTT — a bench instrument for
 *              characterising the LFP battery.
 *
 * WHAT IT IS FOR
 *   1. Watch the ADC / battery voltage in different situations — charging, at
 *      rest, under load, and on discharge.
 *   2. Calibrate the divider ratio to THIS board ('d').
 *   3. Drive the REAL operating load (a one-side arrow chase) so a discharge
 *      test drains the battery exactly as the field does, and LOG the discharge
 *      to CSV over time ('c').
 *   4. Capture the 0/50/100% threshold voltages ('e'/'m'/'f') to drop into the
 *      Part-2 firmware's percentage map.
 *
 * OPERATING LOAD: in the field a node normally shows a single-side chase (LHS or
 * RHS), so the battery load is: the two STATIC zones (hardwired always-on — they
 * draw as soon as the board is powered, this sketch does not switch them) PLUS a
 * 2-strip arrow chase on one side. This tool reproduces that: pick '1' (left) or
 * '2' (right); the static zones are already on. Run the drain in that state so
 * the captured anchors match the load the firmware reads at.
 *
 * BATTERY: 4S LiFePO4, 12.8 V nominal, 6 Ah. The discharge curve is FLAT through
 * the middle, so voltage->% is coarse — a two-segment map anchored on three
 * measured voltages (0/50/100%) is about as good as voltage alone gets. Capture
 * ON BATTERY (PSU disconnected — with the charger on you read the charger, not
 * the pack), UNDER this load, and let each point settle before stamping. The 50%
 * anchor cannot be eyeballed (flat curve) — mark it at half the total runtime
 * (or half the 6 Ah if you are measuring discharge current).
 *
 * VOLTAGE READ: analogReadMilliVolts() applies the ESP32's factory eFuse ADC
 * calibration internally — far better than raw/4095*3.3, which matters because
 * the useful LFP signal is only tens of mV.
 *
 * DIVIDER: 100k (battery+ -> pin) / 18k (pin -> GND).
 *   Vadc = Vbatt * 18/118 = Vbatt / 6.556.  At 14.6 V -> 2.23 V (safe, linear).
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>

// ---- Hardware ----
const int PIN_VOLT_BATT = 39;          // ADC1_CH3 — the old battery sense line
#define I2C_SDA  4
#define I2C_SCL  13
#define MCP_ADDR 0x20
Adafruit_MCP23X17 mcp;
bool mcpOk = false;

// ---- Operating-load chase (matches Firmware.ino: 2-LED wrapping, 250 ms) ----
const uint8_t chaseFrames[5] = {0x18, 0x0C, 0x06, 0x03, 0x11};
const int     numFrames = 5;
const unsigned long FRAME_MS = 250;
uint8_t loadMode = 0;                   // 0 = off, 1 = left chase, 2 = right chase

// ---- Calibration state (mutable: this tool exists to derive these) ----
float BATT_DIV_RATIO = 6.556f;         // (100+18)/18 nominal; refine with 'd'

float V0   = 0.0f;                     // battery volts at 0%   (empty)
float V50  = 0.0f;                     // battery volts at 50%
float V100 = 0.0f;                     // battery volts at 100% (full)
bool  haveV0 = false, haveV50 = false, haveV100 = false;

// ---- CSV discharge log ----
const unsigned long CSV_INTERVAL_MS = 30000;   // one row per 30 s (edit to taste)
bool csvLog = false;
unsigned long csvStartMs = 0;
unsigned long lastCsvMs  = 0;

// ---- 1 Hz human-readable stream ----
bool stream = false;
unsigned long lastStream = 0;

// ========================================================
//  OPERATING LOAD  (non-blocking arrow chase)
// ========================================================
void runChase() {
  static unsigned long prev = 0;
  static int frame = 0;
  if (!mcpOk || loadMode == 0) return;
  if (millis() - prev < FRAME_MS) return;
  prev = millis();
  uint8_t f = chaseFrames[frame];
  if (loadMode == 1) { mcp.writeGPIOA(f);    mcp.writeGPIOB(0x00); }
  else               { mcp.writeGPIOA(0x00); mcp.writeGPIOB(f);    }
  frame = (frame + 1) % numFrames;
}

void setLoad(uint8_t m) {
  loadMode = m;
  if (!mcpOk) { Serial.println(F(">> LOAD change ignored — MCP not found")); return; }
  if (m == 0) { mcp.writeGPIOA(0x00); mcp.writeGPIOB(0x00); Serial.println(F(">> LOAD off")); }
  else if (m == 1) Serial.println(F(">> LOAD: LEFT-side chase (operating load)"));
  else             Serial.println(F(">> LOAD: RIGHT-side chase (operating load)"));
}

// ========================================================
//  SAMPLING
// ========================================================
int getMedianRaw(int pin) {
  int s[51];
  for (int i = 0; i < 51; i++) s[i] = analogRead(pin);
  for (int i = 1; i < 51; i++) {
    int key = s[i], j = i - 1;
    while (j >= 0 && s[j] > key) { s[j + 1] = s[j]; j--; }
    s[j + 1] = key;
  }
  return s[25];
}

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

float readBatteryVolts() {
  return (getMedianMv(PIN_VOLT_BATT) / 1000.0f) * BATT_DIV_RATIO;
}

// ========================================================
//  TWO-SEGMENT PERCENTAGE MAP (0 / 50 / 100 anchors)
// ========================================================
// Two straight segments — 0..50 between V0 and V50, 50..100 between V50 and V100
// — so the 50% point can bend the map to fit LFP's flat middle. Returns -1 until
// all three anchors are set and increasing.
float batteryPercent(float v) {
  if (!(haveV0 && haveV50 && haveV100)) return -1.0f;
  if (!(V0 < V50 && V50 < V100))        return -1.0f;
  if (v >= V100) return 100.0f;
  if (v <= V0)   return 0.0f;
  if (v >= V50)  return 50.0f + 50.0f * (v - V50) / (V100 - V50);
  return 50.0f * (v - V0) / (V50 - V0);
}

// ========================================================
//  SERIAL
// ========================================================
// Blocking line read — services the chase so the load keeps animating while it
// waits. Standalone tool, no watchdog to feed.
bool readLine(char* buf, size_t len, unsigned long timeoutMs) {
  size_t i = 0; unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    runChase();
    while (Serial.available()) {
      char ch = Serial.read();
      if (ch == '\n' || ch == '\r') { if (i > 0) { buf[i] = '\0'; return true; } }
      else if (i < len - 1) buf[i++] = ch;
    }
    delay(2);
  }
  buf[0] = '\0'; return false;
}

void printMenu() {
  Serial.println(F("\n=============================================="));
  Serial.println(F("  BATTERY CALIBRATION  —  GPIO39, 100k/18k"));
  Serial.println(F("=============================================="));
  Serial.println(F("  h -> this menu"));
  Serial.println(F("  r -> read once (raw ADC, pin mV, Vbatt, %)"));
  Serial.println(F("  s -> toggle 1 Hz human-readable stream"));
  Serial.println(F("  c -> toggle CSV discharge log (30 s rows)"));
  Serial.println(F("  1 -> LOAD: left-side chase  (operating load)"));
  Serial.println(F("  2 -> LOAD: right-side chase (operating load)"));
  Serial.println(F("  0 -> LOAD: off"));
  Serial.println(F("  d -> calibrate divider ratio (enter multimeter Vbatt)"));
  Serial.println(F("  e -> mark CURRENT voltage as 0%   (empty)"));
  Serial.println(F("  m -> mark CURRENT voltage as 50%  (mid)"));
  Serial.println(F("  f -> mark CURRENT voltage as 100% (full)"));
  Serial.println(F("  p -> print paste-ready block"));
  Serial.println(F("==============================================\n"));
}

void readOnce() {
  int raw = getMedianRaw(PIN_VOLT_BATT);
  uint32_t mv = getMedianMv(PIN_VOLT_BATT);
  float vbatt = (mv / 1000.0f) * BATT_DIV_RATIO;
  float pct = batteryPercent(vbatt);
  Serial.printf("  rawADC=%4d   pin=%4u mV   Vbatt=%.3f V   ratio=%.4f   ",
                raw, mv, vbatt, BATT_DIV_RATIO);
  if (pct < 0) Serial.println(F("%=--- (set e/m/f)"));
  else         Serial.printf("%%=%.0f\n", pct);
}

void printCsvRow() {
  unsigned long elapsed = (millis() - csvStartMs) / 1000;
  int raw = getMedianRaw(PIN_VOLT_BATT);
  uint32_t mv = getMedianMv(PIN_VOLT_BATT);
  float vbatt = (mv / 1000.0f) * BATT_DIV_RATIO;
  float pct = batteryPercent(vbatt);
  if (pct < 0) Serial.printf("%lu,%d,%u,%.3f,\n",   elapsed, raw, mv, vbatt);
  else         Serial.printf("%lu,%d,%u,%.3f,%.0f\n", elapsed, raw, mv, vbatt, pct);
}

void calibrateDivider() {
  char buf[24];
  Serial.print(F("\n[CAL] Measure the battery with a multimeter, type the volts: "));
  if (!readLine(buf, sizeof(buf), 30000) || !buf[0]) { Serial.println(F("\n[CAL] Skipped.")); return; }
  float vtrue = atof(buf);
  uint32_t mv = getMedianMv(PIN_VOLT_BATT);
  float vadc = mv / 1000.0f;
  if (vtrue > 0 && vadc > 0.01f) {
    BATT_DIV_RATIO = vtrue / vadc;
    Serial.printf("\n[CAL] pin=%.4f V, true=%.3f V  ->  BATT_DIV_RATIO = %.4f\n",
                  vadc, vtrue, BATT_DIV_RATIO);
  } else Serial.println(F("\n[CAL] Bad input or no signal — unchanged."));
}

void markAnchor(char which) {
  float v = readBatteryVolts();
  const char* name;
  switch (which) {
    case 'e': V0   = v; haveV0   = true; name = "0%   (V0)";   break;
    case 'm': V50  = v; haveV50  = true; name = "50%  (V50)";  break;
    case 'f': V100 = v; haveV100 = true; name = "100% (V100)"; break;
    default: return;
  }
  Serial.printf("[CAL] Marked %s = %.3f V\n", name, v);
  if (haveV0 && haveV50 && haveV100 && !(V0 < V50 && V50 < V100))
    Serial.println(F("[CAL] WARNING: anchors not increasing (need V0 < V50 < V100) — re-capture."));
}

void printBlock() {
  Serial.println(F("\n// ---- PASTE INTO PART-2 FIRMWARE ----"));
  Serial.printf("const float BATT_DIV_RATIO = %.4ff;\n", BATT_DIV_RATIO);
  Serial.printf("const float BATT_V0   = %.3ff;   // 0%%\n",   V0);
  Serial.printf("const float BATT_V50  = %.3ff;   // 50%%\n",  V50);
  Serial.printf("const float BATT_V100 = %.3ff;   // 100%%\n", V100);
  Serial.println(F("// ------------------------------------"));
  if (!(haveV0 && haveV50 && haveV100))
    Serial.println(F("// NOTE: not all anchors captured yet (e / m / f)."));
}

void handleSerial() {
  if (!Serial.available()) return;
  char c = Serial.read();
  while (Serial.available() && (Serial.peek() == '\n' || Serial.peek() == '\r')) Serial.read();
  switch (c) {
    case 'h': printMenu();        break;
    case 'r': readOnce();         break;
    case 's': stream = !stream;
              if (stream) csvLog = false;      // one output format at a time
              Serial.printf(">> Stream %s\n", stream ? "ON" : "OFF"); break;
    case 'c': csvLog = !csvLog;
              if (csvLog) {
                stream = false;
                csvStartMs = millis();
                lastCsvMs  = millis() - CSV_INTERVAL_MS;   // emit first row now
                Serial.println(F(">> CSV log ON (start this when you cut the PSU)"));
                Serial.println(F("elapsed_s,rawADC,pin_mV,Vbatt,pct"));
              } else Serial.println(F(">> CSV log OFF"));
              break;
    case '1': setLoad(1);         break;
    case '2': setLoad(2);         break;
    case '0': setLoad(0);         break;
    case 'd': calibrateDivider(); break;
    case 'e': case 'm': case 'f': markAnchor(c); break;
    case 'p': printBlock();       break;
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
  pinMode(PIN_VOLT_BATT, INPUT);

  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.print(F("\nProbing MCP23017 @0x20 (SDA=4/SCL=13) ... "));
  if (mcp.begin_I2C(MCP_ADDR, &Wire)) {
    for (int i = 0; i < 16; i++) mcp.pinMode(i, OUTPUT);
    mcp.writeGPIOA(0x00); mcp.writeGPIOB(0x00);
    mcpOk = true;
    Serial.println(F("OK (load control ready)."));
  } else {
    mcpOk = false;
    Serial.println(F("NOT FOUND — voltage logging works, load control disabled."));
  }

  Serial.println(F("Battery calibration tool ready (GPIO39, 100k/18k divider)."));
  printMenu();
}

void loop() {
  handleSerial();
  runChase();

  if (stream && millis() - lastStream >= 1000) {
    lastStream = millis();
    readOnce();
  }
  if (csvLog && millis() - lastCsvMs >= CSV_INTERVAL_MS) {
    lastCsvMs = millis();
    printCsvRow();
  }
}
