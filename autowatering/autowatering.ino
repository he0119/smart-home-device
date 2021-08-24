// Pins
#define DHT_PIN D4
#define VALVE1_PIN D5
#define VALVE2_PIN D6
#define VALVE3_PIN D7
#define PUMP_PIN D8

// Button
#include "OneButton.h"
OneButton valve1_btn(D1, false, false);
OneButton valve2_btn(D2, false, false);
OneButton valve3_btn(9, false, false); //SD2
OneButton pump_btn(10, false, false);  //SD3

// Config
#ifdef CI_TESTING

#include "config.example.h"
#define DEBUG_PRINTLN(...)
#define DEBUG_PRINT(...)

#else

#ifdef ENABLE_DEBUG
#include "config.test.h"
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__);
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__);
#else
#include "config.h"
#define DEBUG_PRINTLN(...)
#define DEBUG_PRINT(...)
#endif

#endif

// WIFI&OTA&FS
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <FS.h>

// Ticker&Watchdog
#include <Ticker.h>
Ticker secondTick;
volatile int watchdogCount = 1;

// NTP
#include <NTPClient.h>
#include <WiFiUdp.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.aliyun.com");

// Json
#include <ArduinoJson.h>

// MQTT
#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient client(espClient);
String device_status_topic = "device/" + String(device_name) + "/status";
String device_set_topic = "device/" + String(device_name) + "/set";

// DHT
#include <DHTStable.h>
#ifdef DHT_VERSION_11
#define readdht read11
#endif
#ifdef DHT_VERSION_22
#define readdht read22
#endif
DHTStable DHT;

// Status
unsigned long lastMillis = 0; // Upload Data Timer
float temperature;
float relative_humidity;
unsigned long data_readtime;
long wifi_signal;

bool need_save_config = false;

// relay
#include "relay.h"
Relay valve1(VALVE1_PIN);
Relay valve2(VALVE2_PIN);
Relay valve3(VALVE3_PIN);
Relay pump(PUMP_PIN);

/**
 * @brief 上传当前状态
 *
 * @param reset 是否重置定时上传计时器
 *
 */
void upload(bool reset)
{
  const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(11);
  DynamicJsonDocument doc(capacity);

  doc["timestamp"] = data_readtime;
  JsonObject data = doc.createNestedObject("data");
  data["temperature"] = temperature;
  data["humidity"] = relative_humidity;
  data["valve1"] = valve1.status();
  data["valve2"] = valve2.status();
  data["valve3"] = valve3.status();
  data["pump"] = pump.status();
  data["valve1_delay"] = valve1.delay();
  data["valve2_delay"] = valve2.delay();
  data["valve3_delay"] = valve3.delay();
  data["pump_delay"] = pump.delay();
  data["wifi_signal"] = wifi_signal;

  char msg[300];
  serializeJson(doc, msg);

  DEBUG_PRINTLN("Upload status");
  DEBUG_PRINTLN(msg);
  client.publish(device_status_topic.c_str(), msg);

  if (reset)
    lastMillis = millis(); // Reset the upload data timer
}

void callback(char* topic, byte* payload, unsigned int length)
{
  DEBUG_PRINTLN("Message arrived [");
  DEBUG_PRINTLN(topic);
  DEBUG_PRINTLN("] ");
  const size_t capacity = JSON_OBJECT_SIZE(8) + 150;
  DynamicJsonDocument doc(capacity);
  auto error = deserializeJson(doc, payload);
  // Test if parsing succeeds.
  if (error)
  {
    return;
  }

  if (doc.containsKey("valve1"))
  {
    doc["valve1"] ? valve1.open() : valve1.close();
  }
  if (doc.containsKey("valve2"))
  {
    doc["valve2"] ? valve2.open() : valve2.close();
  }
  if (doc.containsKey("valve3"))
  {
    doc["valve3"] ? valve3.open() : valve3.close();
  }
  if (doc.containsKey("pump"))
  {
    doc["pump"] ? pump.open() : pump.close();
  }

  if (doc.containsKey("valve1_delay"))
  {
    valve1.set_delay(doc["valve1_delay"]);
    need_save_config = true;
  }
  if (doc.containsKey("valve2_delay"))
  {
    valve2.set_delay(doc["valve2_delay"]);
    need_save_config = true;
  }
  if (doc.containsKey("valve3_delay"))
  {
    valve3.set_delay(doc["valve3_delay"]);
    need_save_config = true;
  }
  if (doc.containsKey("pump_delay"))
  {
    pump.set_delay(doc["pump_delay"]);
    need_save_config = true;
  }

  data_readtime = timeClient.getEpochTime();

  upload(0);
  if (need_save_config)
  {
    save_config();
    need_save_config = false;
  }
}

void setup_wifi()
{
  delay(10);
  WiFi.mode(WIFI_STA);
  // We start by connecting to a WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    delay(5000);
    ESP.restart();
  }
}

