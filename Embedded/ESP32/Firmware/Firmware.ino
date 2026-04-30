#include <ETH.h>
#include <PubSubClient.h>
#include <WiFi.h>

// --- USER CONFIGURATION ----------------------------------------
const char* mqtt_server_ip = "192.168.1.10"; // Raspberry Pi IP
const int mqtt_port = 1883;

// Unique Register Assignment (CHANGE THIS FOR EACH UNIT)
const int ASSIGNED_REGISTER = 40001;

// Static IP Settings (CHANGE THIS FOR EACH UNIT)
IPAddress local_IP(192, 168, 1, 101); 
IPAddress gateway(192, 168, 1, 1);    
IPAddress subnet(255, 255, 255, 0);   
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// --- CURRENT SENSOR CALIBRATION ---
const float ADC_REF_VOLT = 3.3;
const float SENSITIVITY_1 = 0.173; 
const float SENSITIVITY_2 = 0.173; 
const float ZERO_VOLT_1  = 2.36; 
const float ZERO_VOLT_2  = 2.36; 
const float ALPHA        = 0.15;  

const float CURRENT_THRESHOLD_1 = 0.100;
const float CURRENT_THRESHOLD_2 = 0.120;

// --- BATTERY CALIBRATION ---
const float R1 = 4200.0;  // Updated to 4.2k Ohms
const float R2 = 1000.0;  // Updated to 1k Ohms
const float ADC_RESOLUTION = 4095.0; 
const float K_CALIBRATION = 1.057; // Accurately calculated from 13.13V / 12.42V

// GPIO Pins
const int RELAY_1_PIN = 4;
const int RELAY_2_PIN = 13;
const int POWER_MONITOR_PIN = 34; 
const int CURRENT_PIN_1 = 35;     
const int CURRENT_PIN_2 = 36;     
const int BATTERY_PIN = 32;       // NEW: Battery Pin

// Ethernet PHY Configuration (Standard Arduino IDE)
#define ETH_PHY_ADDR  1
#define ETH_PHY_POWER -1
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_CLK_MODE  ETH_CLOCK_GPIO17_OUT

// MQTT Topics
char control_topic[50];
char status_topic[50];
char power_topic[50];
char current1_topic[50];
char current2_topic[50];
char batt_pct_topic[50];          // NEW: Battery Topic
const char* scan_topic = "metro/signage/scan"; 

WiFiClient ethClient;
PubSubClient client(ethClient);
long lastReconnectAttempt = 0;
static bool eth_connected = false;

// Power Monitoring Variables
const int POWER_THRESHOLD = 1800;  
const unsigned long DEBOUNCE_DELAY = 50; 
bool pwr_stableState = false;
bool pwr_lastReading = false;
unsigned long pwr_lastDebounceTime = 0;

// Current Monitoring Variables 
float filteredCurrent1 = 0.0;
float filteredCurrent2 = 0.0;
const char* lastCurrentState1 = "---";
const char* lastCurrentState2 = "---";
unsigned long lastCurrentReadTime = 0;
const long CURRENT_READ_INTERVAL = 500; 

// Relay & Battery Monitoring Variables
int currentRelayState = 0;        // 0=OFF, 1/2=One LED, 3=Both LEDs
int lastBattPct = -1; 
unsigned long lastBattReadTime = 0;
const long BATT_READ_INTERVAL = 5000; // Check battery every 5 seconds

