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