// Read sensor data
void read_data()
{
  int chk = DHT.readdht(DHT_PIN);
  switch (chk)
  {
  case DHTLIB_OK:
    relative_humidity = DHT.getHumidity();
    temperature = DHT.getTemperature();
    break;
  default:
    relative_humidity = NULL;
    temperature = NULL;
    break;
  }
  wifi_signal = WiFi.RSSI();
  data_readtime = timeClient.getEpochTime();
}

bool load_config()
{
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile)
  {
    DEBUG_PRINTLN("Read config failed");
    return false;
  }

  size_t size = configFile.size();

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  const size_t capacity = JSON_OBJECT_SIZE(4) + 150;
  DynamicJsonDocument doc(capacity);
  auto error = deserializeJson(doc, buf.get());

  if (error)
  {
    return false;
  }

  // Read Config-----------
  valve1.set_delay(doc["valve1_delay"]);
  valve2.set_delay(doc["valve2_delay"]);
  valve3.set_delay(doc["valve3_delay"]);
  pump.set_delay(doc["pump_delay"]);
  // ----------------------

  return true;
}

bool save_config()
{
  const size_t capacity = JSON_OBJECT_SIZE(4);
  DynamicJsonDocument doc(capacity);

  // Save Config------------
  doc["valve1_delay"] = valve1.delay();
  doc["valve2_delay"] = valve2.delay();
  doc["valve3_delay"] = valve3.delay();
  doc["pump_delay"] = pump.delay();
  // -----------------------

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    DEBUG_PRINTLN("Write config failed");
    return false;
  }

  serializeJson(doc, configFile);
  return true;
}

// Watchdog
void ISRwatchdog()
{
  watchdogCount++;
  if (watchdogCount > 60) // Not Responding for 60 seconds, it will reset the board.
  {
    ESP.reset();
  }
}

// MQTT Connection
void reconnect()
{
  DEBUG_PRINTLN("Attempting MQTT connection...");
  // 客户端 ID 和设备名称一致
  // Attempt to connect
  if (client.connect(device_name, device_name, mqtt_password))
  {
    DEBUG_PRINTLN("connected");
    client.subscribe(device_set_topic.c_str(), 1);
  }
  else
  {
    DEBUG_PRINT("failed, rc=");
    DEBUG_PRINT(client.state());
    DEBUG_PRINTLN("try again in 1 seconds");
    // Wait 1 seconds before retrying
    delay(1000);
  }
}

void setup()
{
#ifdef ENABLE_DEBUG
  Serial.begin(115200);
#endif

  DEBUG_PRINTLN("Setting all pins");
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(VALVE1_PIN, OUTPUT);
  pinMode(VALVE2_PIN, OUTPUT);
  pinMode(VALVE3_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(VALVE1_PIN, LOW);
  digitalWrite(VALVE2_PIN, LOW);
  digitalWrite(VALVE3_PIN, LOW); //Off

  // Button
  // Single Click event attachment with lambda
  valve1_btn.attachClick([]()
    {
      DEBUG_PRINTLN("Valve1 Pressed!");
      valve1.toggle();
      upload(0);
    });
  valve2_btn.attachClick([]()
    {
      DEBUG_PRINTLN("Valve2 Pressed!");
      valve2.toggle();
      upload(0);
    });
  valve3_btn.attachClick([]()
    {
      DEBUG_PRINTLN("Valve3 Pressed!");
      valve3.toggle();
      upload(0);
    });
  pump_btn.attachClick([]()
    {
      DEBUG_PRINTLN("Pump Pressed!");
      pump.toggle();
      upload(0);
    });

  SPIFFS.begin(); // FS
  if (!load_config())
    save_config(); // Read config, or save default settings.

  setup_wifi(); // Setup Wi-Fi

  timeClient.begin(); // Start NTP service

  // OTA
  DEBUG_PRINTLN("Starting OTA");
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(device_name);
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
    {
      DEBUG_PRINTLN((float)progress / total * 100);
      watchdogCount = 1; // Feed dog while doing update
    });
  ArduinoOTA.begin();

  // MQTT
  DEBUG_PRINTLN("Starting MQTT");
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Watchdog
  secondTick.attach(1, ISRwatchdog);
}

void loop()
{
  watchdogCount = 1; // Feed dog

  ArduinoOTA.handle(); // OTA
  timeClient.update(); // NTP

  // keep watching the push button:
  valve1_btn.tick();
  valve2_btn.tick();
  valve3_btn.tick();
  pump_btn.tick();

  // Relays
  valve1.tick();
  valve2.tick();
  valve3.tick();
  pump.tick();

  // MQTT
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  // Upload data every 10 seconds
  if (millis() - lastMillis > 10000)
  {
    read_data();
    upload(1);
  }
}
