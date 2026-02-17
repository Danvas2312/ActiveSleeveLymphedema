#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// SYSTEM CONFIGURATION
// ============================================================================

// Calibration Settings
#define CALIBRATION_SAMPLES 20          // Number of samples for baseline averaging
#define CALIBRATION_DELAY_MS 100        // Delay between calibration samples (ms)

// Main Loop Timing
#define SENSOR_READ_INTERVAL_MS 1000    // How often to read sensors (1 second)
#define MONITORING_INTERVAL_MS 5000     // How often to check for swelling (5 seconds)

// ============================================================================
// TEMPERATURE THRESHOLDS (from your document [1])
// ============================================================================

// Normal skin temperature range: 30-34°C
#define TEMP_NORMAL_MIN 30.0            // Minimum normal temperature (°C)
#define TEMP_NORMAL_MAX 34.0            // Maximum normal temperature (°C)

// Temperature deviation thresholds (relative to baseline)
#define TEMP_THRESHOLD_POSSIBLE 1.0     // +1.0°C = Possible swelling
#define TEMP_THRESHOLD_LIKELY 2.0       // +2.0°C = Likely swelling
#define TEMP_THRESHOLD_SIGNIFICANT 3.0  // +3.0°C = Significant swelling

// Active threshold for triggering compression
#define TEMP_TRIGGER_THRESHOLD TEMP_THRESHOLD_LIKELY  // Use "likely" as trigger point

// ============================================================================
// PRESSURE THRESHOLDS
// ============================================================================

// Target compression pressure
#define TARGET_PRESSURE_MMHG 40.0       // Target therapeutic pressure (mmHg)

// Pressure sensor calibration (will be determined during testing)
#define PRESSURE_SENSOR_MIN_READING 0   // ADC value at 0 mmHg
#define PRESSURE_SENSOR_MAX_READING 4095 // ADC value at max pressure (12-bit ADC)
#define PRESSURE_MAX_MMHG 100.0         // Maximum pressure the sensor can read

// Swelling detection via pressure (Option 1: Increased contact pressure)
// When arm swells, sleeve gets tighter, FSR reads higher
#define PRESSURE_DEVIATION_THRESHOLD 5.0 // mmHg above baseline suggests swelling

// Safety limits
#define PRESSURE_SAFETY_MAX 60.0        // Never exceed this pressure (mmHg)
#define PRESSURE_TOLERANCE 5.0          // Acceptable deviation from target (mmHg)

// ============================================================================
// COMPRESSION CYCLE PARAMETERS
// ============================================================================

// Three-phase cycle: INFLATE → HOLD → RELEASE
#define INFLATE_TIMEOUT_MS 30000        // Max time to reach target pressure (30 sec)
#define HOLD_DURATION_MS 10000          // Hold pressure for 10 seconds
#define RELEASE_DURATION_MS 5000        // Release pressure for 5 seconds
#define COOLDOWN_DURATION_MS 60000      // Wait 60 seconds before next cycle

// Pump control - How the cycle acts
#define PUMP_PWM_FREQUENCY 5000         // PWM frequency for pump (Hz)
#define PUMP_PWM_RESOLUTION 8           // 8-bit resolution (0-255)
#define PUMP_PWM_INITIAL 180            // Starting PWM duty cycle (0-255)
#define PUMP_PWM_MAX 255                // Maximum PWM duty cycle

// ============================================================================
// DATA SMOOTHING & FILTERING
// ============================================================================

// Moving average filter sizes
#define TEMP_FILTER_SIZE 5              // Average last 5 temperature readings
#define PRESSURE_FILTER_SIZE 3          // Average last 3 pressure readings per sensor

// Noise rejection
#define TEMP_MAX_CHANGE_PER_READING 2.0 // Reject readings that change more than 2°C
#define PRESSURE_MAX_CHANGE 10.0        // Reject pressure spikes > 10 mmHg

// ============================================================================
// BLUETOOTH CONFIGURATION
// ============================================================================

#define BT_DEVICE_NAME "LymphSleeve_G10" // Bluetooth device name
#define BT_BAUD_RATE 115200              // Serial baud rate
#define BT_UPDATE_INTERVAL_MS 2000       // Send BT updates every 2 seconds

// ============================================================================
// EEPROM ADDRESSES (for storing baseline values)
// ============================================================================

#define EEPROM_SIZE 64                   // Allocate 64 bytes for EEPROM
#define ADDR_TEMP_BASELINE 0             // Address for temperature baseline (4 bytes)
#define ADDR_PRESSURE1_BASELINE 4        // Address for pressure sensor 1 (4 bytes)
#define ADDR_PRESSURE2_BASELINE 8        // Address for pressure sensor 2 (4 bytes)
#define ADDR_PRESSURE3_BASELINE 12       // Address for pressure sensor 3 (4 bytes)
#define ADDR_CALIBRATION_FLAG 16         // Flag to check if calibration exists (1 byte)

// Calibration validity flag
#define CALIBRATION_VALID_FLAG 0xAA      // Magic number indicating valid calibration

// ============================================================================
// DEBUG & TESTING FLAGS
// ============================================================================

// Set to 1 to enable debug output to Serial Monitor
#define DEBUG_MODE 1

// Individual debug flags
#define DEBUG_SENSORS 1                  // Print sensor readings
#define DEBUG_CALIBRATION 1              // Print calibration process
#define DEBUG_DECISION_LOGIC 1           // Print swelling detection decisions
#define DEBUG_PNEUMATICS 1               // Print pump/valve control
#define DEBUG_STATE_MACHINE 1            // Print state transitions

// ============================================================================
// HARDWARE SAFETY
// ============================================================================

#define WATCHDOG_TIMEOUT_MS 8000         // Reset if loop hangs for 8 seconds
#define MAX_CONSECUTIVE_ERRORS 3         // Max sensor read errors before safe shutdown

#endif // CONFIG_H
