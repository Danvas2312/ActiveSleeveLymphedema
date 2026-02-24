/**
 * Pressure_Sensors.ino
 * 
 * 
 * Features here:
 * - Read 3 FSR pressure sensors (analog)
 * - Calibrate pressure baseline
 * - EEPROM storage for pressure baseline
 * - Swelling detection based on pressure
 * - Sensor smoothing/filtering
 * 
 */

#include <EEPROM.h>
#include "config.h"
#include "pins.h"

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

float pressureBaseline = 0.0;
bool isCalibrated = false;

// Moving average buffers for sensor smoothing
const int MOVING_AVG_SIZE = 5;
float sensor1Buffer[MOVING_AVG_SIZE] = {0};
float sensor2Buffer[MOVING_AVG_SIZE] = {0};
float sensor3Buffer[MOVING_AVG_SIZE] = {0};
int bufferIndex = 0;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("  PRESSURE SENSOR SYSTEM");
  Serial.println("========================================\n");
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  Serial.print("✓ EEPROM initialized (");
  Serial.print(EEPROM_SIZE);
  Serial.println(" bytes)");
  
  // Configure pressure sensor pins
  pinMode(PRESSURE_SENSOR_1_PIN, INPUT);
  pinMode(PRESSURE_SENSOR_2_PIN, INPUT);
  pinMode(PRESSURE_SENSOR_3_PIN, INPUT);
  Serial.println("✓ Pressure sensor pins configured");
  
  Serial.print("  Sensor 1: GPIO ");
  Serial.println(PRESSURE_SENSOR_1_PIN);
  Serial.print("  Sensor 2: GPIO ");
  Serial.println(PRESSURE_SENSOR_2_PIN);
  Serial.print("  Sensor 3: GPIO ");
  Serial.println(PRESSURE_SENSOR_3_PIN);
  
  Serial.println("\n========================================\n");
  
  // Initialize sensor buffers
  for (int i = 0; i < MOVING_AVG_SIZE; i++) {
    sensor1Buffer[i] = 0;
    sensor2Buffer[i] = 0;
    sensor3Buffer[i] = 0;
  }
  
  // Try to load existing calibration
  if (loadPressureBaselineFromEEPROM()) {
    Serial.println("✓ Pressure baseline loaded!\n");
    isCalibrated = true;
  } else {
    Serial.println("No pressure baseline found");
    Serial.println("  Press 'C' to calibrate\n");
  }
  
  showMenu();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Check for commands (non-blocking)
  if (Serial.available()) {
    char cmd = Serial.read();
    handleCommand(cmd);
  }
  
  // If calibrated, monitor pressure
  if (isCalibrated) {
    monitorPressure();
  } else {
    // Show raw readings (for debugging)
    showRawReadings();
  }
  
  delay(SENSOR_READ_INTERVAL_MS);
}

// ============================================================================
// SENSOR READING FUNCTIONS
// ============================================================================

/**
 * Read single pressure sensor with validation
 * Returns raw ADC value (0-4095)
 */
float readPressureSensor(int pin) {
  int adcValue = analogRead(pin);
  
  // Validate ADC reading is within expected range
  if (adcValue < 0 || adcValue > 4095) {
    Serial.print("✗ WARNING: Sensor pin ");
    Serial.print(pin);
    Serial.print(" out of range: ");
    Serial.println(adcValue);
    return 0.0;
  }
  
  // TODO: Apply calibration formula from Sensors Team
  // float pressure_mmHg = calibrationFormula(adcValue);
  // return pressure_mmHg;
  
  // For now, return raw ADC
  return (float)adcValue;
}

/**
 * Apply moving average filter to smooth sensor readings
 */
void updateMovingAverage() {
  sensor1Buffer[bufferIndex] = readPressureSensor(PRESSURE_SENSOR_1_PIN);
  sensor2Buffer[bufferIndex] = readPressureSensor(PRESSURE_SENSOR_2_PIN);
  sensor3Buffer[bufferIndex] = readPressureSensor(PRESSURE_SENSOR_3_PIN);
  
  bufferIndex = (bufferIndex + 1) % MOVING_AVG_SIZE;
}

