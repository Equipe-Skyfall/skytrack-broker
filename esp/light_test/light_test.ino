// Simple TEMT6000 Light Sensor Test
// Tests light sensor on GPIO 32

#define LIGHT_SENSOR_PIN 32

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== TEMT6000 Light Sensor Test ===");
  Serial.println("Reading from GPIO 32");
  Serial.println("Expected range: 0-4095");
  Serial.println("0 = Complete darkness or sensor issue");
  Serial.println("4095 = Very bright light or floating pin");
  Serial.println("=====================================\n");

  // Set pin as input
  pinMode(LIGHT_SENSOR_PIN, INPUT);
}

void loop() {
  // Read raw ADC value (0-4095 for ESP32 12-bit ADC)
  int rawValue = analogRead(LIGHT_SENSOR_PIN);

  // Calculate voltage (ESP32 ADC reference is 3.3V)
  float voltage = (rawValue / 4095.0) * 3.3;

  // Print results
  Serial.println("--- Light Sensor Reading ---");
  Serial.print("Raw ADC Value: ");
  Serial.println(rawValue);
  Serial.print("Voltage: ");
  Serial.print(voltage, 3);
  Serial.println(" V");

  // Diagnostic info
  if (rawValue == 0) {
    Serial.println("⚠️  WARNING: Stuck at 0 - Check VCC/GND wiring or sensor is faulty");
  } else if (rawValue == 4095) {
    Serial.println("⚠️  WARNING: Stuck at max - Pin might be floating or no GND connection");
  } else if (rawValue < 100) {
    Serial.println("Status: Very dark");
  } else if (rawValue < 1000) {
    Serial.println("Status: Dim light");
  } else if (rawValue < 3000) {
    Serial.println("Status: Moderate light");
  } else {
    Serial.println("Status: Bright light");
  }

  Serial.println("----------------------------\n");

  delay(1000); // Read every second
}
