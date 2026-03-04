// pins.h
// Pin definitions for LymphedemaSleeve project

#ifndef PINS_H
#define PINS_H

// ========================================
// MOTOR DRIVER PINS (TB6612FNG)
// ========================================
// Motor A = Air Pump
#define AIN1 25  // Pump direction control 1
#define AIN2 26  // Pump direction control 2
#define PWMA 27  // Pump speed control (PWM)

// Motor B = Solenoid Valve
#define BIN1 32  // Solenoid control 1
#define BIN2 33  // Solenoid control 2
#define PWMB 14  // Solenoid control (PWM)

// ========================================
// PRESSURE SENSOR PINS (FSR 402)
// NOTE: GPIO 34, 35, 36 are INPUT-ONLY on ESP32
//       They have no internal pull-up resistors
//       Always use external 10kΩ pull-down resistor
// ========================================
#define PRESSURE_PIN_1        34  // Proximal bladder (shoulder)
#define PRESSURE_PIN_2        35  // Middle bladder   (forearm)
#define PRESSURE_PIN_3        36  // Distal bladder   (wrist)

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
