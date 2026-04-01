/*
 * LymphedemaSleeve.ino
 * Main Firmware
 *
 * BYPASS FLAG:
 *   true  = Temperature only (current mode - FSRs not yet calibrated)
 *   false = Full OR logic   (flip when FSRs are calibrated)
 *
 * State Machine:
 *   CALIBRATE  → Capture baseline temperature (+ pressure if not bypassed)
 *   MONITOR    → Read sensors every 500ms, check OR trigger
 *   COMPRESS   → Inflate 30s → Hold 15s → Release 15s
 *   COOLDOWN   → Wait 30s before resuming monitoring
 *
 * Trigger Logic (OR):
 *   Compression fires if EITHER:
 *     - Temperature rises >= +1.0°C above baseline
 *     - Pressure rises   >= PRESSURE_DEVIATION_THRESHOLD above baseline
 */

#include <Wire.h>
#include <Adafruit_TMP117.h>
#include <EEPROM.h>
#include "pins.h"
#include "config.h"

// ============================================================
// BYPASS FLAG
// Set to false once FSR sensors are wired and calibrated
// ============================================================
#define BYPASS_PRESSURE_SENSORS true

// ============================================================================
// mmHg CALIBRATION TABLE
// ============================================================================
// Replace ADC values with YOUR actual measured values using penny weights
// Format: {ADC_value, mmHg_value}
// ADC values MUST increase as mmHg increases
// ============================================================================
float CAL_TABLE[][2] = {
  {0,    0},   // 0 mmHg  → replace with your measured ADC at 0 mmHg
  {100,  10},  // 10 mmHg → replace with your measured ADC at 10 mmHg
  {300,  20},  // 20 mmHg → replace with your measured ADC at 20 mmHg
  {600,  30},  // 30 mmHg → replace with your measured ADC at 30 mmHg
  {1000, 40}   // 40 mmHg → replace with your measured ADC at 40 mmHg
};
const int CAL_TABLE_SIZE = 5;

// ============================================================================
// STATE MACHINE
// ============================================================================
enum SystemState {
  STATE_CALIBRATE,
  STATE_MONITOR,
  STATE_COMPRESS,
  STATE_COOLDOWN
};

SystemState currentState = STATE_CALIBRATE;

// ============================================================================
// GLOBAL OBJECTS + VARIABLES
// ============================================================================
Adafruit_TMP117 tmp117;

float tempBaseline      = 0.0;
float currentTemp       = 0.0;
bool  tempTriggered     = false;

float pressureBaseline  = 0.0;
float currentPressure   = 0.0;
bool  pressureTriggered = false;

unsigned long cooldownStart  = 0;
unsigned long lastSensorRead = 0;

// ============================================================================
// HELPER: Celsius to Fahrenheit
// ============================================================================
float toFahrenheit(float celsius) {
  return (celsius * 9.0 / 5.0) + 32.0;
}

