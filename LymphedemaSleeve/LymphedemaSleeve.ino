/*
 * LymphedemaSleeve.ino
 * Main Firmware 
 * ============================================================
 *
 * PRESSURE SENSOR NOTE (current hardware config):
 *   One physical FSR 402 sensor is connected to all three pins
 *   (GPIO 34, 35, 36). The three readings are averaged together
 *   and treated as a single sensor value. This keeps the 3-pin
 *   wiring structure intact for future expansion to 3 separate
 *   sensors — just set SINGLE_SENSOR_MODE false in config.h.
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
 *
 * Serial Commands (115200 baud):
 *   e - Emergency stop (pump off + solenoid open + halt)
 *   r - Resume monitoring after emergency stop
 *   s - Print current system status
 *   h - Show this help menu
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
#define BYPASS_PRESSURE_SENSORS false

// ============================================================================
// mmHg CALIBRATION TABLE
// ============================================================================
// Replace ADC values with YOUR actual penny weight measurements.
// Format: {ADC_value, mmHg_value}
// ADC values MUST increase as mmHg increases.
// ============================================================================
float CAL_TABLE[][2] = {
  {0,    0},   // 0 mmHg  → replace with your measured ADC
  {100,  10},  // 10 mmHg → replace with your measured ADC
  {300,  20},  // 20 mmHg → replace with your measured ADC
  {600,  30},  // 30 mmHg → replace with your measured ADC
  {1000, 40}   // 40 mmHg → replace with your measured ADC
};
const int CAL_TABLE_SIZE = 5;

// ============================================================================
// STATE MACHINE
// ============================================================================
enum SystemState {
  STATE_CALIBRATE,
  STATE_MONITOR,
  STATE_COMPRESS,
  STATE_COOLDOWN,
  STATE_STOPPED     // Emergency stop state
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

// Cycle counter
int compressionCycleCount = 0;

// ============================================================================
// HELPER: Read pressure from all 3 pins and average them.
// With one physical sensor wired to all 3 pins the values will be
// nearly identical — averaging cancels any minor ADC noise.
// When 3 separate sensors are installed, this naturally averages them.
// NEW CODE!!!
// ============================================================================
float readAveragePressure() {
  float p1 = analogRead(PRESSURE_PIN_1);
  float p2 = analogRead(PRESSURE_PIN_2);
  float p3 = analogRead(PRESSURE_PIN_3);

#if SINGLE_SENSOR_MODE
  // All pins read the same physical sensor — simple average for noise reduction
  return (p1 + p2 + p3) / 3.0;
#else
  // Three separate sensors — average across the sleeve
  return (p1 + p2 + p3) / 3.0;
#endif
  // Note: the math is identical either way; SINGLE_SENSOR_MODE controls
  // Serial output labelling only (see runMonitor / runCalibration).
}

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
// EMERGENCY STOP
// Immediately stops pump, opens solenoid, halts state machine
// ============================================================================
void emergencyStop() {
  Serial.println("\n========================================");
  Serial.println("[EMERGENCY STOP] Activated!");
  Serial.println("========================================");

  // Stop pump immediately - no ramping
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, 0);

  // Open solenoid to vent all air
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, 255);

  Serial.println("  Pump:     OFF");
  Serial.println("  Solenoid: OPEN (venting air)");
  Serial.println("  State:    HALTED");
  Serial.println("\nType 'r' to resume monitoring.");
  Serial.println("Type 'h' for help menu.");
  Serial.println("========================================\n");

  currentState = STATE_STOPPED;
}

// ============================================================================
// PRINT STATUS
// ============================================================================
void printStatus() {
  Serial.println("========================================");
  Serial.println("[STATUS]");
  Serial.print("  State:       ");
  switch (currentState) {
    case STATE_CALIBRATE: Serial.println("CALIBRATING"); break;
    case STATE_MONITOR:   Serial.println("MONITORING");  break;
    case STATE_COMPRESS:  Serial.println("COMPRESSING"); break;
    case STATE_COOLDOWN:  Serial.println("COOLDOWN");    break;
    case STATE_STOPPED:   Serial.println("STOPPED");     break;
  }
  Serial.print("  Temp:        ");
  Serial.print(currentTemp, 2);
  Serial.print(" C / ");
  Serial.print(toFahrenheit(currentTemp), 2);
  Serial.println(" F");
  Serial.print("  Temp baseline: ");
  Serial.print(tempBaseline, 2);
  Serial.println(" C");
  Serial.print("  Cycles run:  ");
  Serial.println(compressionCycleCount);
  Serial.print("  FSR bypass:  ");
  Serial.println(BYPASS_PRESSURE_SENSORS ? "YES" : "NO");
  Serial.print("  Sensor mode:    ");
  Serial.println(SINGLE_SENSOR_MODE ? "SINGLE (1 FSR on 3 pins)" : "MULTI (3 separate FSRs)");
  Serial.println("========================================\n");
}

// ============================================================================
// PRINT HELP MENU
// ============================================================================
void printHelp() {
  Serial.println("========================================");
  Serial.println("  COMMANDS");
  Serial.println("========================================");
  Serial.println("  e - Emergency stop");
  Serial.println("  r - Resume monitoring");
  Serial.println("  s - Print status");
  Serial.println("  h - Show this menu");
  Serial.println("========================================\n");
}

// ============================================================================
// PROCESS SERIAL COMMANDS
// Can be called from any state
// ============================================================================
void processSerialCommands() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  switch (cmd) {
    case 'e': case 'E':
      emergencyStop();
      break;

    case 'r': case 'R':
      if (currentState == STATE_STOPPED) {
        Serial.println("[RESUME] Returning to monitoring...\n");
        // Close solenoid before resuming
        digitalWrite(BIN1, LOW);
        analogWrite(PWMB, 0);
        currentState = STATE_MONITOR;
        lastSensorRead = 0;
      } else {
        Serial.println("[INFO] System is already running.\n");
      }
      break;

    case 's': case 'S':
      printStatus();
      break;

    case 'h': case 'H': case '?':
      printHelp();
      break;

    default:
      break;
  }
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

  // Pressure pins
  pinMode(PRESSURE_PIN_1, INPUT);
  pinMode(PRESSURE_PIN_2, INPUT);
  pinMode(PRESSURE_PIN_3, INPUT);

  // TMP117
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.print("Initializing TMP117... ");
  if (!tmp117.begin()) {
    Serial.println("FAILED!");
    Serial.println("  Check: SDA->GPIO21, SCL->GPIO22, VCC->3.3V");
    while (1) delay(500);
  }
  Serial.println("OK!");

  EEPROM.begin(EEPROM_SIZE);

  // Print mode and commands
  Serial.println("========================================");
   if (SINGLE_SENSOR_MODE) {
    Serial.println("  PRESSURE: Single FSR on GPIO 34/35/36");
    Serial.println("  (1 sensor wired to all 3 pins, averaged)");
  } else {
    Serial.println("  PRESSURE: 3 separate FSR sensors");
    Serial.println("  GPIO 34 (proximal), 35 (middle), 36 (distal)");
  }
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
  Serial.println("----------------------------------------");
  Serial.println("  COMMANDS: e=stop  r=resume  s=status  h=help");
  Serial.println("========================================\n");

  currentState = STATE_CALIBRATE;
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  // Always check for serial commands regardless of state
  processSerialCommands();

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
    case STATE_STOPPED:
      // Do nothing - wait for 'r' command
      // LED blinks fast to indicate stopped state
      break;
    default:
      currentState = STATE_CALIBRATE;
      break;
  }

  // Heartbeat LED
  // Fast blink = stopped, slow blink = running
  static unsigned long lastBlink = 0;
  unsigned long blinkInterval = (currentState == STATE_STOPPED) ? 200 : 1000;
  if (millis() - lastBlink > blinkInterval) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    lastBlink = millis();
  }
}

// ============================================================================
// STATE: CALIBRATE
// ============================================================================
void runCalibration() {
  Serial.println("========================================");
  Serial.println("[CALIBRATE] Capturing patient baseline...");
  Serial.println("  Keep arm still and relaxed.");
  Serial.println("  Sampling for ~3 seconds...");
  Serial.println("========================================\n");

  float tempSum = 0.0;
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {

    // Check for emergency stop during calibration
    processSerialCommands();
    if (currentState == STATE_STOPPED) return;

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

  if (!BYPASS_PRESSURE_SENSORS) {
    float pressureSum = 0.0;
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
      pressureSum += readAveragePressure();
      delay(CALIBRATION_DELAY_MS);
    }
    pressureBaseline = pressureSum / CALIBRATION_SAMPLES;
// NEW CODE
    Serial.print("[CALIBRATE] Pressure baseline: ");
    Serial.print((int)pressureBaseline);
    Serial.print(" ADC (");
    Serial.print(adcToMmHg(pressureBaseline), 1);
    if (SINGLE_SENSOR_MODE) {
      Serial.println(" mmHg) [single FSR, avg of 3 pins]");
    } else {
      Serial.println(" mmHg) [avg of 3 sensors]");
    }
  } else {
    Serial.println("[CALIBRATE] Pressure: SKIPPED");
  }

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

  Serial.println("[CALIBRATE] Done! Monitoring started.");
  Serial.println("  Type 'e' at any time to emergency stop.\n");
}

// ============================================================================
// STATE: MONITOR
// ============================================================================
void runMonitor() {
  unsigned long now = millis();
  if (now - lastSensorRead < SENSOR_READ_INTERVAL_MS) return;
  lastSensorRead = now;

  sensors_event_t event;
  tmp117.getEvent(&event);
  currentTemp = event.temperature;
  float tempDelta    = currentTemp - tempBaseline;
  float currentTempF = toFahrenheit(currentTemp);

// NEW CODE
  float pressureDelta = 0.0;
  if (!BYPASS_PRESSURE_SENSORS) {
    currentPressure   = readAveragePressure();
    pressureDelta     = currentPressure - pressureBaseline;
    pressureTriggered = (pressureDelta >= PRESSURE_DEVIATION_THRESHOLD);
  }

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
// Inflate 45s → Hold 15s → Release 15s
// Emergency stop checked during each phase
// ============================================================================
void runCompression() {
  compressionCycleCount++;

  Serial.println("\n========================================");
  Serial.print("[COMPRESS] Starting cycle #");
  Serial.println(compressionCycleCount);
  Serial.println("  Type 'e' to emergency stop at any time");
  Serial.println("========================================");

  // ── Phase 1: Inflate ──────────────────────────────────────
  Serial.println("[COMPRESS] Phase 1: Inflate (45s)...");
  digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
  analogWrite(PWMA, 200);
  digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW);
  analogWrite(PWMB, 0);

  unsigned long phaseStart = millis();
  // new line ^

  while (millis() - phaseStart < 45000) {
    processSerialCommands();
    if (currentState == STATE_STOPPED) return;  // exit immediately

    // Print countdown every 5 seconds
    unsigned long elapsed = millis() - phaseStart;
    static unsigned long lastPrint = 0;
    if (elapsed - lastPrint >= 5000) {
      int remaining = (30000 - elapsed) / 1000;
      Serial.print("  Inflate: ");
      Serial.print(remaining);
      Serial.println("s remaining...");
      lastPrint = elapsed;
    }
    delay(100);
  }

  // Stop pump
  digitalWrite(AIN1, LOW); analogWrite(PWMA, 0);
  Serial.println("[COMPRESS] Phase 1 complete.\n");

  // ── Phase 2: Hold ─────────────────────────────────────────
  Serial.println("[COMPRESS] Phase 2: Hold (15s)...");
  phaseStart = millis();
  while (millis() - phaseStart < 15000) {
    processSerialCommands();
    if (currentState == STATE_STOPPED) return;
    delay(100);
  }
  Serial.println("[COMPRESS] Phase 2 complete.\n");

  // ── Phase 3: Release ──────────────────────────────────────
  Serial.println("[COMPRESS] Phase 3: Release (15s)...");
  digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
  analogWrite(PWMB, 255);

  phaseStart = millis();
  while (millis() - phaseStart < 25000) {
    processSerialCommands();
    if (currentState == STATE_STOPPED) return;
    delay(100);
  }

  // Safe state
  digitalWrite(BIN1, LOW); analogWrite(PWMB, 0);
  digitalWrite(AIN1, LOW); analogWrite(PWMA, 0);

  Serial.println("[COMPRESS] Cycle complete.");
  Serial.print("[COMPRESS] Total cycles completed: ");
  Serial.println(compressionCycleCount);
  Serial.println();
}

// ============================================================================
// STATE: COOLDOWN
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
