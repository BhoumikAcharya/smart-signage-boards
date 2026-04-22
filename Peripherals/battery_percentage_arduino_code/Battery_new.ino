
#include <Arduino.h>

// ========== YOUR EXACT CIRCUIT VALUES ==========
const int batteryPin = 34;           // ADC pin connected between 3rd and 4th resistor

// Resistor values (in ohms)
const float R1 = 1000.0;             // 1kΩ
const float R2 = 2200.0;             // 2.2kΩ
const float R3 = 1000.0;             // 1kΩ
const float R4 = 1000.0;             // 1kΩ (to GND)

// Calculate divider ratio
const float R_TOP = R1 + R2 + R3;     // Resistance from positive to tap point = 4.2kΩ
const float R_BOTTOM = R4;            // Resistance from tap point to GND = 1kΩ
const float R_TOTAL = R_TOP + R_BOTTOM;  // Total = 5.2kΩ
const float DIVIDER_RATIO = R_BOTTOM / R_TOTAL;  // = 1000/5200 = 0.1923077

// ESP32 ADC Configuration
const float VREF = 3.3;              // ADC reference voltage
const int ADC_RESOLUTION = 4095;     // 12-bit (0-4095)

// Battery Parameters (for 12.8V LiFePO4 or Lithium-ion)
const float BATTERY_FULL_VOLTAGE = 12.8;    // 100% charge (absolute max)
const float BATTERY_EMPTY_VOLTAGE = 11.0;   // 0% charge (safe cutoff)


// ========== CALIBRATION ==========
// Adjust this value until ESP32 reading matches multimeter
// Start with 1.0 and increase/decrease as needed
const float CALIBRATION_FACTOR = 1.0632;  // 12.47/11.88 = 1.05

// Optional: If you want to measure and use actual divider ratio
// Uncomment and set these after measuring with multimeter
// const float ACTUAL_DIVIDER_RATIO = 0.198;  // Measure and update 

// ========== GLOBAL VARIABLES ==========
float batteryVoltage = 0;
float percentage = 0;
bool ledsAreOn = false;
unsigned long lastReadingTime = 0;

// ========== HELPER FUNCTIONS ==========

// Read raw ADC and convert to actual battery voltage
float readBatteryVoltage() {
  int rawADC = analogRead(batteryPin);
  
  // Calculate voltage at GPIO pin
  float voltageAtPin = (rawADC / (float)ADC_RESOLUTION) * VREF;
  
  // Calculate actual battery voltage using your divider ratio
  // V_battery = V_pin / DIVIDER_RATIO
  // Calculate battery voltage with calibration
  float batteryVoltage = (voltageAtPin / DIVIDER_RATIO) * CALIBRATION_FACTOR;
  
  return batteryVoltage;
}

// Calculate battery percentage
// float calculatePercentage(float voltage) {
//   // Constrain to valid range
//   if (voltage >= BATTERY_FULL_VOLTAGE) return 100.0;
//   if (voltage <= BATTERY_EMPTY_VOLTAGE) return 0.0;
  
//   // Linear interpolation
//   float percentage = ((voltage - BATTERY_EMPTY_VOLTAGE) / 
//                       (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE)) * 100.0;
  
//   return constrain(percentage, 0.0, 100.0);
// }

// ========== SETUP ==========
void setup() {
  Serial.begin(9600);
  delay(1000);
  
  Serial.println("========================================");
  Serial.println("   BATTERY MONITORING SYSTEM");
  Serial.println("========================================");
  Serial.println();
  
  // Serial.println("Circuit Configuration:");
  // Serial.println("  Battery (+) ---[1kΩ]---[2.2kΩ]---[1kΩ]---[1kΩ]--- GND");
  // Serial.println("                                    ↑");
  // Serial.println("                              GPIO 34 connected HERE");
  // Serial.println();
  
  Serial.print("Calibration Factor: ");
  Serial.println(CALIBRATION_FACTOR);
  Serial.print("  Divider Ratio: 1:");
  Serial.println(1.0 / DIVIDER_RATIO, 1);
  Serial.print("  Max Battery Voltage: ");
  Serial.print(BATTERY_FULL_VOLTAGE);
  Serial.println(" V");
  Serial.print("  Min Safe Voltage: ");
  Serial.print(BATTERY_EMPTY_VOLTAGE);
  Serial.println(" V");
  Serial.println();
  
  // Configure ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  
  delay(2000);
}

// ========== MAIN LOOP ==========
void loop() {
  // Read battery voltage every 2 seconds
  if (millis() - lastReadingTime >= 2000) {
    lastReadingTime = millis();
    
    // Take multiple readings for accuracy
    float sum = 0;
    for (int i = 0; i < 50; i++) {
      sum += readBatteryVoltage();
      delay(10);
    }
    batteryVoltage = sum / 50.0;
    
    // Display Results
    Serial.println("────────────────────────────────────────");
    Serial.println("         BATTERY STATUS");
    Serial.println("────────────────────────────────────────");
    
    Serial.print("ADC Reading: ");
    Serial.print(analogRead(batteryPin));
    Serial.print(" | Voltage at GPIO: ");
    Serial.print((analogRead(batteryPin) / 4095.0) * 3.3, 3);
    Serial.println(" V");
    
    Serial.print("Battery Voltage: ");
    Serial.print(batteryVoltage, 2);
    Serial.println(" V");
    
    // Calculate percentage
    // percentage = calculatePercentage(batteryVoltage);
    
    // Serial.print("Charge: ");
    // Serial.print(percentage, 1);
    // Serial.println(" %");
    
    // // Visual battery bar
    // Serial.print("[");
    // int bars = (int)(percentage / 10);
    // for (int i = 0; i < 10; i++) {
    //   Serial.print(i < bars ? "█" : "░");
    // }
    // Serial.print("]");
    // Serial.println();
    
    // Warnings
    if (batteryVoltage > 12.1) {
      Serial.println("\nBattery is at 100%");
    }
    else if (batteryVoltage >= 11.5 && batteryVoltage <= 12.1) {
      Serial.println("\nBattery is at 50%");
    }
    else {
      Serial.println("\nBattery is at 0%");
    }
    
    Serial.println("────────────────────────────────────────\n");
  }
}