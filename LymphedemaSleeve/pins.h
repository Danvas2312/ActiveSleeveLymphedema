// pins.h
// Pin definitions for LymphedemaSleeve project
// Group 10 - Biomedical Senior Design
#ifndef PINS_H
#define PINS_H

// ============================================================================
// I2C COMMUNICATION (TMP117 Temperature Sensor)
// ============================================================================
// Note: ESP32 has default I2C pins, but you can reassign them if needed

#define I2C_SDA_PIN 21          // I2C Data line (default for ESP32)
#define I2C_SCL_PIN 22          // I2C Clock line (default for ESP32)

// TMP117 I2C Address
#define TMP117_I2C_ADDRESS 0x48 // Default address (can be 0x49 if ADD0 pin high)

// ============================================================================
// ANALOG INPUTS (Pressure Sensors - FSR 402)
// ============================================================================
// ESP32 has 12-bit ADC (0-4095 range)
// Use ADC1 pins (ADC2 pins conflict with WiFi, avoid them)

#define PRESSURE_SENSOR_1_PIN 36  // VP (ADC1_CH0) - Proximal bladder
#define PRESSURE_SENSOR_2_PIN 39  // VN (ADC1_CH3) - Middle bladder  
#define PRESSURE_SENSOR_3_PIN 34  // ADC1_CH6 - Distal bladder
/ Note: Pins 36, 39, 34, 35 are INPUT ONLY (no pullup/pulldown)
// These are perfect for analog sensors

// ============================================================================
// MOTOR DRIVER (TB6612FNG) - Controls Pump and Solenoid
// ============================================================================

// PUMP CONTROL (Motor A on TB6612FNG)
#define PUMP_PWM_PIN 25         // PWMA - Speed control (PWM capable)
#define PUMP_IN1_PIN 26         // AIN1 - Direction control 1
#define PUMP_IN2_PIN 27         // AIN2 - Direction control 2

// SOLENOID VALVE CONTROL (Motor B on TB6612FNG)  
#define SOLENOID_PWM_PIN 32     // PWMB - Can use as digital ON/OFF
#define SOLENOID_IN1_PIN 33     // BIN1 - Direction control 1
#define SOLENOID_IN2_PIN 14     // BIN2 - Direction control 2

// MOTOR DRIVER STANDBY PIN
#define MOTOR_STBY_PIN 12       // STBY - Set HIGH to enable motors, LOW to disable

// ============================================================================
// STATUS INDICATORS (Optional but Recommended)
// ============================================================================

#define LED_STATUS_PIN 2        // Built-in LED on most ESP32 boards
#define LED_CALIBRATION_PIN 4   // External LED for calibration feedback
#define LED_COMPRESSION_PIN 16  // External LED for compression indication

// ============================================================================
// POWER MANAGEMENT (Optional)
// ============================================================================

#define BATTERY_VOLTAGE_PIN 35  // ADC1_CH7 - For battery monitoring
                                // Connect to voltage divider from battery