/**
 * Get smoothed average pressure from all 3 sensors
 */
float getAveragePressure() {
  updateMovingAverage();
  
  float sum1 = 0, sum2 = 0, sum3 = 0;
  
  for (int i = 0; i < MOVING_AVG_SIZE; i++) {
    sum1 += sensor1Buffer[i];
    sum2 += sensor2Buffer[i];
    sum3 += sensor3Buffer[i];
  }
  
  float avg1 = sum1 / MOVING_AVG_SIZE;
  float avg2 = sum2 / MOVING_AVG_SIZE;
  float avg3 = sum3 / MOVING_AVG_SIZE;
  
  return (avg1 + avg2 + avg3) / 3.0;
}

/**
 * Show raw readings from all sensors
 */
void showRawReadings() {
  float p1 = readPressureSensor(PRESSURE_SENSOR_1_PIN);
  float p2 = readPressureSensor(PRESSURE_SENSOR_2_PIN);
  float p3 = readPressureSensor(PRESSURE_SENSOR_3_PIN);
  float avg = (p1 + p2 + p3) / 3.0;
  
  Serial.print("Sensor 1: ");
  Serial.print((int)p1);
  Serial.print(" | Sensor 2: ");
  Serial.print((int)p2);
  Serial.print(" | Sensor 3: ");
  Serial.print((int)p3);
  Serial.print(" | Average: ");
  Serial.println((int)avg);
}

// ============================================================================
// CALIBRATION FUNCTIONS
// ============================================================================

/**
 * Calibrate pressure baseline (improved - non-blocking design)
 * Takes 20 samples from all 3 sensors and averages
 */
float calibratePressureBaseline() {
  float sum = 0.0;
  
  Serial.println("\n========================================");
  Serial.println("  PRESSURE CALIBRATION");
  Serial.println("========================================");
  Serial.println("Keep arm relaxed (no compression)");
  Serial.print("Taking ");
  Serial.print(CALIBRATION_SAMPLES);
  Serial.println(" samples...\n");
  
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    float avgPressure = getAveragePressure();
    
    // Validate reading
    if (avgPressure < 0 || avgPressure > 4095) {
      Serial.print("  Sample ");
      Serial.print(i + 1);
      Serial.println(" - INVALID, retrying...");
      i--; // Retry this sample
      continue;
    }
    
    sum += avgPressure;
    
    Serial.print("  Sample ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.print(CALIBRATION_SAMPLES);
    Serial.print(": ");
    Serial.print((int)avgPressure);
    Serial.println(" (avg ADC)");
    
    delay(CALIBRATION_DELAY_MS);
  }
  
  float baseline = sum / CALIBRATION_SAMPLES;
  
  Serial.print("\n✓ Pressure baseline: ");
  Serial.print((int)baseline);
  Serial.println(" (avg ADC)");
  Serial.println("========================================\n");
  
  return baseline;
}

// ============================================================================
// EEPROM FUNCTIONS
// ============================================================================

/**
 * Save pressure baseline to EEPROM
 */
void savePressureBaselineToEEPROM(float baseline) {
  Serial.println("Saving pressure baseline to EEPROM...");
  
  // Validate baseline before saving
  if (baseline < 0 || baseline > 4095) {
    Serial.println("ERROR: Invalid baseline value!");
    return;
  }
  
  // Write pressure baseline (address 4-7)
  EEPROM.writeFloat(ADDR_PRESSURE_BASELINE, baseline);
  
  // Write valid flag (address 16)
  EEPROM.write(ADDR_CALIBRATION_FLAG, CALIBRATION_VALID_FLAG);
  
  // Commit
  EEPROM.commit();
  
  Serial.println(" Pressure baseline saved!");
  Serial.print("  Pressure: ");
  Serial.print((int)baseline);
  Serial.println(" (ADC)");
  Serial.print("  Flag: 0x");
  Serial.println(CALIBRATION_VALID_FLAG, HEX);
  Serial.println();
}

