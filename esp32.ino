#include <ETH.h>
#include <PubSubClient.h>
#include <WiFi.h>

// --- USER CONFIGURATION ----------------------------------------
const char* mqtt_server_ip = "192.168.1.10"; // Raspberry Pi IP
const int mqtt_port = 1883;

// Unique Register Assignment (CHANGE THIS FOR EACH UNIT)
const int ASSIGNED_REGISTER = 40007;

// Static IP Settings (CHANGE THIS FOR EACH UNIT)
IPAddress local_IP(192, 168, 1, 107); // Unique Static IP
IPAddress gateway(192, 168, 1, 1);    // Router/Gateway IP
IPAddress subnet(255, 255, 255, 0);   // Subnet Mask
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// --- CURRENT SENSOR CALIBRATION (FROM TESTED CODE) ---
const float ADC_REF_VOLT = 3.3;

// Sensitivity Settings (185mV/A for ACS712-05B, 100mV/A for 20A, 66mV/A for 30A)
const float SENSITIVITY_1 = 0.162; // Sensitivity for Sensor 1
const float SENSITIVITY_2 = 0.142; // Sensitivity for Sensor 2

const float ZERO_VOLT_1  = 2.515; // Calibrated Zero for Sensor 1
const float ZERO_VOLT_2  = 2.520; // Calibrated Zero for Sensor 2
const float ALPHA        = 0.15;  // Low Pass Filter Alpha

// Current Thresholds (in Amps)
const float CURRENT_THRESHOLD_1 = 0.100;
const float CURRENT_THRESHOLD_2 = 0.120;

// ---------------------------------------------------------------

// GPIO Pins
const int RELAY_1_PIN = 4;
const int RELAY_2_PIN = 13;
const int POWER_MONITOR_PIN = 34; // Input-only pin for Voltage
const int CURRENT_PIN_1 = 35;     // ACS712 Sensor 1
const int CURRENT_PIN_2 = 36;     // ACS712 Sensor 2

// Ethernet PHY Configuration (LAN8720)
#define ETH_PHY_ADDR    1
#define ETH_PHY_POWER  -1
#define ETH_PHY_MDC     23
#define ETH_PHY_MDIO    18
#define ETH_PHY_TYPE    ETH_PHY_LAN8720
#define ETH_CLK_MODE    ETH_CLOCK_GPIO17_OUT

// MQTT Topics
char control_topic[50];
char status_topic[50];
char power_topic[50];
char current1_topic[50];
char current2_topic[50];
const char* scan_topic = "metro/signage/scan"; // Broadcast topic

WiFiClient ethClient;
PubSubClient client(ethClient);
long lastReconnectAttempt = 0;
static bool eth_connected = false;

// Power Monitoring Variables
const int POWER_THRESHOLD = 3300;
const unsigned long DEBOUNCE_DELAY = 50; 
bool pwr_stableState = false;
bool pwr_lastReading = false;
unsigned long pwr_lastDebounceTime = 0;

// Current Monitoring Variables
float filteredCurrent1 = 0.0;
float filteredCurrent2 = 0.0;
String lastCurrentState1 = "---";
String lastCurrentState2 = "---";
unsigned long lastCurrentReadTime = 0;
const long CURRENT_READ_INTERVAL = 500; // Read every 500ms

void eth_event_handler(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      ETH.setHostname("esp32-signage");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      Serial.println("Mbps");
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}

// Helper to publish all statuses (Used on Connect and on PING)
void publish_all_status() {
  // 1. Publish ONLINE status with IP
  String onlineMsg = "ONLINE:" + ETH.localIP().toString();
  client.publish(status_topic, onlineMsg.c_str(), true);
  
  // 2. Publish Power Status
  int raw = analogRead(POWER_MONITOR_PIN);
  pwr_stableState = (raw > POWER_THRESHOLD);
  String pwrMsg = pwr_stableState ? "OK" : "FAIL";
  client.publish(power_topic, pwrMsg.c_str(), true);

  // 3. Publish Current Status (Force send current state)
  client.publish(current1_topic, lastCurrentState1.c_str(), true);
  client.publish(current2_topic, lastCurrentState2.c_str(), true);

  Serial.println(">>> Reported Full Status");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  // --- Check for Broadcast PING ---
  if (String(topic) == scan_topic && message == "PING") {
    Serial.println("\n[PING] Received Broadcast Ping.");
    publish_all_status();
    return;
  }

  // --- Relay Control Logic ---
  int control_value = message.toInt();
  Serial.printf("Received Relay Command: %d\n", control_value);

  switch (control_value) {
    case 0:
      digitalWrite(RELAY_1_PIN, HIGH); digitalWrite(RELAY_2_PIN, HIGH); break;
    case 1:
      digitalWrite(RELAY_1_PIN, LOW); digitalWrite(RELAY_2_PIN, HIGH); break;
    case 2:
      digitalWrite(RELAY_1_PIN, HIGH); digitalWrite(RELAY_2_PIN, LOW); break;
    case 3:
      digitalWrite(RELAY_1_PIN, LOW); digitalWrite(RELAY_2_PIN, LOW); break;
    default:
      digitalWrite(RELAY_1_PIN, HIGH); digitalWrite(RELAY_2_PIN, HIGH); break;
  }
}

