/*
 * LymphedemaSleeve - Main Firmware
 * Group 10 - BMEG
 */

#include "pins.h"

// ========================================
// GLOBAL VARIABLES
// ========================================
unsigned long lastPrint = 0;
int counter = 0;

// ========================================
// SETUP - Runs once at startup
// ========================================
void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);
  
  // Print startup message
  Serial.println("========================================");
  Serial.println("LymphedemaSleeve - Initialization");
  Serial.println("Group 10 - Biomedical Senior Design");
  Serial.println("========================================");
  
  // Configure motor driver pins as outputs
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  
  // Configure built-in LED as output
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Configure sensor pins as inputs
  pinMode(PRESSURE_PIN_1, INPUT);
  pinMode(PRESSURE_PIN_2, INPUT);
  pinMode(PRESSURE_PIN_3, INPUT);
  pinMode(SODIUM_PIN, INPUT);
  
  Serial.println("✓ All pins configured");
  Serial.println("✓ System initialized");
  Serial.println("========================================");
}

// ========================================
// MAIN LOOP - Runs continuously
// ========================================
void loop() {
  // Blink LED and print message every second
  unsigned long currentTime = millis();
  
  if (currentTime - lastPrint >= 1000) {
    // Toggle LED
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    
    // Print status
    counter++;
    Serial.print("System running... count: ");
    Serial.println(counter);
    
    lastPrint = currentTime;
  }
}
