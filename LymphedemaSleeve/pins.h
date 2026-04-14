// pins.h
// Pin definitions for LymphedemaSleeve project

#ifndef PINS_H
#define PINS_H

// ========================================
// MOTOR DRIVER PINS (TB6612FNG)
// ========================================
// Motor A = Air Pump
#define AIN1 27  // Pump direction control 1
#define AIN2 14  // Pump direction control 2
#define PWMA 12  // Pump speed control (PWM)

// Motor B = Solenoid Valve
#define BIN1 33  // Solenoid control 1
#define BIN2 25  // Solenoid control 2
#define PWMB 26  // Solenoid control (PWM)

// TB6612 Standby pin — MUST be HIGH to enable motor outputs.
// If STBY is LOW or floating, both motor channels are disabled
// regardless of any other signal. Wire this pin to ESP32 GPIO 32.
#define STBY_PIN 32

// ========================================
// PRESSURE SENSOR PINS (FSR 402)
//
// CURRENT HARDWARE: One physical FSR 402 sensor wired to all
// three pins (GPIO 34, 35, 36). Readings from all 3 pins are
// averaged together and treated as a single sensor value.
// See SINGLE_SENSOR_MODE in config.h.
//
// FUTURE EXPANSION: When 3 separate sensors are installed,
// set SINGLE_SENSOR_MODE false in config.h — no pin changes needed.
//
// NOTE: GPIO 34, 35, 36 are INPUT-ONLY on ESP32.
//       No internal pull-up resistors — always use external 10kΩ pull-down.
// ========================================
#define PRESSURE_PIN_1        34  // FSR shared (proximal when expanded)
#define PRESSURE_PIN_2        35  // FSR shared (middle when expanded)
#define PRESSURE_PIN_3        36  // FSR shared (distal when expanded)
 
// Aliases used in pressure_sensors.ino
#define PRESSURE_SENSOR_1_PIN 34
#define PRESSURE_SENSOR_2_PIN 35
#define PRESSURE_SENSOR_3_PIN 36
 
// ========================================
// I2C PINS (TMP117 Temperature Sensor)
// ========================================
#define SDA_PIN     21  // I2C Data line
#define SCL_PIN     22  // I2C Clock line
#define I2C_SDA_PIN 21  // Alternative name (same as SDA_PIN)
#define I2C_SCL_PIN 22  // Alternative name (same as SCL_PIN)

// ========================================
// OTHER PINS
// ========================================
#define LED_BUILTIN 2  // Built-in LED (for status indication)

// ========================================
// REMOVED PINS
// SODIUM_PIN removed — sodium sensor dropped from design (Nov 2025)
// ========================================

#endif // PINS_H
