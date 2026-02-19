/*
 * PneumaticsTest_Standalone.ino
 * Person B - Phase 1 Pneumatic Control Test
 * Group 10 - BMEG Biomedical Senior Design
 *
 * SINGLE FILE VERSION - No external dependencies
 *
 * PURPOSE: Test pump and solenoid control for lymphedema compression sleeve
 *          All 3 bladders share one pump line (inflate together)
 *          Natural physics causes distal (wrist) bladder to fill first
 *
 * HARDWARE REQUIREMENTS:
 * - ESP32 development board
 * - TB6612FNG motor driver
 * - VN2708PM 6V air pump
 * - 6V normally-closed solenoid valve
 * - 6V power supply (1A minimum)
 * - Flyback diodes across pump and solenoid
 *
 * WIRING CHECKLIST:
 * [ ] TB6612FNG VMOT  --> 6V power supply positive
 * [ ] TB6612FNG VCC   --> ESP32 3.3V (logic power)
 * [ ] TB6612FNG GND   --> Common ground (ESP32 + power supply)
 * [ ] Pump wires      --> TB6612FNG A01, A02
 * [ ] Solenoid wires  --> TB6612FNG B01, B02
 * [ ] ESP32 GPIO pins --> TB6612FNG control pins (see PIN DEFINITIONS below)
 * [ ] Flyback diodes  --> 1N4007 across motor terminals (cathode to positive)
 *
 * SERIAL COMMANDS (type in Serial Monitor at 115200 baud):
 *   p<speed>  - Set pump speed (0-255), e.g., p100, p200
 *   p0        - Stop pump
 *   so        - Solenoid open (vent/release air)
 *   sc        - Solenoid close (seal/hold air)
 *   s         - Print status
 *   e         - Emergency stop (pump off + solenoid open)
 *   cycle     - Run one compression cycle (inflate->hold->release)
 *   auto      - Run continuous compression cycles
 *   stop      - Stop auto mode
 *   h         - Help menu
 *
 * TESTING PROCEDURE:
 * 1. Upload code WITHOUT hardware connected
 * 2. Open Serial Monitor, verify commands respond
 * 3. Power off ESP32
 * 4. Wire hardware per checklist
 * 5. Power on, start with low speed: "p50"
 * 6. Gradually increase: "p100", "p150", "p200"
 * 7. Test solenoid: "so", then "sc"
 * 8. Run single cycle: "cycle"
 * 9. If all good, run auto mode: "auto"
 */

// ============================================================================
// PIN DEFINITIONS (from pins.h)
// ============================================================================
// Motor Driver TB6612FNG - Channel A (Pump)
#define AIN1 25   // Pump direction control 1
#define AIN2 26   // Pump direction control 2
#define PWMA 27   // Pump speed control (PWM)

// Motor Driver TB6612FNG - Channel B (Solenoid)
#define BIN1 32   // Solenoid control 1
#define BIN2 33   // Solenoid control 2
#define PWMB 14   // Solenoid control (PWM)

// Status LED
#define LED_BUILTIN 2

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================
// Compression Cycle Timing (adjust these after testing with physical sleeve)
#define INFLATE_TIME_MS     5000    // How long to run pump (5 seconds)
#define HOLD_TIME_MS        3000    // How long to hold pressure (3 seconds)
#define RELEASE_TIME_MS     4000    // How long to vent air (4 seconds)
#define CYCLE_COOLDOWN_MS   10000   // Wait between cycles (10 seconds)

// Pump Speed Settings
#define PUMP_SPEED_LOW      100     // Gentle inflation
#define PUMP_SPEED_MEDIUM   150     // Normal operation
#define PUMP_SPEED_HIGH     200     // Aggressive inflation (use with caution)
#define PUMP_RAMP_TIME_MS   500     // Milliseconds to ramp pump speed changes

// Serial
#define SERIAL_BAUD_RATE    115200

// ============================================================================
// GLOBAL STATE VARIABLES
// ============================================================================
static int current_pump_speed = 0;        // Current PWM value (0-255)
static bool solenoid_open_state = false;  // true = valve open (venting)
static bool auto_mode_active = false;     // true = running continuous cycles
static unsigned long last_cycle_time = 0; // Track timing for auto mode

