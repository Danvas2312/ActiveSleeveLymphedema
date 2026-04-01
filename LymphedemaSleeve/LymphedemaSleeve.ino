/*
 * LymphedemaSleeve.ino
 * Main Firmware — Full Integration
 * Group 10 — BMEG 48103 Biomedical Senior Design
 *
 * State Machine:
 *   CALIBRATE  Capture baseline temperature AND pressure; save both to EEPROM.
 *              If a valid calibration already exists in EEPROM, skip straight
 *              to MONITOR so the sleeve is ready immediately after power-on.
 *   MONITOR    Read both sensors every 500 ms with moving-average filtering.
 *              Trigger compression if EITHER threshold is exceeded (OR logic).
 *   COMPRESS   Inflate bladders (30 s) -> Hold (15 s) -> Release (15 s).
 *   COOLDOWN   30-second rest before resuming monitoring.
 *
 * Trigger Logic (OR):
 *   - Temperature delta >= +1.0 °C above baseline   (TEMP_THRESHOLD_POSSIBLE)
 *   - Average pressure  >= +200 ADC above baseline  (PRESSURE_DEVIATION_THRESHOLD)
 */

#include <Wire.h>
#include <Adafruit_TMP117.h>
#include <EEPROM.h>
#include "pins.h"
#include "config.h"

// ============================================================================
// TEMPERATURE FILTER (moving average over TEMP_FILTER_SIZE samples)
// ============================================================================
class TemperatureFilter {
private:
  float buf[TEMP_FILTER_SIZE];
  int   idx   = 0;
  int   count = 0;

public:
  TemperatureFilter() {
    for (int i = 0; i < TEMP_FILTER_SIZE; i++) buf[i] = 0.0f;
  }

  void add(float t) {
    buf[idx] = t;
    idx = (idx + 1) % TEMP_FILTER_SIZE;
    if (count < TEMP_FILTER_SIZE) count++;
  }

  float smoothed() const {
    if (count == 0) return 0.0f;
    float s = 0.0f;
    for (int i = 0; i < count; i++) s += buf[i];
    return s / count;
  }

  bool ready() const { return count >= TEMP_FILTER_SIZE; }

  void reset() {
    idx = 0; count = 0;
    for (int i = 0; i < TEMP_FILTER_SIZE; i++) buf[i] = 0.0f;
  }
};

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
Adafruit_TMP117  tmp117;
TemperatureFilter tempFilter;

// Pressure moving-average buffers (one per FSR sensor)
float pressureBuf1[MOVING_AVG_SIZE] = {0};
float pressureBuf2[MOVING_AVG_SIZE] = {0};
float pressureBuf3[MOVING_AVG_SIZE] = {0};
int   pressureBufIdx = 0;

// Baselines (loaded from EEPROM or captured during calibration)
float tempBaseline     = 0.0f;
float pressureBaseline = 0.0f;

// Current filtered readings
float currentTemp     = 0.0f;
float currentPressure = 0.0f;

// OR-logic trigger flags
bool tempTriggered     = false;
bool pressureTriggered = false;

// Timing
unsigned long cooldownStart  = 0;
unsigned long lastSensorRead = 0;

// ============================================================================
// PRESSURE HELPER — updates circular buffers, returns filtered average
// ============================================================================
float readSmoothedPressure() {
  pressureBuf1[pressureBufIdx] = (float)analogRead(PRESSURE_PIN_1);
  pressureBuf2[pressureBufIdx] = (float)analogRead(PRESSURE_PIN_2);
  pressureBuf3[pressureBufIdx] = (float)analogRead(PRESSURE_PIN_3);
  pressureBufIdx = (pressureBufIdx + 1) % MOVING_AVG_SIZE;

  float s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
  for (int i = 0; i < MOVING_AVG_SIZE; i++) {
    s1 += pressureBuf1[i];
    s2 += pressureBuf2[i];
    s3 += pressureBuf3[i];
  }
  return ((s1 + s2 + s3) / MOVING_AVG_SIZE) / 3.0f;
}

// ============================================================================
// EEPROM HELPERS
// ============================================================================
void saveCalibrationToEEPROM() {
  EEPROM.writeFloat(ADDR_TEMP_BASELINE,     tempBaseline);
  EEPROM.writeFloat(ADDR_PRESSURE_BASELINE, pressureBaseline);
  EEPROM.write(ADDR_CALIBRATION_FLAG, CALIBRATION_VALID_FLAG);
  EEPROM.commit();
  Serial.println("[EEPROM] Calibration saved.");
}