// --- CURRENT SENSOR LOGIC (Merged from Tested Code) ---
void checkCurrentSensors() {
  if (millis() - lastCurrentReadTime < CURRENT_READ_INTERVAL) return;
  lastCurrentReadTime = millis();

  // --- SENSOR 1 ---
  long totalADC1 = 0;
  for (int i = 0; i < 150; i++) {
    totalADC1 += analogRead(CURRENT_PIN_1);
  }
  float avgADC1 = totalADC1 / 150.0;
  float voltage1 = (avgADC1 / 4095.0) * ADC_REF_VOLT;
  // Use SENSITIVITY_1
  float rawCurrent1 = (voltage1 - ZERO_VOLT_1) / SENSITIVITY_1;
  
  if (abs(rawCurrent1) < 0.06) rawCurrent1 = 0; // Deadzone
  filteredCurrent1 = abs((rawCurrent1 * ALPHA) + (filteredCurrent1 * (1 - ALPHA)));

  // Determine State 1
  String currentState1 = (filteredCurrent1 > CURRENT_THRESHOLD_1) ? "OK" : "FAIL";

  // Publish if changed
  if (currentState1 != lastCurrentState1) {
    lastCurrentState1 = currentState1;
    client.publish(current1_topic, currentState1.c_str(), true);
    // Serial.printf("S1: %.2fA -> %s\n", filteredCurrent1, currentState1.c_str());
  }

  // --- SENSOR 2 ---
  long totalADC2 = 0;
  for (int i = 0; i < 150; i++) {
    totalADC2 += analogRead(CURRENT_PIN_2);
  }
  float avgADC2 = totalADC2 / 150.0;
  float voltage2 = (avgADC2 / 4095.0) * ADC_REF_VOLT;
  // Use SENSITIVITY_2
  float rawCurrent2 = (voltage2 - ZERO_VOLT_2) / SENSITIVITY_2;
  
  if (abs(rawCurrent2) < 0.06) rawCurrent2 = 0; // Deadzone
  filteredCurrent2 = abs((rawCurrent2 * ALPHA) + (filteredCurrent2 * (1 - ALPHA)));

  // Determine State 2
  String currentState2 = (filteredCurrent2 > CURRENT_THRESHOLD_2) ? "OK" : "FAIL";

  // Publish if changed
  if (currentState2 != lastCurrentState2) {
    lastCurrentState2 = currentState2;
    client.publish(current2_topic, currentState2.c_str(), true);
    // Serial.printf("S2: %.2fA -> %s\n", filteredCurrent2, currentState2.c_str());
  }

  // --- LIVE DEBUGGING FOR TESTING PURPOSES ---
  Serial.printf("[TEST] S1: %.3fV / %.3fA  |  S2: %.3fV / %.3fA\n", voltage1, filteredCurrent1, voltage2, filteredCurrent2);
}

void checkPowerMonitor() {
  int analogValue = analogRead(POWER_MONITOR_PIN);
  bool currentReading = (analogValue > POWER_THRESHOLD);

  if (currentReading != pwr_lastReading) {
    pwr_lastDebounceTime = millis();
  }
  pwr_lastReading = currentReading;

  if ((millis() - pwr_lastDebounceTime) > DEBOUNCE_DELAY) {
    if (currentReading != pwr_stableState) {
      pwr_stableState = currentReading;
      String payload = pwr_stableState ? "OK" : "FAIL";
      client.publish(power_topic, payload.c_str(), true);
      Serial.printf(">> Power Changed: %s\n", payload.c_str());
    }
  }

  //For Testing the PSU Failure Code!!!!!
  //Serial.println(analogValue);

}

/*
 * Author:
  * Date:
 * Description:
 * Called from function:
 * 

*/


boolean mqtt_connect() {
  if (!eth_connected) return false;
  
  Serial.print("Attempting MQTT connection...");
  String clientId = "ESP32EthClient-";
  clientId += ETH.macAddress();

  if (client.connect(clientId.c_str(), status_topic, 1, true, "OFFLINE")) {
    Serial.println("Connected!");
    client.subscribe(control_topic);
    client.subscribe(scan_topic);
    publish_all_status();
  } else {
    Serial.printf("failed, rc=%d try again in 5s\n", client.state());
  }
  return client.connected();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Hardware Setup
  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);
  digitalWrite(RELAY_1_PIN, HIGH);
  digitalWrite(RELAY_2_PIN, HIGH);
  
  pinMode(POWER_MONITOR_PIN, INPUT);
  pinMode(CURRENT_PIN_1, INPUT);
  pinMode(CURRENT_PIN_2, INPUT);

  // ADC Settings
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // String Setup
  sprintf(control_topic, "metro/signage/register/%d/value", ASSIGNED_REGISTER);
  sprintf(status_topic, "metro/signage/register/%d/status", ASSIGNED_REGISTER);
  sprintf(power_topic, "metro/signage/register/%d/power", ASSIGNED_REGISTER);
  sprintf(current1_topic, "metro/signage/register/%d/current1", ASSIGNED_REGISTER);
  sprintf(current2_topic, "metro/signage/register/%d/current2", ASSIGNED_REGISTER);

  Serial.printf("\n--- ESP32 Ethernet Signage Controller (%d) ---\n", ASSIGNED_REGISTER);
  Serial.printf("Calibrated Zero Points: S1=%.3fV, S2=%.3fV\n", ZERO_VOLT_1, ZERO_VOLT_2);

  // Ethernet Setup
  WiFi.onEvent(eth_event_handler);
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
  ETH.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);

  // MQTT Setup
  client.setServer(mqtt_server_ip, mqtt_port);
  client.setCallback(callback);
  client.setKeepAlive(4);
}

void loop() {
  if (!eth_connected) {
      lastReconnectAttempt = 0;
      delay(1000);
      return;
  }
  
  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (mqtt_connect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop();
    checkPowerMonitor();
    checkCurrentSensors();
  }
}