// ============================================================================
// PNEUMATIC CONTROL FUNCTIONS
// ============================================================================

/**
 * Initialize motor driver pins
 * Call once in setup()
 */
void pneumatics_init() {
  Serial.println("[PNEUMATICS] Initializing motor driver...");

  // Configure pump pins (Motor A)
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);

  // Configure solenoid pins (Motor B)
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);

  // Set safe initial state: everything OFF
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, 0);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, 0);

  current_pump_speed = 0;
  solenoid_open_state = false;

  Serial.println("[PNEUMATICS] TB6612FNG initialized");
  Serial.println("[PNEUMATICS] Initial state: Pump OFF | Solenoid CLOSED");
}

/**
 * Start pump at specified speed with ramping
 * speed: 0-255 (PWM duty cycle)
 */
void pump_start(int speed) {
  // Bounds check
  if (speed < 0) speed = 0;
  if (speed > 255) speed = 255;

  Serial.print("[PUMP] Starting at speed ");
  Serial.println(speed);

  // Set direction: forward only (IN1=HIGH, IN2=LOW)
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);

  // Ramp up gradually if starting from stop
  if (current_pump_speed == 0 && speed > 0) {
    pump_ramp_to(speed, PUMP_RAMP_TIME_MS);
  } else {
    analogWrite(PWMA, speed);
    current_pump_speed = speed;
  }
}

/**
 * Gradually change pump speed over time
 * Prevents current spikes and mechanical stress
 */
void pump_ramp_to(int target_speed, int ramp_time_ms) {
  // Bounds check
  if (target_speed < 0) target_speed = 0;
  if (target_speed > 255) target_speed = 255;

  int start_speed = current_pump_speed;
  int speed_delta = target_speed - start_speed;

  if (speed_delta == 0) return;  // Already at target

  Serial.print("[PUMP] Ramping from ");
  Serial.print(start_speed);
  Serial.print(" to ");
  Serial.print(target_speed);
  Serial.print(" over ");
  Serial.print(ramp_time_ms);
  Serial.println("ms");

  unsigned long start_time = millis();
  unsigned long elapsed;

  while ((elapsed = millis() - start_time) < ramp_time_ms) {
    float progress = (float)elapsed / (float)ramp_time_ms;
    int new_speed = start_speed + (int)(speed_delta * progress);
    
    analogWrite(PWMA, new_speed);
    current_pump_speed = new_speed;
    
    delay(10);  // Small delay for smooth ramping
  }

  // Ensure exact target is reached
  analogWrite(PWMA, target_speed);
  current_pump_speed = target_speed;
}

/**
 * Stop the pump with safe ramping
 */
void pump_stop() {
  if (current_pump_speed == 0) {
    Serial.println("[PUMP] Already stopped");
    return;
  }

  Serial.println("[PUMP] Stopping...");
  pump_ramp_to(0, 300);  // 300ms ramp down

  // Turn off direction pins
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, 0);
  current_pump_speed = 0;

  Serial.println("[PUMP] Stopped");
}

/**
 * Check if pump is running
 */
bool pump_is_running() {
  return (current_pump_speed > 0);
}

/**
 * Get current pump speed
 */
int pump_get_speed() {
  return current_pump_speed;
}

/**
 * Open solenoid valve (releases air pressure)
 * Normally-closed valve: needs power to OPEN
 */
void solenoid_open() {
  if (solenoid_open_state) {
    Serial.println("[SOLENOID] Already open");
    return;
  }

  Serial.println("[SOLENOID] Opening valve (venting pressure)");

  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, 255);  // Full power
  solenoid_open_state = true;
}

/**
 * Close solenoid valve (traps air pressure)
 */
void solenoid_close() {
  if (!solenoid_open_state) {
    Serial.println("[SOLENOID] Already closed");
    return;
  }

  Serial.println("[SOLENOID] Closing valve (sealing pressure)");

  // Remove power - valve returns to normally-closed state
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, 0);
  solenoid_open_state = false;
}

/**
 * Check if solenoid is open
 */
bool solenoid_is_open() {
  return solenoid_open_state;
}

