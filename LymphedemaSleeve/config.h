// config.h
// Central configuration file for LymphedemaSleeve
// Group 10 - Biomedical Senior Design
// 
// PURPOSE: All tunable constants live here.
// If you need to adjust a threshold, change it HERE only.

#ifndef CONFIG_H
#define CONFIG_H

// ========================================
// TEMPERATURE THRESHOLDS
// Source: Design Specifications Document, Section 2
// These are deviations ABOVE the patient's baseline
// ========================================
#define TEMP_THRESHOLD_POSSIBLE   1.0   // +1.0°C above baseline = possible swelling
#define TEMP_THRESHOLD_LIKELY     2.0   // +2.0°C above baseline = likely swelling
#define TEMP_THRESHOLD_SIGNIFICANT 3.0  // +3.0°C above baseline = significant swelling

// ========================================
// PRESSURE THRESHOLDS
// Logic: Swelling = tighter fit = HIGHER pressure than baseline
// Units: Raw ADC value deviation (0-4095 on ESP32's 12-bit ADC)
// NOTE: These will need tuning once you test with the physical sleeve.
//       Start with 10% above baseline as your trigger point.
// ========================================
#define PRESSURE_THRESHOLD_PERCENT  0.10  // 10% above baseline triggers detection
                                          // e.g., if baseline = 1000, trigger at 1100

// ========================================
// CALIBRATION SETTINGS
// ========================================
#define CALIBRATION_SAMPLES     20      // Number of samples to average for baseline
                                        // Matches Figure 7 in your design doc
#define CALIBRATION_DELAY_MS    150     // Milliseconds between each calibration sample
                                        // 20 samples x 150ms = 3 seconds total

// ========================================
// COMPRESSION TARGET
// Source: Design Specifications Document, Section 5b
// ========================================
#define TARGET_PRESSURE_MMHG    40      // Target therapeutic compression pressure
                                        // Used by Person B in pneumatics control

// ========================================
// SENSOR SMOOTHING
// We take multiple readings and average them to reduce electrical noise
// ========================================
#define SMOOTH_SAMPLES          5       // Number of readings to average per sensor cycle
#define SMOOTH_DELAY_MS         10      // Milliseconds between smoothing samples

// Temperature Filter Settings (for moving average)
#define TEMP_FILTER_SIZE        10      // Number of samples for temperature smoothing
#define SENSOR_READ_INTERVAL_MS 500     // Read sensors every 500ms (2 Hz)

// Temperature Normal Range (for baseline reference)
#define TEMP_NORMAL_MIN         30.0    // Minimum normal skin temperature (Celsius)
#define TEMP_NORMAL_MAX         34.0    // Maximum normal skin temperature (Celsius)

// ========================================
// COOLDOWN PERIOD
// After a compression cycle, wait before monitoring again
// ========================================
#define COOLDOWN_DURATION_MS    30000   // 30 seconds cooldown after each compression

// ========================================
// ESP32 ADC SETTINGS
// The ESP32 has a 12-bit ADC (values 0 - 4095)
// Operating voltage: 3.3V
// ========================================
#define ADC_MAX_VALUE           4095    // 12-bit ADC max
#define ADC_REFERENCE_VOLTAGE   3.3     // Volts

// ========================================
// SERIAL MONITOR
// ========================================
#define SERIAL_BAUD_RATE        115200  // Must match Serial.begin() in .ino file
#define PRINT_INTERVAL_MS       1000    // Print sensor readings every 1 second

#endif // CONFIG_H
