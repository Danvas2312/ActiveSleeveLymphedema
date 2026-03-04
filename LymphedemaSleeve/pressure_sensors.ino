/**
 * pressure_sensors.ino
 * FSR 402 Pressure Sensor Module
 *
 *
 * Hardware Wiring (voltage divider per FSR 402 datasheet):
 *
 *   ESP32 3.3V ──────┐
 *                    │
 *                 10k resistor
 *                    │
 *               GPIO Pin  ADC read here
 *                    │
 *                 FSR 402
 *                    │
 *   ESP32 GND ───────┘
 *
 * Sensor Placement:
 *   GPIO 34  Sensor 1 (Proximal / shoulder)
 *   GPIO 35  Sensor 2 (Middle / forearm)
 *   GPIO 36  Sensor 3 (Distal / wrist)
 *
 * Commands (Serial Monitor at 115200 baud):
 *   C - Calibrate baseline
 *   L - Load baseline from EEPROM
 *   R - Show raw detailed readings
 *   T - Test swelling detection
 *   S - Show system status
 *   X - Clear saved calibration
 *   M - Show menu
 */

#include <EEPROM.h>
#include "config.h"
#include "pins.h"

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

float pressureBaseline = 0.0;
bool  isCalibrated     = false;

// Moving average buffers for each sensor
float sensor1Buffer[MOVING_AVG_SIZE] = {0};
float sensor2Buffer[MOVING_AVG_SIZE] = {0};
float sensor3Buffer[MOVING_AVG_SIZE] = {0};
int   bufferIndex = 0;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);

  Serial.println("\n\n========================================");
  Serial.println("  PRESSURE SENSOR SYSTEM");
  Serial.println("  Group 10 - Biomedical Senior Design");
  Serial.println("========================================\n");

  // 12-bit ADC resolution (0-4095)
  analogReadResolution(12);

  // Configure pressure sensor pins
  pinMode(PRESSURE_SENSOR_1_PIN, INPUT);
  pinMode(PRESSURE_SENSOR_2_PIN, INPUT);
  pinMode(PRESSURE_SENSOR_3_PIN, INPUT);

  Serial.println("Sensor pins configured:");
  Serial.print("  Sensor 1 (Proximal/shoulder): GPIO ");
  Serial.println(PRESSURE_SENSOR_1_PIN);
  Serial.print("  Sensor 2 (Middle/forearm):    GPIO ");
  Serial.println(PRESSURE_SENSOR_2_PIN);
  Serial.print("  Sensor 3 (Distal/wrist):      GPIO ");
  Serial.println(PRESSURE_SENSOR_3_PIN);

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  Serial.print("\nEEPROM initialized (");
  Serial.print(EEPROM_SIZE);
  Serial.println(" bytes)");

  // Initialize moving average buffers
  for (int i = 0; i < MOVING_AVG_SIZE; i++) {
    sensor1Buffer[i] = 0;
    sensor2Buffer[i] = 0;
    sensor3Buffer[i] = 0;
  }

  // Try to load saved calibration from EEPROM
  if (loadPressureBaselineFromEEPROM()) {
    Serial.println("Pressure baseline loaded from EEPROM!\n");
    isCalibrated = true;
  } else {
    Serial.println("No saved baseline found.");
    Serial.println("  Press C to calibrate\n");
  }

  showMenu();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Check for serial commands
  if (Serial.available()) {
    char cmd = Serial.read();
    handleCommand(cmd);
  }

  // If calibrated, run swelling monitoring
  if (isCalibrated) {
    monitorPressure();
  } else {
    showRawReadings();
  }

  delay(SENSOR_READ_INTERVAL_MS);
}

// ============================================================================
// SENSOR READING FUNCTIONS
// ============================================================================

float readPressureSensor(int pin) {
  int adcValue = analogRead(pin);
  if (adcValue < 0 || adcValue > ADC_MAX_VALUE) {
    Serial.print("WARNING: GPIO ");
    Serial.print(pin);
    Serial.print(" out of range: ");
    Serial.println(adcValue);
    return 0.0;
  }
  return (float)adcValue;
}

void updateMovingAverage() {
  sensor1Buffer[bufferIndex] = readPressureSensor(PRESSURE_SENSOR_1_PIN);
  sensor2Buffer[bufferIndex] = readPressureSensor(PRESSURE_SENSOR_2_PIN);
  sensor3Buffer[bufferIndex] = readPressureSensor(PRESSURE_SENSOR_3_PIN);
  bufferIndex = (bufferIndex + 1) % MOVING_AVG_SIZE;
}

float getAveragePressure() {
  updateMovingAverage();
  float sum1 = 0, sum2 = 0, sum3 = 0;
  for (int i = 0; i < MOVING_AVG_SIZE; i++) {
    sum1 += sensor1Buffer[i];
    sum2 += sensor2Buffer[i];
    sum3 += sensor3Buffer[i];
  }
  return ((sum1 + sum2 + sum3) / MOVING_AVG_SIZE) / 3.0;
}