/**
 * Emergency stop: pump off + solenoid open
 */
void pneumatics_emergency_stop() {
  Serial.println("========================================");
  Serial.println("[EMERGENCY STOP] Shutting down!");
  Serial.println("========================================");

  // Stop pump immediately
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, 0);
  current_pump_speed = 0;

  // Open solenoid to vent
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, 255);
  solenoid_open_state = true;

  // Stop auto mode if running
  auto_mode_active = false;

  Serial.println("[EMERGENCY STOP] All pressure vented, system safe");
}

/**
 * Print current pneumatic system status
 */
void pneumatics_print_status() {
  Serial.println("========================================");
  Serial.println("[PNEUMATICS STATUS]");
  Serial.print("  Pump: ");
  if (current_pump_speed > 0) {
    Serial.print("RUNNING at speed ");
    Serial.print(current_pump_speed);
    Serial.println(" PWM");
  } else {
    Serial.println("STOPPED");
  }
  
  Serial.print("  Solenoid: ");
  Serial.println(solenoid_open_state ? "OPEN (venting)" : "CLOSED (sealed)");
  
  Serial.print("  Auto Mode: ");
  Serial.println(auto_mode_active ? "ACTIVE" : "INACTIVE");
  
  Serial.println("========================================");
}

// ============================================================================
// COMPRESSION CYCLE FUNCTIONS
// ============================================================================

/**
 * Run one complete compression cycle
 * INFLATE (5s) -> HOLD (3s) -> RELEASE (4s)
 * 
 * This simulates therapeutic compression for lymphedema treatment.
 * The bladders fill from distal (wrist) to proximal (shoulder) naturally
 * due to tubing resistance.
 */
void run_compression_cycle() {
  Serial.println("\n========================================");
  Serial.println("[CYCLE] Starting Compression Cycle");
  Serial.println("========================================\n");

  // PHASE 1: INFLATE
  Serial.println("[CYCLE] Phase 1: INFLATING bladders");
  Serial.print("[CYCLE] Running pump for ");
  Serial.print(INFLATE_TIME_MS / 1000);
  Serial.println(" seconds...");
  
  solenoid_close();  // Ensure valve is closed
  delay(100);
  
  pump_start(PUMP_SPEED_MEDIUM);  // Start inflating
  
  // Visual countdown during inflation
  unsigned long inflate_start = millis();
  while (millis() - inflate_start < INFLATE_TIME_MS) {
    if ((millis() - inflate_start) % 1000 == 0) {
      int seconds_left = (INFLATE_TIME_MS - (millis() - inflate_start)) / 1000;
      Serial.print("  ... ");
      Serial.print(seconds_left);
      Serial.println("s remaining");
      delay(10);  // Prevent multiple prints in same millisecond
    }
    delay(50);
  }
  
  pump_stop();
  Serial.println("[CYCLE] Phase 1 complete: Bladders inflated\n");

  // PHASE 2: HOLD PRESSURE
  Serial.println("[CYCLE] Phase 2: HOLDING pressure");
  Serial.print("[CYCLE] Maintaining compression for ");
  Serial.print(HOLD_TIME_MS / 1000);
  Serial.println(" seconds...");
  
  delay(HOLD_TIME_MS);
  
  Serial.println("[CYCLE] Phase 2 complete: Pressure held\n");

  // PHASE 3: RELEASE
  Serial.println("[CYCLE] Phase 3: RELEASING pressure");
  Serial.print("[CYCLE] Venting air for ");
  Serial.print(RELEASE_TIME_MS / 1000);
  Serial.println(" seconds...");
  
  solenoid_open();  // Open valve to vent
  
  delay(RELEASE_TIME_MS);
  
  solenoid_close();  // Close valve
  Serial.println("[CYCLE] Phase 3 complete: Pressure released\n");

  Serial.println("========================================");
  Serial.println("[CYCLE] Compression cycle COMPLETE");
  Serial.println("========================================\n");

  last_cycle_time = millis();  // Track for auto mode
}

/**
 * Auto mode: Run continuous compression cycles with cooldown between
 */