// ============================================================================
// HELPER: ADC to mmHg using calibration table
// ============================================================================
float adcToMmHg(float adcValue) {
  if (adcValue <= CAL_TABLE[0][0]) return 0.0;
  if (adcValue >= CAL_TABLE[CAL_TABLE_SIZE - 1][0])
    return CAL_TABLE[CAL_TABLE_SIZE - 1][1];

  for (int i = 0; i < CAL_TABLE_SIZE - 1; i++) {
    if (adcValue >= CAL_TABLE[i][0] && adcValue <= CAL_TABLE[i + 1][0]) {
      float adcRange  = CAL_TABLE[i + 1][0] - CAL_TABLE[i][0];
      float mmhgRange = CAL_TABLE[i + 1][1] - CAL_TABLE[i][1];
      float fraction  = (adcValue - CAL_TABLE[i][0]) / adcRange;
      return CAL_TABLE[i][1] + (fraction * mmhgRange);
    }
  }
  return 0.0;
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);

  Serial.println("========================================");
  Serial.println("  LymphedemaSleeve - Group 10");
  Serial.println("  Biomedical Senior Design");
  Serial.println("  University of Arkansas");
  Serial.println("========================================");

  // Motor driver pins
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT); pinMode(PWMA, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT); pinMode(PWMB, OUTPUT);

  // Safe initial state: pump off, solenoid closed
  digitalWrite(AIN1, LOW); digitalWrite(AIN2, LOW); analogWrite(PWMA, 0);
  digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW); analogWrite(PWMB, 0);

  pinMode(LED_BUILTIN, OUTPUT);
  analogReadResolution(12);

  // Pressure pins configured even if bypassed
  pinMode(PRESSURE_PIN_1, INPUT);
  pinMode(PRESSURE_PIN_2, INPUT);
  pinMode(PRESSURE_PIN_3, INPUT);

  // TMP117 temperature sensor
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.print("Initializing TMP117... ");
  if (!tmp117.begin()) {
    Serial.println("FAILED!");
    Serial.println("  Check: SDA->GPIO21, SCL->GPIO22, VCC->3.3V");
    while (1) delay(500);
  }
  Serial.println("OK!");

  // EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // Print mode
  Serial.println("========================================");
  if (BYPASS_PRESSURE_SENSORS) {
    Serial.println("  MODE: Temperature + Pneumatics");
    Serial.println("  Pressure: BYPASSED");
    Serial.println("  Trigger:  Temperature only (+1.0 C)");
  } else {
    Serial.println("  MODE: Full System (OR logic)");
    Serial.println("  Trigger:  Temperature OR Pressure");
  }
  Serial.println("  Inflate:  30s @ speed 200");
  Serial.println("  Hold:     15s");
  Serial.println("  Release:  15s");
  Serial.println("  Cooldown: 30s");
  Serial.println("========================================\n");

  currentState = STATE_CALIBRATE;
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  switch (currentState) {
    case STATE_CALIBRATE:
      runCalibration();
      currentState = STATE_MONITOR;
      break;
    case STATE_MONITOR:
      runMonitor();
      break;
    case STATE_COMPRESS:
      runCompression();
      cooldownStart = millis();
      currentState  = STATE_COOLDOWN;
      break;
    case STATE_COOLDOWN:
      runCooldown();
      break;
    default:
      currentState = STATE_CALIBRATE;
      break;
  }

  // Heartbeat LED blinks every second
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 1000) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    lastBlink = millis();
  }
}

// ============================================================================
// STATE: CALIBRATE
// 20-sample average of temp (and pressure if not bypassed)
// ============================================================================
void runCalibration() {
  Serial.println("========================================");
  Serial.println("[CALIBRATE] Capturing patient baseline...");
  Serial.println("  Keep arm still and relaxed.");
  Serial.println("  Sampling for ~3 seconds...");
  Serial.println("========================================\n");

  // Temperature baseline
  float tempSum = 0.0;
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    sensors_event_t event;
    tmp117.getEvent(&event);
    tempSum += event.temperature;

    Serial.print("  Sample ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.print(CALIBRATION_SAMPLES);
    Serial.print(": ");
    Serial.print(event.temperature, 2);
    Serial.print(" C / ");
    Serial.print(toFahrenheit(event.temperature), 2);
    Serial.println(" F");

    delay(CALIBRATION_DELAY_MS);
  }
  tempBaseline = tempSum / CALIBRATION_SAMPLES;

  // Pressure baseline (only if not bypassed)
  if (!BYPASS_PRESSURE_SENSORS) {
    float pressureSum = 0.0;
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
      float p1 = analogRead(PRESSURE_PIN_1);
      float p2 = analogRead(PRESSURE_PIN_2);
      float p3 = analogRead(PRESSURE_PIN_3);
      pressureSum += (p1 + p2 + p3) / 3.0;
      delay(CALIBRATION_DELAY_MS);
    }
    pressureBaseline = pressureSum / CALIBRATION_SAMPLES;
    Serial.print("[CALIBRATE] Pressure baseline: ");
    Serial.print((int)pressureBaseline);
    Serial.print(" ADC (");
    Serial.print(adcToMmHg(pressureBaseline), 1);
    Serial.println(" mmHg)");
  } else {
    Serial.println("[CALIBRATE] Pressure: SKIPPED");
  }

  // Print temperature baseline in both units
  Serial.print("[CALIBRATE] Temp baseline:     ");
  Serial.print(tempBaseline, 2);
  Serial.print(" C / ");
  Serial.print(toFahrenheit(tempBaseline), 2);
  Serial.println(" F");

  Serial.print("[CALIBRATE] Will trigger at: >");
  Serial.print(tempBaseline + TEMP_THRESHOLD_POSSIBLE, 2);
  Serial.print(" C / >");
  Serial.print(toFahrenheit(tempBaseline + TEMP_THRESHOLD_POSSIBLE), 2);
  Serial.println(" F\n");

  Serial.println("[CALIBRATE] Done! Monitoring started.\n");
}