void showRawReadings() {
  float p1  = readPressureSensor(PRESSURE_SENSOR_1_PIN);
  float p2  = readPressureSensor(PRESSURE_SENSOR_2_PIN);
  float p3  = readPressureSensor(PRESSURE_SENSOR_3_PIN);
  float avg = (p1 + p2 + p3) / 3.0;
  Serial.print("S1: "); Serial.print((int)p1);
  Serial.print(" | S2: "); Serial.print((int)p2);
  Serial.print(" | S3: "); Serial.print((int)p3);
  Serial.print(" | Avg: "); Serial.println((int)avg);
}

// ============================================================================
// CALIBRATION
// ============================================================================

float calibratePressureBaseline() {
  float sum = 0.0;

  Serial.println("\n========================================");
  Serial.println("  PRESSURE CALIBRATION");
  Serial.println("========================================");
  Serial.println("Keep arm relaxed, sleeve NOT inflated.");
  Serial.print("Taking "); Serial.print(CALIBRATION_SAMPLES);
  Serial.println(" samples (~3 seconds)...\n");

  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    float avgPressure = getAveragePressure();
    if (avgPressure < 0 || avgPressure > ADC_MAX_VALUE) {
      Serial.println("  INVALID sample, retrying...");
      i--;
      continue;
    }
    sum += avgPressure;
    Serial.print("  Sample "); Serial.print(i + 1);
    Serial.print("/"); Serial.print(CALIBRATION_SAMPLES);
    Serial.print(": "); Serial.print((int)avgPressure);
    Serial.println(" ADC");
    delay(CALIBRATION_DELAY_MS);
  }

  float baseline = sum / CALIBRATION_SAMPLES;
  Serial.print("\nBaseline: "); Serial.print((int)baseline); Serial.println(" ADC");
  Serial.print("Swelling threshold: +"); Serial.print(PRESSURE_DEVIATION_THRESHOLD);
  Serial.println(" ADC above baseline");
  Serial.println("========================================\n");

  return baseline;
}

// ============================================================================
// EEPROM FUNCTIONS
// ============================================================================

void savePressureBaselineToEEPROM(float baseline) {
  if (baseline < 0 || baseline > ADC_MAX_VALUE) {
    Serial.println("ERROR: Invalid baseline, not saved.");
    return;
  }
  EEPROM.writeFloat(ADDR_PRESSURE_BASELINE, baseline);
  EEPROM.write(ADDR_CALIBRATION_FLAG, CALIBRATION_VALID_FLAG);
  EEPROM.commit();
  Serial.print("Saved to EEPROM: "); Serial.print((int)baseline); Serial.println(" ADC\n");
}

bool loadPressureBaselineFromEEPROM() {
  Serial.println("Checking EEPROM...");
  byte flag = EEPROM.read(ADDR_CALIBRATION_FLAG);
  if (flag == CALIBRATION_VALID_FLAG) {
    float baseline = EEPROM.readFloat(ADDR_PRESSURE_BASELINE);
    if (baseline < 0 || baseline > ADC_MAX_VALUE) {
      Serial.println("EEPROM data corrupted.");
      return false;
    }
    pressureBaseline = baseline;
    Serial.print("Baseline loaded: "); Serial.print((int)pressureBaseline); Serial.println(" ADC");
    return true;
  }
  Serial.println("No valid calibration found.");
  return false;
}

void clearEEPROM() {
  EEPROM.write(ADDR_CALIBRATION_FLAG, 0x00);
  EEPROM.commit();
  Serial.println("EEPROM calibration cleared.\n");
}

// ============================================================================
// SWELLING MONITORING
// ============================================================================

void monitorPressure() {
  float currentPressure = getAveragePressure();
  float deviation       = currentPressure - pressureBaseline;
  float deviationPct    = (pressureBaseline > 0)
                          ? (deviation / pressureBaseline) * 100.0
                          : 0.0;

  Serial.print("Pressure: "); Serial.print((int)currentPressure);
  Serial.print(" ADC | D: ");
  if (deviation >= 0) Serial.print("+");
  Serial.print((int)deviation);
  Serial.print(" (");
  if (deviationPct >= 0) Serial.print("+");
  Serial.print(deviationPct, 1);
  Serial.print("%) | ");

  if (deviation >= PRESSURE_DEVIATION_THRESHOLD) {
    Serial.println("SWELLING SIGNAL - compression may be needed");
  } else if (deviation >= PRESSURE_DEVIATION_THRESHOLD * 0.5) {
    Serial.println("ELEVATED - monitoring closely");
  } else {
    Serial.println("Normal");
  }
}

// ============================================================================
// COMMAND HANDLER
// ============================================================================