void run_auto_mode() {
  if (!auto_mode_active) return;

  // Check if enough time has passed since last cycle
  if (millis() - last_cycle_time >= CYCLE_COOLDOWN_MS) {
    run_compression_cycle();
    
    if (auto_mode_active) {  // Check again in case user stopped during cycle
      Serial.print("[AUTO MODE] Next cycle in ");
      Serial.print(CYCLE_COOLDOWN_MS / 1000);
      Serial.println(" seconds...\n");
    }
  }
}

// ============================================================================
// COMMAND PROCESSING
// ============================================================================

void process_command(String cmd) {
  Serial.print("> ");
  Serial.println(cmd);

  // PUMP COMMANDS: p<speed>
  if (cmd.startsWith("p")) {
    String speedStr = cmd.substring(1);
    int speed = speedStr.toInt();

    if (speed < 0 || speed > 255) {
      Serial.println("[ERROR] Speed must be 0-255");
      return;
    }

    if (speed == 0) {
      pump_stop();
    } else {
      pump_start(speed);
    }
    return;
  }

  // SOLENOID COMMANDS
  if (cmd == "so") {
    solenoid_open();
    return;
  }
  if (cmd == "sc") {
    solenoid_close();
    return;
  }

  // STATUS
  if (cmd == "s") {
    pneumatics_print_status();
    return;
  }

  // EMERGENCY STOP
  if (cmd == "e") {
    pneumatics_emergency_stop();
    return;
  }

  // SINGLE CYCLE
  if (cmd == "cycle") {
    auto_mode_active = false;  // Disable auto if running
    run_compression_cycle();
    return;
  }

  // AUTO MODE
  if (cmd == "auto") {
    Serial.println("[AUTO MODE] Starting continuous cycles...");
    Serial.println("[AUTO MODE] Type 'stop' to end");
    auto_mode_active = true;
    last_cycle_time = 0;  // Trigger immediate first cycle
    return;
  }

  // STOP AUTO MODE
  if (cmd == "stop") {
    if (auto_mode_active) {
      Serial.println("[AUTO MODE] Stopping...");
      auto_mode_active = false;
      pump_stop();
      solenoid_close();
    } else {
      Serial.println("[INFO] Auto mode is not active");
    }
    return;
  }

  // HELP
  if (cmd == "h" || cmd == "help" || cmd == "?") {
    print_menu();
    return;
  }

  // Unknown command
  Serial.println("[ERROR] Unknown command. Type 'h' for help.");
}

void print_menu() {
  Serial.println("\n========================================");
  Serial.println("COMMAND MENU");
  Serial.println("========================================");
  Serial.println("Basic Control:");
  Serial.println("  p<0-255>  Set pump speed (e.g., p100, p200)");
  Serial.println("  p0        Stop pump");
  Serial.println("  so        Open solenoid (vent air)");
  Serial.println("  sc        Close solenoid (seal air)");
  Serial.println("");
  Serial.println("Compression Cycles:");
  Serial.println("  cycle     Run one compression cycle");
  Serial.println("  auto      Start continuous cycles");
  Serial.println("  stop      Stop auto mode");
  Serial.println("");
  Serial.println("Status & Safety:");
  Serial.println("  s         Print system status");
  Serial.println("  e         Emergency stop");
  Serial.println("  h         Show this menu");
  Serial.println("========================================\n");
}

// ============================================================================
// ARDUINO SETUP
// ============================================================================

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);

  Serial.println("\n\n========================================");
  Serial.println("  PNEUMATICS CONTROL SYSTEM");
  Serial.println("  Person B - Phase 1 Test");
  Serial.println("  Group 10 - Biomedical Senior Design");
  Serial.println("========================================\n");

  // Initialize LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Initialize pneumatic system
  pneumatics_init();

  // Print command menu
  print_menu();

  // Startup blink sequence
  Serial.println("[READY] Type commands and press Enter\n");
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
  }
}

// ============================================================================
// ARDUINO MAIN LOOP
// ============================================================================

void loop() {
  // Process serial commands
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    process_command(command);
  }

  // Handle auto mode
  if (auto_mode_active) {
    run_auto_mode();
  }

  // Heartbeat LED blink
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 1000) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    lastBlink = millis();
  }
}