/**
 * Load pressure baseline from EEPROM
 * Returns true if valid calibration found
 */
bool loadPressureBaselineFromEEPROM() {
  Serial.println("Checking EEPROM...");
  
  byte flag = EEPROM.read(ADDR_CALIBRATION_FLAG);
  
  if (flag == CALIBRATION_VALID_FLAG) {
    float baseline = EEPROM.readFloat(ADDR_PRESSURE_BASELINE);
    
    // Validate loaded baseline
    if (baseline < 0 || baseline > 4095) {
      Serial.println("Corrupted baseline data!");
      return false;
    }
    
    pressureBaseline = baseline;
    
    Serial.println(" Valid pressure calibration found!");
    Serial.print("  Pressure baseline: ");
    Serial.print((int)pressureBaseline);
    Serial.println(" (ADC)");
    Serial.println();
    
    return true;
  } else {
    Serial.println(" No valid calibration found");
    Serial.print("  Flag: 0x");
    Serial.print(flag, HEX);
    Serial.print(" (expected 0x");
    Serial.print(CALIBRATION_VALID_FLAG, HEX);
    Serial.println(")");
    Serial.println();
    
    return false;
  }
}

/**
 * Clear EEPROM
 */
void clearEEPROM() {
  Serial.println("Clearing EEPROM...");
  
  EEPROM.write(ADDR_CALIBRATION_FLAG, 0x00);
  EEPROM.commit();
  
  Serial.println("Calibration cleared\n");
}

// ============================================================================
// MONITORING
// ============================================================================

/**
 * Monitor pressure and detect swelling
 */
void monitorPressure() {
  float currentPressure = getAveragePressure();
  float deviation = currentPressure - pressureBaseline;
  
  Serial.print("Pressure: ");
  Serial.print((int)currentPressure);
  Serial.print(" (");
  if (deviation > 0) Serial.print("+");
  Serial.print((int)deviation);
  Serial.print(") | ");
  
  // Check threshold
  if (abs(deviation) < PRESSURE_DEVIATION_THRESHOLD * 0.5) {
    Serial.println("Normal");
  } else if (abs(deviation) < PRESSURE_DEVIATION_THRESHOLD) {
    Serial.println("Elevated");
  } else {
    Serial.println(" High pressure!");
  }
}

// ============================================================================
// USER INTERFACE
// ============================================================================

/**
 * Handle commands
 */
void handleCommand(char cmd) {
  Serial.println();
  
  switch(cmd) {
    case 'C':
    case 'c':
      Serial.println(">>> CALIBRATE <<<\n");
      pressureBaseline = calibratePressureBaseline();
      savePressureBaselineToEEPROM(pressureBaseline);
      isCalibrated = true;
      showMenu();
      break;
      
    case 'L':
    case 'l':
      Serial.println(">>> LOAD <<<\n");
      if (loadPressureBaselineFromEEPROM()) {
        isCalibrated = true;
      }
      showMenu();
      break;
      
    case 'R':
    case 'r':
      Serial.println(">>> RAW READINGS <<<\n");
      displayDetailedReadings();
      showMenu();
      break;
      
    case 'T':
    case 't':
      Serial.println(">>> TEST <<<\n");
      testDetection();
      showMenu();
      break;
      
    case 'X':
    case 'x':
      Serial.println(">>> CLEAR <<<\n");
      clearEEPROM();
      isCalibrated = false;
      pressureBaseline = 0.0;
      showMenu();
      break;
      
    case 'S':
    case 's':
      Serial.println(">>> STATUS <<<\n");
      showStatus();
      showMenu();
      break;
      
    case 'M':
    case 'm':
      showMenu();
      break;
      
    default:
      Serial.println(" Unknown command - Press 'M'\n");
      break;
  }
}

/**
 * Display detailed readings from all sensors
 */