void handleCommand(char cmd) {
  Serial.println();
  switch (cmd) {
    case 'C': case 'c':
      pressureBaseline = calibratePressureBaseline();
      savePressureBaselineToEEPROM(pressureBaseline);
      isCalibrated = true;
      showMenu();
      break;
    case 'L': case 'l':
      if (loadPressureBaselineFromEEPROM()) isCalibrated = true;
      showMenu();
      break;
    case 'R': case 'r':
      displayDetailedReadings();
      showMenu();
      break;
    case 'T': case 't':
      testDetection();
      showMenu();
      break;
    case 'S': case 's':
      showStatus();
      showMenu();
      break;
    case 'X': case 'x':
      clearEEPROM();
      isCalibrated = false;
      pressureBaseline = 0.0;
      showMenu();
      break;
    case 'M': case 'm':
      showMenu();
      break;
    default:
      Serial.println("Unknown command. Press M for menu.\n");
      break;
  }
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

void displayDetailedReadings() {
  Serial.println("========================================");
  Serial.println("  DETAILED PRESSURE READINGS");
  Serial.println("========================================");
  float p1  = readPressureSensor(PRESSURE_SENSOR_1_PIN);
  float p2  = readPressureSensor(PRESSURE_SENSOR_2_PIN);
  float p3  = readPressureSensor(PRESSURE_SENSOR_3_PIN);
  float avg = (p1 + p2 + p3) / 3.0;
  Serial.print("Sensor 1 (Proximal) GPIO "); Serial.print(PRESSURE_SENSOR_1_PIN);
  Serial.print(": "); Serial.print((int)p1); Serial.println(" ADC");
  Serial.print("Sensor 2 (Middle)   GPIO "); Serial.print(PRESSURE_SENSOR_2_PIN);
  Serial.print(": "); Serial.print((int)p2); Serial.println(" ADC");
  Serial.print("Sensor 3 (Distal)   GPIO "); Serial.print(PRESSURE_SENSOR_3_PIN);
  Serial.print(": "); Serial.print((int)p3); Serial.println(" ADC");
  Serial.print("\nAverage: "); Serial.print((int)avg); Serial.println(" ADC");
  if (isCalibrated) {
    float dev = avg - pressureBaseline;
    Serial.print("Deviation: ");
    if (dev >= 0) Serial.print("+");
    Serial.print((int)dev); Serial.println(" ADC");
  }
  Serial.println("========================================\n");
}

void testDetection() {
  if (!isCalibrated) { Serial.println("Not calibrated. Press C first.\n"); return; }
  Serial.println("========================================");
  Serial.println("  SWELLING DETECTION TEST");
  Serial.println("========================================");
  float current = getAveragePressure();
  float dev     = current - pressureBaseline;
  Serial.print("Baseline:  "); Serial.print((int)pressureBaseline); Serial.println(" ADC");
  Serial.print("Current:   "); Serial.print((int)current); Serial.println(" ADC");
  Serial.print("Deviation: ");
  if (dev >= 0) Serial.print("+");
  Serial.print((int)dev); Serial.println(" ADC");
  Serial.print("Threshold: +"); Serial.print(PRESSURE_DEVIATION_THRESHOLD); Serial.println(" ADC\n");
  if (dev >= PRESSURE_DEVIATION_THRESHOLD) {
    Serial.println("Result: SWELLING SIGNAL DETECTED");
    Serial.println("  Compression cycle would trigger (OR logic)");
  } else {
    Serial.println("Result: Normal - no swelling signal");
  }
  Serial.println("========================================\n");
}

void showStatus() {
  Serial.println("========================================");
  Serial.println("  SYSTEM STATUS");
  Serial.println("========================================");
  Serial.print("Calibration: ");
  Serial.println(isCalibrated ? "CALIBRATED" : "NOT CALIBRATED");
  if (isCalibrated) {
    Serial.print("  Baseline:  "); Serial.print((int)pressureBaseline); Serial.println(" ADC");
    Serial.print("  Threshold: +"); Serial.print(PRESSURE_DEVIATION_THRESHOLD); Serial.println(" ADC");
  }
  Serial.println("\nSensors: 3x FSR-402");
  Serial.println("Trigger Logic: OR (pressure alone triggers compression)");
  Serial.println("========================================\n");
}

void showMenu() {
  Serial.println("========================================");
  Serial.println("  COMMANDS");
  Serial.println("========================================");
  Serial.println("  C - Calibrate baseline");
  Serial.println("  L - Load baseline from EEPROM");
  Serial.println("  R - Raw readings (detailed)");
  Serial.println("  T - Test swelling detection");
  Serial.println("  S - Show status");
  Serial.println("  X - Clear calibration");
  Serial.println("  M - Show this menu");
  Serial.print("  Status: ");
  Serial.println(isCalibrated ? "CALIBRATED" : "NOT CALIBRATED - press C");
  Serial.println("========================================\n");
