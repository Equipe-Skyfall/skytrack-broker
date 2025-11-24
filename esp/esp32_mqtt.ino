#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

// ============================
// --- CONFIGURATION ---
// ============================
#define DHTPIN 14         // DHT22 data pin
#define DHTTYPE DHT22    // Sensor type
#define PHOTOSENSOR_PIN 32  // TEMT6000 photosensor pin (ADC1 - safe with WiFi)
#define BUZZER_PIN 13       // Buzzer pin
#define TEMP_ALARM_THRESHOLD 30.0  // Temperature threshold in °C (adjust as needed)

// WiFi credentials
const char* ssid = "Ichiban";
const char* password = "12345678";

// MQTT configuration
const char* mqtt_server = "166d9acce84b47e48593e715d2114d59.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "skytrack";
const char* mqtt_pass = "123456789Skytrack";

// ============================
// --- OBJECTS ---
// ============================
WiFiClientSecure espClient;
PubSubClient client(espClient);
DHT_Unified dht(DHTPIN, DHTTYPE);

uint32_t delayMS = 2000; // 2-second interval for testing
String macAddress;
unsigned long lastReconnectAttempt = 0;

// ============================
// --- SETUP ---
// ============================
void setup() {
  Serial.begin(115200);   // <-- Important: ESP32 default baud rate
  delay(1000);

  Serial.println();
  Serial.println(F("=== ESP32 DHT22 MQTT Client (Test Mode) ==="));

  WiFi.mode(WIFI_MODE_STA);
  setupWiFi();

  // Get MAC address (used as UUID and MQTT topic)
  macAddress = WiFi.macAddress();
  Serial.print("MAC Address: ");
  Serial.println(macAddress);

  // Setup MQTT (TLS insecure mode for HiveMQ Cloud)
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);

  // Sync time (required for secure connection)
  Serial.println("Syncing time...");
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
  Serial.println("Time synced!");

  // Initialize DHT sensor
  dht.begin();
  Serial.println("DHT22 initialized!");

  // Initialize photosensor and buzzer pins
  pinMode(PHOTOSENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off initially
  Serial.println("Photosensor and Buzzer initialized!");

  Serial.println("============================================");
}

// ============================
// --- MAIN LOOP ---
// ============================
void loop() {
  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (reconnectMQTT()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop();
  }

  static unsigned long lastReading = 0;
  if (millis() - lastReading > delayMS) {
    lastReading = millis();
    readAndSendSensorData();
  }

  delay(100);
}

// ============================
// --- FUNCTIONS ---
// ============================
void setupWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
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

bool reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
    return false;
  }

  Serial.print("Connecting to MQTT...");
  String clientId = "ESP32_" + macAddress;
  if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
    Serial.println("✅ connected!");
    return true;
  } else {
    Serial.print("❌ failed, rc=");
    Serial.println(client.state());
    return false;
  }
}

void readAndSendSensorData() {
  sensors_event_t event;
  float temperature = NAN;
  float humidity = NAN;
  int lightLevel = 0;

  // Read temperature
  dht.temperature().getEvent(&event);
  if (!isnan(event.temperature)) temperature = event.temperature;

  // Read humidity
  dht.humidity().getEvent(&event);
  if (!isnan(event.relative_humidity)) humidity = event.relative_humidity;

  // Read photosensor (light level)
  lightLevel = analogRead(PHOTOSENSOR_PIN);

  // Check temperature alarm (only if we have valid readings and reasonable values)
  if (!isnan(temperature) && temperature > 0 && temperature < 85 && temperature > TEMP_ALARM_THRESHOLD) {
    // Trigger buzzer alarm
    tone(BUZZER_PIN, 1000, 500); // 1000Hz tone for 500ms
    Serial.println("🔔 ALARM: Temperature exceeded threshold!");
  } else {
    // Ensure buzzer is off when not alarming
    noTone(BUZZER_PIN);
  }

  // Print readings to Serial
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

  // Send data to MQTT
  if (client.connected() && !isnan(temperature) && !isnan(humidity)) {
    time_t now = time(nullptr);
    unsigned long unixtime = now;

    String payload = "{";
    payload += "\"uuid\":\"" + macAddress + "\",";
    payload += "\"unixtime\":" + String(unixtime) + ",";
    payload += "\"temperatura\":" + String(temperature, 1) + ",";
    payload += "\"umidade\":" + String(humidity, 1) + ",";
    payload += "\"luz\":" + String(lightLevel);
    payload += "}";

    String topicMac = macAddress;
    topicMac.replace(":", "");
    String topic = "weather/" + topicMac + "/data";

    if (client.publish(topic.c_str(), payload.c_str())) {
      Serial.println("✅ MQTT Publish Success:");
      Serial.println(payload);
    } else {
      Serial.println("❌ MQTT Publish Failed");
    }
  } else {
    Serial.println("⚠️ MQTT not connected or invalid data!");
  }
}
