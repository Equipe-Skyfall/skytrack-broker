/*
 * ESP32 Weather Station with MQTT
 *
 * Reads temperature, humidity (DHT22) and light level (TEMT6000)
 * Publishes data to MQTT broker (HiveMQ Cloud)
 * Triggers buzzer alarm when temperature exceeds threshold
 */

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

// ============================================================
// CONFIGURATION - Adjust these settings for your setup
// ============================================================

// Sensor Pins
#define DHTPIN 32                    // DHT22 temperature/humidity sensor
#define PHOTOSENSOR_PIN 14           // TEMT6000 light sensor (analog input)
#define BUZZER_PIN 13                // Active buzzer for temperature alarm

// Sensor Configuration
#define DHTTYPE DHT22                // Sensor type (DHT11 or DHT22)
#define TEMP_ALARM_THRESHOLD 30.0    // Temperature alarm threshold (°C)
#define SENSOR_READ_INTERVAL 2000    // How often to read sensors (milliseconds)

// WiFi Credentials
const char* WIFI_SSID = "Ichiban";
const char* WIFI_PASSWORD = "12345678";

// MQTT Broker Configuration (HiveMQ Cloud)
const char* MQTT_SERVER = "166d9acce84b47e48593e715d2114d59.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;          // TLS/SSL port
const char* MQTT_USER = "skytrack";
const char* MQTT_PASSWORD = "123456789Skytrack";

// ============================================================
// GLOBAL OBJECTS & VARIABLES
// ============================================================

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
DHT_Unified dht(DHTPIN, DHTTYPE);

String deviceMacAddress;              // Used as unique device ID
unsigned long lastReconnectAttempt = 0;

// ============================================================
// SETUP - Runs once on startup
// ============================================================

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Initialize buzzer (turn off immediately to prevent startup noise)
  initializeBuzzer();

  delay(1000);
  Serial.println();
  Serial.println(F("=== ESP32 Weather Station ==="));

  // Connect to WiFi
  connectToWiFi();

  // Get device MAC address (used as unique identifier)
  deviceMacAddress = WiFi.macAddress();
  Serial.print("Device MAC: ");
  Serial.println(deviceMacAddress);

  // Configure MQTT
  espClient.setInsecure();  // Use insecure mode for HiveMQ Cloud
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  // Sync time with NTP servers (required for TLS)
  syncTime();

  // Initialize sensors
  initializeSensors();

  Serial.println("============================================");
  Serial.println("Setup complete! Starting main loop...");
  Serial.println("============================================");
}

// ============================================================
// MAIN LOOP - Runs continuously
// ============================================================

void loop() {
  // Maintain MQTT connection
  if (!mqttClient.connected()) {
    reconnectMQTT();
  } else {
    mqttClient.loop();
  }

  // Read and send sensor data at regular intervals
  static unsigned long lastReading = 0;
  if (millis() - lastReading > SENSOR_READ_INTERVAL) {
    lastReading = millis();
    readAndPublishSensorData();
  }

  delay(100);
}

// ============================================================
// INITIALIZATION FUNCTIONS
// ============================================================

void initializeBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);  // Ensure buzzer is off
  delay(100);  // Let hardware stabilize
}

void initializeSensors() {
  // Initialize DHT22 sensor
  dht.begin();
  Serial.println("✅ DHT22 sensor initialized");

  // Initialize photosensor pin
  pinMode(PHOTOSENSOR_PIN, INPUT);
  Serial.println("✅ TEMT6000 light sensor initialized");
}

void syncTime() {
  Serial.println("Syncing time with NTP servers...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);
  int attempts = 0;

  while (now < 1000000000L && attempts < 20) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    attempts++;
  }

  Serial.println();
  if (now >= 1000000000L) {
    Serial.println("✅ Time synced successfully");
  } else {
    Serial.println("⚠️  Time sync failed, continuing anyway...");
  }
}

// ============================================================
// CONNECTIVITY FUNCTIONS
// ============================================================

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("✅ WiFi connected! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("❌ WiFi connection failed! Restarting...");
    ESP.restart();
  }
}

void reconnectMQTT() {
  // Don't attempt too frequently
  unsigned long now = millis();
  if (now - lastReconnectAttempt < 5000) {
    return;
  }
  lastReconnectAttempt = now;

  // Ensure WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
    return;
  }

  // Attempt MQTT connection
  Serial.print("Connecting to MQTT broker...");
  String clientId = "ESP32_" + deviceMacAddress;

  if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
    Serial.println("✅ Connected!");
    lastReconnectAttempt = 0;
  } else {
    Serial.print("❌ Failed, rc=");
    Serial.println(mqttClient.state());
  }
}

// ============================================================
// SENSOR READING & PUBLISHING
// ============================================================

void readAndPublishSensorData() {
  // Read sensor values
  float temperature = readTemperature();
  float humidity = readHumidity();
  int lightLevel = readLightLevel();

  // Check temperature alarm
  checkTemperatureAlarm(temperature);

  // Print readings to serial monitor
  printSensorReadings(temperature, humidity, lightLevel);

  // Publish to MQTT if connected and data is valid
  if (mqttClient.connected() && !isnan(temperature) && !isnan(humidity)) {
    publishToMQTT(temperature, humidity, lightLevel);
  } else {
    Serial.println("⚠️  Cannot publish - MQTT disconnected or invalid sensor data");
  }
}

float readTemperature() {
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  return isnan(event.temperature) ? NAN : event.temperature;
}

float readHumidity() {
  sensors_event_t event;
  dht.humidity().getEvent(&event);
  return isnan(event.relative_humidity) ? NAN : event.relative_humidity;
}

int readLightLevel() {
  return analogRead(PHOTOSENSOR_PIN);
}

void checkTemperatureAlarm(float temperature) {
  // Only trigger alarm for valid, reasonable temperature readings
  bool alarmCondition = !isnan(temperature) &&
                        temperature > 0 &&
                        temperature < 85 &&
                        temperature > TEMP_ALARM_THRESHOLD;

  if (alarmCondition) {
    digitalWrite(BUZZER_PIN, HIGH);
    Serial.println("🔔 ALARM: Temperature exceeded threshold!");
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void printSensorReadings(float temperature, float humidity, int lightLevel) {
  Serial.println("------ SENSOR READINGS ------");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
  Serial.print("Light Level: ");
  Serial.print(lightLevel);
  Serial.println(" (0-4095)");
  Serial.println("-----------------------------");
}

void publishToMQTT(float temperature, float humidity, int lightLevel) {
  // Get current Unix timestamp
  time_t now = time(nullptr);
  unsigned long unixtime = now;

  // Build JSON payload
  String payload = "{";
  payload += "\"uuid\":\"" + deviceMacAddress + "\",";
  payload += "\"unixtime\":" + String(unixtime) + ",";
  payload += "\"temperatura\":" + String(temperature, 1) + ",";
  payload += "\"umidade\":" + String(humidity, 1) + ",";
  payload += "\"luz\":" + String(lightLevel);
  payload += "}";

  // Build MQTT topic (weather/{MAC_ADDRESS}/data)
  String topicMac = deviceMacAddress;
  topicMac.replace(":", "");  // Remove colons from MAC
  String topic = "weather/" + topicMac + "/data";

  // Publish to MQTT broker
  if (mqttClient.publish(topic.c_str(), payload.c_str())) {
    Serial.println("✅ MQTT Publish Success:");
    Serial.println(payload);
  } else {
    Serial.println("❌ MQTT Publish Failed");
  }
}