bool loadCalibrationFromEEPROM() {
  if (EEPROM.read(ADDR_CALIBRATION_FLAG) != CALIBRATION_VALID_FLAG) return false;

  float t = EEPROM.readFloat(ADDR_TEMP_BASELINE);
  float p = EEPROM.readFloat(ADDR_PRESSURE_BASELINE);

  // Sanity-check: reject obviously invalid stored values
  if (t < 20.0f || t > 45.0f)           return false;
  if (p < 0.0f  || p > ADC_MAX_VALUE)   return false;

  tempBaseline     = t;
  pressureBaseline = p;
  return true;
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);

  Serial.println("========================================");
  Serial.println("  LymphedemaSleeve - Full Integration");
  Serial.println("  Group 10 - Biomedical Senior Design");
  Serial.println("========================================");

  // Motor driver — safe initial state (pump off, solenoid closed)
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT); pinMode(PWMA, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT); pinMode(PWMB, OUTPUT);
  digitalWrite(AIN1, LOW); digitalWrite(AIN2, LOW); analogWrite(PWMA, 0);
  digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW); analogWrite(PWMB, 0);

  // Status LED
  pinMode(LED_BUILTIN, OUTPUT);

  // ADC + pressure sensor pins (GPIO 34/35/36 are input-only on ESP32)
  analogReadResolution(12);
  pinMode(PRESSURE_PIN_1, INPUT);
  pinMode(PRESSURE_PIN_2, INPUT);
  pinMode(PRESSURE_PIN_3, INPUT);

  // I2C + TMP117
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.print("Initializing TMP117... ");
  if (!tmp117.begin()) {
    Serial.println("FAILED! Check: SDA->GPIO21, SCL->GPIO22, VCC->3.3V");
    while (1) delay(500);
  }
  Serial.println("OK");

  // EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // Attempt to restore a saved calibration so the sleeve is ready immediately
  Serial.print("Checking EEPROM for saved calibration... ");
  if (loadCalibrationFromEEPROM()) {
    Serial.println("found.");
    Serial.print("  Temp baseline:     "); Serial.print(tempBaseline, 2);     Serial.println(" C");
    Serial.print("  Pressure baseline: "); Serial.print((int)pressureBaseline); Serial.println(" ADC");
    Serial.println("  Skipping calibration — entering monitoring.\n");
    currentState = STATE_MONITOR;
  } else {
    Serial.println("none — will calibrate now.");
    currentState = STATE_CALIBRATE;
  }

  Serial.println("Trigger logic: Temperature OR Pressure");
  Serial.println("========================================\n");
}

// ============================================================================
// MAIN LOOP — State Machine
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

  // Heartbeat LED (1 Hz blink)
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 1000) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    lastBlink = millis();
  }
}

// ============================================================================
// STATE: CALIBRATE
// Captures 20-sample baseline for both temperature and pressure, saves to EEPROM
// ============================================================================
void runCalibration() {
  Serial.println("========================================");
  Serial.println("[CALIBRATE] Capturing patient baseline...");
  Serial.println("  Keep sleeve relaxed and arm still.");
  Serial.print("  Taking "); Serial.print(CALIBRATION_SAMPLES);
  Serial.println(" samples (~3 seconds each sensor)...");
  Serial.println("========================================\n");

  // --- Temperature baseline ---
  float tempSum = 0.0f;
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    sensors_event_t ev;
    tmp117.getEvent(&ev);
    tempFilter.add(ev.temperature);
    tempSum += ev.temperature;
    Serial.print("  Temp  "); Serial.print(i + 1); Serial.print("/");
    Serial.print(CALIBRATION_SAMPLES); Serial.print(": ");
    Serial.print(ev.temperature, 2); Serial.println(" C");
    delay(CALIBRATION_DELAY_MS);
  }
  tempBaseline = tempSum / CALIBRATION_SAMPLES;

  // --- Pressure baseline ---
  float pressureSum = 0.0f;
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    float p = readSmoothedPressure();
    pressureSum += p;
    delay(CALIBRATION_DELAY_MS);
  }
  pressureBaseline = pressureSum / CALIBRATION_SAMPLES;

  // Persist both baselines
  saveCalibrationToEEPROM();

  Serial.println();
  Serial.print("[CALIBRATE] Temperature baseline: "); Serial.print(tempBaseline, 2);
  Serial.println(" C");
  Serial.print("[CALIBRATE] Pressure baseline:    "); Serial.print((int)pressureBaseline);
  Serial.println(" ADC");
  Serial.print("[CALIBRATE] Temp trigger at:      >");
  Serial.print(tempBaseline + TEMP_THRESHOLD_POSSIBLE, 2); Serial.println(" C");
  Serial.print("[CALIBRATE] Pressure trigger at:  >");
  Serial.print((int)(pressureBaseline + PRESSURE_DEVIATION_THRESHOLD)); Serial.println(" ADC");
  Serial.println("[CALIBRATE] Complete. Entering monitoring.\n");
}

