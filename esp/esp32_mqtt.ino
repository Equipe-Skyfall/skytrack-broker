
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

#define DHTPIN 18 //pino
#define DHTTYPE DHT22 //tipo do sensor

//config do wifi
const char* ssid = "Suhefa1"; //nome do wifi
const char* password = "Fa147258@"; // senha

//config do mqtt
const char* mqtt_server = "166d9acce84b47e48593e715d2114d59.s1.eu.hivemq.cloud"; //url do servidor mqtt
const int mqtt_port = 8883;
const char* mqtt_user = "skytrack";
const char* mqtt_pass = "123456789Skytrack";

WiFiClientSecure espClient;
PubSubClient client(espClient);

DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;
String macAddress;

unsigned long lastReconnectAttempt = 0;

void setup() {
  // Serial.begin(115200);
  // delay(2000);

  // Serial.println(F("ESP32 DHT22 MQTT Client"));
  // Serial.println(F("======================="));


  WiFi.mode(WIFI_MODE_STA);


  setupWiFi();

  //pega o MAC ADDRESS
  macAddress = WiFi.macAddress();

  // Serial.print(F("MAC Address: "));
  // Serial.println(macAddress);
  String topicMac = macAddress;
  topicMac.replace(":", "");
  // Serial.print(F("Topic: weather/"));
  // Serial.print(topicMac);
  // Serial.println(F("/data"));

  
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);

 
  // Serial.println(F("Synchronizing time with NTP..."));
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  // Serial.print(F("Waiting for time sync"));
  time_t now = time(nullptr);
  int attempts = 0;
  while (now < 1000000000L && attempts < 20) {
    delay(500);
    // Serial.print(F("."));
    now = time(nullptr);
    attempts++;
  }
  // Serial.println();

  // if (now > 1000000000L) {
  //   Serial.print(F("✅ Time synchronized: "));
  //   Serial.println(ctime(&now));
  // } else {
  //   Serial.println(F("⚠️ Time sync failed, using boot time"));
  // }


  dht.begin();
  delay(1000);

  // lendo a cada 5 minutos
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  delayMS = sensor.min_delay / 1000;
  if (delayMS < 300000) {
    delayMS = 300000;
  }

  // Serial.println(F("Starting sensor readings..."));
  // Serial.println();
}

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

  delay(1000);
}

void setupWiFi() {
  WiFi.begin(ssid, password);
  // Serial.print(F("Connecting to WiFi"));

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    // Serial.print(F("."));
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Serial.println();
    // Serial.println(F("WiFi connected!"));
    // Serial.print(F("IP address: "));
    // Serial.println(WiFi.localIP());
    // Serial.print(F("MQTT Server: "));
    // Serial.print(mqtt_server);
    // Serial.print(F(":"));
    // Serial.println(mqtt_port);
  } else {
    // Serial.println();
    // Serial.println(F("WiFi connection failed!"));
    // Serial.println(F("Restarting ESP32..."));
    ESP.restart();
  }
  // Serial.println(F("======================="));
}

bool reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    // Serial.println(F("WiFi disconnected, reconnecting..."));
    setupWiFi();
    return false;
  }

  // Serial.print(F("Attempting MQTT connection..."));


  String clientId = "ESP32_" + macAddress;

  if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
    // Serial.println(F(" connected!"));
    return true;
  } else {
    // Serial.print(F(" failed, rc="));
    // Serial.print(client.state());
    // Serial.println(F(" retrying in 5 seconds"));
    return false;
  }
}

void readAndSendSensorData() {

  sensors_event_t event;
  float temperature = NAN;
  float humidity = NAN;
  bool hasValidData = false;

  dht.temperature().getEvent(&event);
  if (!isnan(event.temperature)) {
    temperature = event.temperature;
    hasValidData = true;
  }

  dht.humidity().getEvent(&event);
  if (!isnan(event.relative_humidity)) {
    humidity = event.relative_humidity;
    hasValidData = true;
  }

  // Serial.println(F("--- Sensor Reading ---"));
  // Serial.print(F("Device: "));
  // Serial.println(macAddress);

  // if (!isnan(temperature)) {
  //   Serial.print(F("Temperature: "));
  //   Serial.print(temperature);
  //   Serial.println(F("°C"));
  // } else {
  //   Serial.println(F("Error reading temperature!"));
  // }

  // if (!isnan(humidity)) {
  //   Serial.print(F("Humidity: "));
  //   Serial.print(humidity);
  //   Serial.println(F("%"));
  // } else {
  //   Serial.println(F("Error reading humidity!"));
  // }

  if (hasValidData && client.connected()) {

    time_t now = time(nullptr);
    unsigned long unixtime = now;
    //formatando a resposta
    String payload = "{";
    payload += "\"uuid\":\"" + macAddress + "\",";
    payload += "\"unixtime\":" + String(unixtime) + ",";

    if (!isnan(temperature)) {
      payload += "\"temperatura\":" + String(temperature, 1) + ",";
    }
    if (!isnan(humidity)) {
      payload += "\"umidade\":" + String(humidity, 1);
    }


    if (payload.endsWith(",")) {
      payload = payload.substring(0, payload.length() - 1);
    }

    payload += "}";

 
    String topicMac = macAddress;
    topicMac.replace(":", "");
    String topic = "weather/" + topicMac + "/data";
    if (client.publish(topic.c_str(), payload.c_str())) {
      // Serial.print(F("✅ Data sent: "));
      // Serial.print(topic);
      // Serial.print(F(" = "));
      // Serial.println(payload);
      // Serial.println(F("📡 All data sent successfully!"));
    } else {
      // Serial.println(F("❌ Failed to send data"));
    }

  } else {
    // if (!hasValidData) {
    //   Serial.println(F("⚠️  No valid sensor data to send"));
    // }
    // if (!client.connected()) {
    //   Serial.println(F("⚠️  MQTT not connected"));
    // }
  }

  // Serial.println(F("---------------------"));
  // Serial.println();
}