void displayDetailedReadings() {
  Serial.println("========================================");
  Serial.println("  DETAILED PRESSURE READINGS");
  Serial.println("========================================\n");
  
  float p1 = readPressureSensor(PRESSURE_SENSOR_1_PIN);
  float p2 = readPressureSensor(PRESSURE_SENSOR_2_PIN);
  float p3 = readPressureSensor(PRESSURE_SENSOR_3_PIN);
  float avg = (p1 + p2 + p3) / 3.0;
  
  Serial.print("Sensor 1 (GPIO ");
  Serial.print(PRESSURE_SENSOR_1_PIN);
  Serial.print("): ");
  Serial.print((int)p1);
  Serial.println(" ADC");
  
  Serial.print("Sensor 2 (GPIO ");
  Serial.print(PRESSURE_SENSOR_2_PIN);
  Serial.print("): ");
  Serial.print((int)p2);
  Serial.println(" ADC");
  
  Serial.print("Sensor 3 (GPIO ");
  Serial.print(PRESSURE_SENSOR_3_PIN);
  Serial.print("): ");
  Serial.print((int)p3);
  Serial.println(" ADC");
  
  Serial.print("\nAverage: ");
  Serial.print((int)avg);
  Serial.println(" ADC");
  
  if (isCalibrated) {
    float dev = avg - pressureBaseline;
    Serial.print("Deviation from baseline: ");
    if (dev > 0) Serial.print("+");
    Serial.print((int)dev);
    Serial.println(" ADC");
  }
  
  Serial.println("========================================\n");
}

/**
 * Test swelling detection
 */
void testDetection() {
  if (!isCalibrated) {
    Serial.println("Not calibrated - Press 'C' first\n");
    return;
  }
  
  Serial.println("========================================");
  Serial.println("  SWELLING DETECTION TEST");
  Serial.println("========================================\n");
  
  float current = getAveragePressure();
  float dev = current - pressureBaseline;
  
  Serial.print("Current: ");
  Serial.print((int)current);
  Serial.print(" (baseline: ");
  Serial.print((int)pressureBaseline);
  Serial.println(")");
  
  Serial.print("Deviation: ");
  if (dev > 0) Serial.print("+");
  Serial.print((int)dev);
  Serial.println(" ADC");
  
  Serial.println("\nResult:");
  if (abs(dev) >= PRESSURE_DEVIATION_THRESHOLD) {
    Serial.println("   PRESSURE ALERT!");
    Serial.println("  → Swelling possible");
  } else {
    Serial.println("  NORMAL");
    Serial.println("   Continue monitoring");
  }
  
  Serial.println("========================================\n");
}

/**
 * Show status
 */
void showStatus() {
  Serial.println("========================================");
  Serial.println("  SYSTEM STATUS");
  Serial.println("========================================");
  
  Serial.print("Calibration: ");
  Serial.println(isCalibrated ? " CALIBRATED" : " NOT CALIBRATED");
  
  if (isCalibrated) {
    Serial.print("  Baseline: ");
    Serial.print((int)pressureBaseline);
    Serial.println(" (ADC)");
  }
  
  Serial.println("\nSensors: 3x FSR-402");
  Serial.print("  Sensor 1: GPIO ");
  Serial.println(PRESSURE_SENSOR_1_PIN);
  Serial.print("  Sensor 2: GPIO ");
  Serial.println(PRESSURE_SENSOR_2_PIN);
  Serial.print("  Sensor 3: GPIO ");
  Serial.println(PRESSURE_SENSOR_3_PIN);
  
  Serial.println("========================================\n");
}

/**
 * Show menu
 */
void showMenu() {
  Serial.println("========================================");
  Serial.println("COMMANDS:");
  Serial.println("========================================");
  Serial.println("  C - Calibrate baseline");
  Serial.println("  L - Load from EEPROM");
  Serial.println("  R - Raw readings (detailed)");
  Serial.println("  T - Test swelling detection");
  Serial.println("  S - Show status");
  Serial.println("  X - Clear calibration");
  Serial.println("  M - Show menu");
  Serial.println("========================================");
  
  if (isCalibrated) {
    Serial.println("Status: ✓ CALIBRATED");
  } else {
    Serial.println("Status:  NOT CALIBRATED (showing raw)");
  }
  
  Serial.println("========================================\n");
}