// ============================================================================
// STATE: MONITOR
// Reads both sensors every SENSOR_READ_INTERVAL_MS; applies OR trigger logic
// ============================================================================
void runMonitor() {
  unsigned long now = millis();
  if (now - lastSensorRead < SENSOR_READ_INTERVAL_MS) return;
  lastSensorRead = now;

  // Temperature — add raw reading to filter, use smoothed value
  sensors_event_t ev;
  tmp117.getEvent(&ev);
  tempFilter.add(ev.temperature);
  currentTemp = tempFilter.smoothed();
  float tempDelta = currentTemp - tempBaseline;

  // Pressure — moving-average across three FSR sensors
  currentPressure = readSmoothedPressure();
  float pressureDelta = currentPressure - pressureBaseline;

  // Serial output
  Serial.print("[MONITOR] Temp: ");
  Serial.print(currentTemp, 2);
  Serial.print(" C (D:");
  if (tempDelta >= 0) Serial.print("+");
  Serial.print(tempDelta, 2);
  Serial.print(") | Pressure: ");
  Serial.print((int)currentPressure);
  Serial.print(" ADC (D:");
  if (pressureDelta >= 0) Serial.print("+");
  Serial.print((int)pressureDelta);
  Serial.print(") | ");

  // OR trigger logic
  tempTriggered     = (tempDelta     >= TEMP_THRESHOLD_POSSIBLE);
  pressureTriggered = (pressureDelta >= PRESSURE_DEVIATION_THRESHOLD);

  if (tempTriggered || pressureTriggered) {
    Serial.println("SWELLING SIGNAL — triggering compression");
    if (tempTriggered)     Serial.println("  -> Temperature threshold exceeded");
    if (pressureTriggered) Serial.println("  -> Pressure threshold exceeded");
    currentState = STATE_COMPRESS;
  } else {
    Serial.println("Normal");
  }
}

// ============================================================================
// STATE: COMPRESS
// Inflate (30 s) -> Hold (15 s) -> Release (15 s) -> safe state
// ============================================================================
void runCompression() {
  Serial.println("\n========================================");
  Serial.println("[COMPRESS] Starting compression cycle");
  Serial.println("========================================");

  // Phase 1: Inflate — pump on, solenoid closed
  Serial.println("[COMPRESS] Phase 1: Inflate (30s)...");
  digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
  analogWrite(PWMA, 200);
  digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW); analogWrite(PWMB, 0);
  delay(30000);

  // Phase 2: Hold — pump off, solenoid still closed
  Serial.println("[COMPRESS] Phase 2: Hold (15s)...");
  analogWrite(PWMA, 0);
  digitalWrite(AIN1, LOW);
  delay(15000);

  // Phase 3: Release — solenoid open
  Serial.println("[COMPRESS] Phase 3: Release (15s)...");
  digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
  analogWrite(PWMB, 255);
  delay(15000);

  // Safe state — all outputs off
  digitalWrite(BIN1, LOW); analogWrite(PWMB, 0);

  Serial.println("[COMPRESS] Cycle complete.\n");
}

// ============================================================================
// STATE: COOLDOWN
// Waits COOLDOWN_DURATION_MS before returning to monitoring
// ============================================================================
void runCooldown() {
  unsigned long elapsed = millis() - cooldownStart;
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 5000) {
    int remaining = (int)((COOLDOWN_DURATION_MS - elapsed) / 1000);
    if (remaining < 0) remaining = 0;
    Serial.print("[COOLDOWN] Resuming in "); Serial.print(remaining); Serial.println("s...");
    lastPrint = millis();
  }
  if (elapsed >= COOLDOWN_DURATION_MS) {
    Serial.println("[COOLDOWN] Complete. Returning to monitoring.\n");
    currentState = STATE_MONITOR;
  }
}