// ============================================================================
// STATE: MONITOR
// Reads sensors every 500ms and checks OR trigger logic
// ============================================================================
void runMonitor() {
  unsigned long now = millis();
  if (now - lastSensorRead < SENSOR_READ_INTERVAL_MS) return;
  lastSensorRead = now;

  // Read temperature
  sensors_event_t event;
  tmp117.getEvent(&event);
  currentTemp = event.temperature;
  float tempDelta   = currentTemp - tempBaseline;
  float currentTempF = toFahrenheit(currentTemp);

  // Read pressure only if enabled
  float pressureDelta = 0.0;
  if (!BYPASS_PRESSURE_SENSORS) {
    float p1 = analogRead(PRESSURE_PIN_1);
    float p2 = analogRead(PRESSURE_PIN_2);
    float p3 = analogRead(PRESSURE_PIN_3);
    currentPressure   = (p1 + p2 + p3) / 3.0;
    pressureDelta     = currentPressure - pressureBaseline;
    pressureTriggered = (pressureDelta >= PRESSURE_DEVIATION_THRESHOLD);
  }

  // Print monitoring line with C and F
  Serial.print("[MONITOR] Temp: ");
  Serial.print(currentTemp, 2);
  Serial.print(" C / ");
  Serial.print(currentTempF, 2);
  Serial.print(" F | D: ");
  if (tempDelta >= 0) Serial.print("+");
  Serial.print(tempDelta, 2);
  Serial.print(" C | Pressure: ");

  if (BYPASS_PRESSURE_SENSORS) {
    Serial.print("BYPASSED | ");
  } else {
    Serial.print((int)currentPressure);
    Serial.print(" ADC (");
    Serial.print(adcToMmHg(currentPressure), 1);
    Serial.print(" mmHg) D:");
    if (pressureDelta >= 0) Serial.print("+");
    Serial.print((int)pressureDelta);
    Serial.print(" | ");
  }

  // Trigger check
  tempTriggered = (tempDelta >= TEMP_THRESHOLD_POSSIBLE);

  if (tempTriggered || pressureTriggered) {
    Serial.println("SWELLING SIGNAL!");
    if (tempTriggered)     Serial.println("  -> Temp threshold exceeded");
    if (pressureTriggered) Serial.println("  -> Pressure threshold exceeded");
    currentState = STATE_COMPRESS;
  } else {
    Serial.println("Normal");
  }
}

// ============================================================================
// STATE: COMPRESS
// Inflate 30s → Hold 15s → Release 15s
// ============================================================================
void runCompression() {
  Serial.println("\n========================================");
  Serial.println("[COMPRESS] Starting compression cycle");
  Serial.println("========================================");

  // Phase 1: Inflate
  Serial.println("[COMPRESS] Phase 1: Inflate (30s)...");
  digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
  analogWrite(PWMA, 200);          // Maximum pump speed
  digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW);
  analogWrite(PWMB, 0);            // Solenoid closed - trap air
  delay(30000);

  // Phase 2: Hold
  Serial.println("[COMPRESS] Phase 2: Hold (15s)...");
  analogWrite(PWMA, 0);            // Pump off
  digitalWrite(AIN1, LOW);
  delay(15000);

  // Phase 3: Release
  Serial.println("[COMPRESS] Phase 3: Release (15s)...");
  digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
  analogWrite(PWMB, 255);          // Solenoid open - vent air
  delay(15000);

  // Safe state - everything off
  digitalWrite(BIN1, LOW); analogWrite(PWMB, 0);
  digitalWrite(AIN1, LOW); analogWrite(PWMA, 0);

  Serial.println("[COMPRESS] Cycle complete.\n");
}

// ============================================================================
// STATE: COOLDOWN
// Waits 30 seconds before returning to monitoring
// ============================================================================
void runCooldown() {
  unsigned long elapsed = millis() - cooldownStart;

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 5000) {
    int remaining = (int)((COOLDOWN_DURATION_MS - elapsed) / 1000);
    if (remaining < 0) remaining = 0;
    Serial.print("[COOLDOWN] Resuming in ");
    Serial.print(remaining);
    Serial.println("s...");
    lastPrint = millis();
  }

  if (elapsed >= COOLDOWN_DURATION_MS) {
    Serial.println("[COOLDOWN] Complete. Back to monitoring.\n");
    currentState = STATE_MONITOR;
  }
}
