* TMP117_Test.ino
 * Test sketch for TMP117 temperature sensor with data smoothing
 * 
 * 
 * Hardware Setup:
 * - TMP117 SDA → ESP32 GPIO 21 (default I2C SDA)
 * - TMP117 SCL → ESP32 GPIO 22 (default I2C SCL)
 * - TMP117 VCC → ESP32 3.3V
 * - TMP117 GND → ESP32 GND
 * 
 * See Wiring_Guide.md for detailed connection instructions
 * 
 * Libraries Required:
 * - Adafruit TMP117 (Install via Library Manager)
 * - Adafruit BusIO (Auto-installed with TMP117)
 */

#include <Wire.h>
#include <Adafruit_TMP117.h>
#include "config.h"  // Thresholds and constants, DONE
#include "pins.h"    // Pin definitions, DONE

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

Adafruit_TMP117 tmp117;  // Temperature sensor object

// ============================================================================
// MOVING AVERAGE FILTER FOR TEMPERATURE
// ============================================================================

class TemperatureFilter {
private:
  float readings[TEMP_FILTER_SIZE];  // Array to store recent readings
  int index;                         // Current position in array
  int count;                         // How many readings we have
  
public:
  TemperatureFilter() {
    index = 0;
    count = 0;
    // Initialize all readings to 0
    for (int i = 0; i < TEMP_FILTER_SIZE; i++) {
      readings[i] = 0.0;
    }
  }
  
  // Add a new temperature reading
  void addReading(float temp) {
    readings[index] = temp;
    index = (index + 1) % TEMP_FILTER_SIZE;  // Wrap around to 0 when full
    if (count < TEMP_FILTER_SIZE) {
      count++;  // Keep track until array is full
    }
  }
  
  // Get the smoothed (averaged) temperature
  float getSmoothed() {
    if (count == 0) return 0.0;  // Avoid division by zero
    
    float sum = 0.0;
    for (int i = 0; i < count; i++) {
      sum += readings[i];
    }
    return sum / count;
  }
  
  // Check if we have enough data for reliable readings
  bool isReady() {
    return (count >= TEMP_FILTER_SIZE);
  }
  
  // Reset the filter
  void reset() {
    index = 0;
    count = 0;
    for (int i = 0; i < TEMP_FILTER_SIZE; i++) {
      readings[i] = 0.0;
    }
  }
};

// Create filter instance
TemperatureFilter tempFilter;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  while (!Serial) delay(10);  // Wait for Serial to connect
  
  Serial.println("===========================================");
  Serial.println("===========================================\n");
  
  // Initialize I2C with defined pins
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);  // GPIO 21 (SDA), GPIO 22 (SCL)
  Serial.print("I2C initialized on SDA=");
  Serial.print(I2C_SDA_PIN);
  Serial.print(", SCL=");
  Serial.println(I2C_SCL_PIN);
  
  // Initialize TMP117
  Serial.print("Initializing TMP117 sensor... ");
  if (!tmp117.begin()) {
    Serial.println("FAILED!");
    Serial.println("\nTroubleshooting:");
    Serial.println("1. Check wiring connections");
    Serial.println("2. Verify sensor power (3.3V)");
    Serial.println("3. Check I2C address (default: 0x48)");
    Serial.println("4. Try scanning I2C bus with I2C Scanner sketch");
    while (1) delay(10);  // Halt execution
  }
  Serial.println("SUCCESS!");
  
  // Print sensor info
  Serial.print("Sensor I2C Address: 0x");
  //Serial.println(tmp117.getAddress(), HEX);
  Serial.println("48 (default)");
  Serial.println("\nStarting temperature readings...");
  Serial.println("Columns: Raw Temp | Smoothed Temp | Status");
  Serial.println("-------------------------------------------\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Read temperature from sensor
  sensors_event_t temp_event;
  tmp117.getEvent(&temp_event);
  
  float rawTemp = temp_event.temperature;  // Temperature in °C
  
  // Add reading to filter
  tempFilter.addReading(rawTemp);
  
  // Get smoothed value
  float smoothedTemp = tempFilter.getSmoothed();
  
  // Print results
  Serial.print("Raw: ");
  Serial.print(rawTemp, 2);  // 2 decimal places
  Serial.print(" °C  |  Smoothed: ");
  Serial.print(smoothedTemp, 2);
  Serial.print(" °C  |  ");
  
  // Status indicator
  if (tempFilter.isReady()) {
    Serial.print("READY");
    
    // Check if temperature is in normal range
    if (smoothedTemp >= TEMP_NORMAL_MIN && smoothedTemp <= TEMP_NORMAL_MAX) {
      Serial.println(" ✓ Normal Range");
    } else if (smoothedTemp < TEMP_NORMAL_MIN) {
      Serial.println(" ⚠ Below Normal");
    } else {
      Serial.println(" ⚠ Above Normal");
    }
  } else {
    Serial.println("Warming up...");
  }
  
  // Wait before next reading
  delay(SENSOR_READ_INTERVAL_MS);
}

// ============================================================================
// HELPER FUNCTIONS (for future use in main project)
// ============================================================================

/**
 * Get current smoothed temperature reading
 * This function will be moved to sensors.cpp later
 */
float getCurrentTemperature() {
  sensors_event_t temp_event;
  tmp117.getEvent(&temp_event);
  tempFilter.addReading(temp_event.temperature);
  return tempFilter.getSmoothed();
}

/**
 * Check if temperature sensor is ready (has enough samples)
 */
bool isTemperatureSensorReady() {
  return tempFilter.isReady();
}
