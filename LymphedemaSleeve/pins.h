// pins.h
// Pin definitions for LymphedemaSleeve project
// Group 10 - Biomedical Senior Design

#ifndef PINS_H
#define PINS_H

// ========================================
// MOTOR DRIVER PINS (TB6612FNG)
// ========================================
// Motor A = Air Pump
#define AIN1 25   // Pump direction control 1
#define AIN2 26   // Pump direction control 2
#define PWMA 27   // Pump speed control (PWM)

// Motor B = Solenoid Valve
#define BIN1 32   // Solenoid control 1
#define BIN2 33   // Solenoid control 2
#define PWMB 14   // Solenoid control (PWM)

// ========================================
// SENSOR PINS
// ========================================
// Analog Pressure Sensors (FSR)
#define PRESSURE_PIN_1 34   // Proximal bladder
#define PRESSURE_PIN_2 35   // Middle bladder
#define PRESSURE_PIN_3 36   // Distal bladder

// ========================================
// I2C PINS (TMP117 Temperature Sensor)
// ========================================
#define SDA_PIN 21       // I2C Data line
#define SCL_PIN 22       // I2C Clock line
#define I2C_SDA_PIN 21   // Alternative name (same as SDA_PIN)
#define I2C_SCL_PIN 22   // Alternative name (same as SCL_PIN)

// ========================================
// OTHER PINS
// ========================================
#define LED_BUILTIN 2   // Built-in LED (for status indication)

#endif // PINS_H


