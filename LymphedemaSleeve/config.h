// config.h
// Central configuration file for LymphedemaSleeve
//
// PURPOSE: All tunable constants live here.

#ifndef CONFIG_H
#define CONFIG_H

// ========================================
// TEMPERATURE THRESHOLDS
// Source: Design Specifications Document, Section 2
// These are deviations ABOVE the patient's baseline
// ========================================
#define TEMP_THRESHOLD_POSSIBLE    1.0  // +1.0°C above baseline = possible swelling
#define TEMP_THRESHOLD_LIKELY      2.0  // +2.0°C above baseline = likely swelling
#define TEMP_THRESHOLD_SIGNIFICANT 3.0  // +3.0°C above baseline = significant swelling

// Temperature Normal Range (for baseline reference)
#define TEMP_NORMAL_MIN  30.0  // Minimum normal skin temperature (Celsius)
#define TEMP_NORMAL_MAX  34.0  // Maximum normal skin temperature (Celsius)

// Temperature Filter Settings (for moving average)
#define TEMP_FILTER_SIZE        10   // Number of samples for temperature smoothing
#define SENSOR_READ_INTERVAL_MS 500  // Read sensors every 500ms (2 Hz)

// ========================================
// PRESSURE THRESHOLDS
// Logic: Swelling = tighter fit = HIGHER pressure than baseline
// Units: Raw ADC value deviation (0-4095 on ESP32's 12-bit ADC)
// ========================================
#define PRESSURE_THRESHOLD_PERCENT   0.10  // 10% above baseline triggers detection
#define PRESSURE_DEVIATION_THRESHOLD 200   // Absolute ADC deviation that flags swelling
                                           // Tune this after physical calibration

// ========================================
// CALIBRATION SETTINGS
// ========================================
#define CALIBRATION_SAMPLES  20   // Number of samples to average for baseline
                                  // Matches Figure 7 in your design doc
#define CALIBRATION_DELAY_MS 150  // Milliseconds between each calibration sample
                                  // 20 samples x 150ms = 3 seconds total

// ========================================
// COMPRESSION TARGET
// Source: Design Specifications Document, Section 5b
// ========================================
#define TARGET_PRESSURE_MMHG 40  // Target therapeutic compression pressure

// ========================================
// SENSOR SMOOTHING
// ========================================
#define SMOOTH_SAMPLES  5   // Number of readings to average per sensor cycle
#define SMOOTH_DELAY_MS 10  // Milliseconds between smoothing samples

// Moving average buffer size for pressure sensors
#define MOVING_AVG_SIZE 5

// ========================================
// EEPROM STORAGE
// Used to persist baseline calibration across power cycles
// Address map:
//   0-3   : temperature baseline (float, 4 bytes)
//   4-7   : pressure baseline (float, 4 bytes)
//   8-15  : reserved for future use
//   16    : calibration valid flag
// ========================================
#define EEPROM_SIZE             32     // Total bytes allocated
#define ADDR_TEMP_BASELINE       0     // EEPROM address for temperature baseline float
#define ADDR_PRESSURE_BASELINE   4     // EEPROM address for pressure baseline float
#define ADDR_CALIBRATION_FLAG   16     // EEPROM address for valid flag byte
#define CALIBRATION_VALID_FLAG  0xAB   // Magic number — confirms valid calibration exists

// ========================================
// COOLDOWN PERIOD
// After a compression cycle, wait before monitoring again
// ========================================
#define COOLDOWN_DURATION_MS 30000  // 30 seconds cooldown after each compression

// ========================================
// TRIGGER LOGIC
// OR logic: compression fires if EITHER sensor exceeds threshold
// ========================================
#define TRIGGER_LOGIC_OR  true   // true  = OR  (either sensor alone triggers)
                                 // false = AND (both sensors must trigger)

// ========================================
// ESP32 ADC SETTINGS
// The ESP32 has a 12-bit ADC (values 0-4095)
// ========================================
#define ADC_MAX_VALUE        4095  // 12-bit ADC max
#define ADC_REFERENCE_VOLTAGE 3.3  // Volts

// ========================================
// SERIAL MONITOR
// ========================================
#define SERIAL_BAUD_RATE  115200  // Must match Serial.begin() in .ino file
#define PRINT_INTERVAL_MS 1000    // Print sensor readings every 1 second

#endif // CONFIG_H
