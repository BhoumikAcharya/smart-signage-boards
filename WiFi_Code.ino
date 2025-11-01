#include <WiFi.h>
#include <PubSubClient.h>

// --- Configuration ---
const char* ssid = "Laptop";
const char* password = "oneminus";
const char* mqtt_server_ip = "192.168.0.15";
const int mqtt_port = 1883;

const int RELAY_1_PIN = 23;
const int RELAY_2_PIN = 22;

// --- Assign a register to this ESP32
const int ASSIGNED_REGISTER = 40005; // Example for 10th unit

char control_topic[50];
char status_topic[50];

WiFiClient espClient;
PubSubClient client(espClient);
long lastReconnectAttempt = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  int control_value = message.toInt();
  Serial.printf("Received control value: %d\n", control_value);

  // LOW = OFF, HIGH = ON.
  switch (control_value) {
    case 0: // Both OFF
      Serial.println("Action: Turning both relays OFF.");
      digitalWrite(RELAY_1_PIN, LOW); 
      digitalWrite(RELAY_2_PIN, LOW); 
      break;
    case 1: // Relay 1 ON, Relay 2 OFF
      Serial.println("Action: Turning Relay 1 ON, Relay 2 OFF.");
      digitalWrite(RELAY_1_PIN, HIGH);  
      digitalWrite(RELAY_2_PIN, LOW); 
      break;
    case 2: // Relay 1 OFF, Relay 2 ON
      Serial.println("Action: Turning Relay 1 OFF, Relay 2 ON.");
      digitalWrite(RELAY_1_PIN, LOW); 
      digitalWrite(RELAY_2_PIN, HIGH);  
      break;
    case 3: // Both ON
      Serial.println("Action: Turning both relays ON.");
      digitalWrite(RELAY_1_PIN, HIGH);  
      digitalWrite(RELAY_2_PIN, HIGH);  
      break;
    default: // Default: Both OFF
      Serial.println("Warning: Unhandled value. Turning both relays OFF.");
      digitalWrite(RELAY_1_PIN, LOW); 
      digitalWrite(RELAY_2_PIN, LOW); 
      break;
  }
}

boolean mqtt_connect() {
  Serial.print("Attempting MQTT connection...");

//Using MAC Address as the unique identification for the RaspberryPi --> needo change it to a mapped IP Address  
  String clientId = "ESP32Client-";
  clientId += String(WiFi.macAddress());

  // LWT parameters: topic, QoS, retain, message
  if (client.connect(clientId.c_str(), status_topic, 1, true, "OFFLINE")) {
    Serial.println(">>> Connected to MQTT Broker! <<<");
    client.publish(status_topic, "ONLINE", true);
    Serial.println("Published ONLINE status.");
    client.subscribe(control_topic);
    Serial.printf("Subscribed to command topic: %s\n", control_topic);
  } else {
    Serial.printf("failed, rc=%d try again in 5 seconds\n", client.state());
  }
  return client.connected();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  sprintf(control_topic, "metro/signage/register/%d/value", ASSIGNED_REGISTER);
  sprintf(status_topic, "metro/signage/register/%d/status", ASSIGNED_REGISTER);

  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);
  // Set initial state to OFF (HIGH for active-low)
  digitalWrite(RELAY_1_PIN, LOW); 
  digitalWrite(RELAY_2_PIN, LOW);

  Serial.printf("\n--- ESP32 Signage Controller for Register %d ---\n", ASSIGNED_REGISTER);
  Serial.printf("Connecting to Wi-Fi: %s\n", ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi Connected!");
  Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());

  client.setServer(mqtt_server_ip, mqtt_port);
  client.setCallback(callback);
  lastReconnectAttempt = 0;
}

void loop() {
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
  }
}