// Updated Event Handler for modern Arduino IDE compatibility
void eth_event_handler(arduino_event_id_t event) {
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

// Helper to publish all statuses
void publish_all_status() {
  char onlineMsg[30];
  snprintf(onlineMsg, sizeof(onlineMsg), "ONLINE:%s", ETH.localIP().toString().c_str());
  client.publish(status_topic, onlineMsg, true);
  
  int raw = analogRead(POWER_MONITOR_PIN);
  pwr_stableState = (raw > POWER_THRESHOLD);
  const char* pwrMsg = pwr_stableState ? "OK" : "FAIL"; 
  client.publish(power_topic, pwrMsg, true);

  client.publish(current1_topic, lastCurrentState1, true);
  client.publish(current2_topic, lastCurrentState2, true);

  // Publish Battery
  if (lastBattPct != -1) {
    char pctStr[8];
    snprintf(pctStr, sizeof(pctStr), "%d", lastBattPct);
    client.publish(batt_pct_topic, pctStr, true);
  }

  Serial.println(">>> Reported Full Status");
}

void callback(char* topic, byte* payload, unsigned int length) {
  char msgBuffer[16]; 
  
  unsigned int copyLength = (length < sizeof(msgBuffer) - 1) ? length : (sizeof(msgBuffer) - 1);
  
  memcpy(msgBuffer, payload, copyLength);
  msgBuffer[copyLength] = '\0'; 

  if (strcmp(topic, scan_topic) == 0 && strcmp(msgBuffer, "PING") == 0) {
    Serial.println("\n[PING] Received Broadcast Ping.");
    publish_all_status();
    return;
  }

  // --- Relay Control Logic ---
  int control_value = atoi(msgBuffer); 
  currentRelayState = control_value; // Store state globally for battery compensation math
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
  float rawCurrent1 = (voltage1 - ZERO_VOLT_1) / SENSITIVITY_1;
  
  if (abs(rawCurrent1) < 0.06) rawCurrent1 = 0; 
  filteredCurrent1 = abs((rawCurrent1 * ALPHA) + (filteredCurrent1 * (1 - ALPHA)));

  const char* currentState1 = (filteredCurrent1 > CURRENT_THRESHOLD_1) ? "OK" : "FAIL";

  if (strcmp(currentState1, lastCurrentState1) != 0) {
    lastCurrentState1 = currentState1;
    client.publish(current1_topic, currentState1, true);
  }
  //-----tesitng code remove later -----
  Serial.println("Voltage CS1: ");
  Serial.println(voltage1);

  // --- SENSOR 2 ---
  long totalADC2 = 0;
  for (int i = 0; i < 150; i++) {
    totalADC2 += analogRead(CURRENT_PIN_2);
  }
  float avgADC2 = totalADC2 / 150.0;
  float voltage2 = (avgADC2 / 4095.0) * ADC_REF_VOLT;
  float rawCurrent2 = (voltage2 - ZERO_VOLT_2) / SENSITIVITY_2;
  
  if (abs(rawCurrent2) < 0.06) rawCurrent2 = 0; 
  filteredCurrent2 = abs((rawCurrent2 * ALPHA) + (filteredCurrent2 * (1 - ALPHA)));

  const char* currentState2 = (filteredCurrent2 > CURRENT_THRESHOLD_2) ? "OK" : "FAIL";

  if (strcmp(currentState2, lastCurrentState2) != 0) {
    lastCurrentState2 = currentState2;
    client.publish(current2_topic, currentState2, true);
  }
  //-----tesitng code remove later -----
  Serial.println("Voltage CS2: ");
  Serial.println(voltage2);
}

// --- NEW: INTEGRATED BATTERY MONITOR ---
void checkBattery() {
  if (millis() - lastBattReadTime < BATT_READ_INTERVAL) return;
  lastBattReadTime = millis();

  // 1. Take 100 samples to average out ADC noise
  long totalADC = 0;
  for (int i = 0; i < 100; i++) {
    totalADC += analogRead(BATTERY_PIN);
  }
  float avgADC = totalADC / 100.0;

  // 2. Calculate the base battery voltage
  float pinVoltage = (avgADC / ADC_RESOLUTION) * ADC_REF_VOLT; 
  float battVoltage = pinVoltage * ((R1 + R2) / R2) * K_CALIBRATION;

  // 3. Compensate for Voltage Sag based on active Relay Loads
  if (currentRelayState == 1 || currentRelayState == 2) {
    battVoltage += 0.40; // Add 0.40V if one LED is ON
  } else if (currentRelayState == 3) {
    battVoltage += 0.58; // Add 0.58V if both LEDs are ON
  }

  // 4. Calculate Discrete Battery Percentage (3 Tiers) using compensated voltage
  int percentage = 0;
  if (battVoltage >= 12.1) {
    percentage = 100;
  } 
  else if (battVoltage >= 11.5) {
    percentage = 50;
  } 
  else {
    percentage = 0;
  }

  // 5. Publish only if the percentage changes
  if (percentage != lastBattPct) {
    lastBattPct = percentage;
    char pctStr[8];
    snprintf(pctStr, sizeof(pctStr), "%d", percentage);
    client.publish(batt_pct_topic, pctStr, true);
    
    Serial.printf(">> Battery Update: %.2fV (Compensated) -> %d%%\n", battVoltage, percentage);
  }
  //-----tesitng code remove later -----
  Serial.println("Batt Volt: ");
  Serial.println(battVoltage);
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
      const char* payload = pwr_stableState ? "OK" : "FAIL";
      client.publish(power_topic, payload, true);
      Serial.printf(">> Power Changed: %s\n", payload);
    }
  }
}

boolean mqtt_connect() {
  if (!eth_connected) return false;
  
  Serial.print("Attempting MQTT connection...");
  
  char clientId[30];
  snprintf(clientId, sizeof(clientId), "ESP32EthClient-%s", ETH.macAddress().c_str());

  if (client.connect(clientId, status_topic, 1, true, "OFFLINE")) {
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

  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);
  digitalWrite(RELAY_1_PIN, HIGH);
  digitalWrite(RELAY_2_PIN, HIGH);
  
  pinMode(POWER_MONITOR_PIN, INPUT);
  pinMode(CURRENT_PIN_1, INPUT);
  pinMode(CURRENT_PIN_2, INPUT);
  pinMode(BATTERY_PIN, INPUT); // NEW: Initialize Battery ADC pin

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  sprintf(control_topic, "metro/signage/register/%d/value", ASSIGNED_REGISTER);
  sprintf(status_topic, "metro/signage/register/%d/status", ASSIGNED_REGISTER);
  sprintf(power_topic, "metro/signage/register/%d/power", ASSIGNED_REGISTER);
  sprintf(current1_topic, "metro/signage/register/%d/current1", ASSIGNED_REGISTER);
  sprintf(current2_topic, "metro/signage/register/%d/current2", ASSIGNED_REGISTER);
  sprintf(batt_pct_topic, "metro/signage/register/%d/battery_pct", ASSIGNED_REGISTER); // NEW

  Serial.printf("\n--- ESP32 Ethernet Signage Controller (%d) ---\n", ASSIGNED_REGISTER);
  
  WiFi.onEvent(eth_event_handler);
  
  // Standard Arduino IDE Ethernet Initialization (ESP32 Core v3.x Argument Order)
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
  ETH.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);

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
    checkBattery(); // NEW: Run the battery check loop
  }
}