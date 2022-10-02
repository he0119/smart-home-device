// Pins
// 数字指 GPIO
#ifdef ESP8266
#define DHT_PIN D4
#define VALVE1_PIN D5
#define VALVE2_PIN D6
#define VALVE3_PIN D7
#define PUMP_PIN D8
#define VALVE1_BTN_PIN D1
#define VALVE2_BTN_PIN D2
#define VALVE3_BTN_PIN 9 //SD2
#define PUMP_BTN_PIN 10 //SD3
#else
// 34,35,36,39 为仅输入
// ESP32-DevKitC-32E
#define DHT_PIN 32
#define VALVE1_PIN 33
#define VALVE2_PIN 25
#define VALVE3_PIN 26
#define PUMP_PIN 27
#define VALVE1_BTN_PIN 4
#define VALVE2_BTN_PIN 2
#define VALVE3_BTN_PIN 15
#define PUMP_BTN_PIN 0
#endif

// Button
#include "OneButton.h"
OneButton valve1_btn(VALVE1_BTN_PIN, false, false);
OneButton valve2_btn(VALVE2_BTN_PIN, false, false);
OneButton valve3_btn(VALVE3_BTN_PIN, false, false);
OneButton pump_btn(PUMP_BTN_PIN, false, false);

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
#ifdef ESP8266
#include <ESP8266WiFi.h>
#define FORMAT_LITTLEFS_IF_FAILED
#else
#include <WiFi.h>
#define FORMAT_LITTLEFS_IF_FAILED true
#endif
#include <ArduinoOTA.h>
#include <FS.h>
#include <LittleFS.h>

// Ticker&Watchdog
#include <Ticker.h>
Ticker secondTick;
volatile int watchdogCount = 1;

// NTP
#include <NTPClient.h>
#include <WiFiUdp.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "cn.ntp.org.cn");

// Json
#include <ArduinoJson.h>

// WebSockets
#include <WebSocketsClient.h>
WebSocketsClient webSocket;

// DHT
#include <dhtnew.h>
DHTNEW mySensor(DHT_PIN);

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
  const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(11);
  DynamicJsonDocument doc(capacity);

  if (reset)
    lastMillis = millis(); // Reset the upload data timer

  // 处理出错的数据
  // 同时为零的时候不上传，这个大概是没插好的时候
  if (temperature == 0 && relative_humidity == 0)
  {
    DEBUG_PRINTLN("Temperature and humidity is zero, skip upload");
    return;
  }
  // 相对湿度如果大于 100% 则不上传
  if (relative_humidity > 100)
  {
    DEBUG_PRINTLN("Humidity > 100%, skip upload");
    return;
  }

  doc["id"] = data_readtime;
  doc["method"] = "properties_changed";
  JsonObject data = doc.createNestedObject("params");
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
  webSocket.sendTXT(msg);
}

void callback(WStype_t type, uint8_t* payload, size_t length)
{
  if (type == WStype_TEXT) {
    DEBUG_PRINTLN("Received text");
    DEBUG_PRINTLN((char*)payload);
    const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(8) + 150;
    DynamicJsonDocument doc(capacity);
    auto error = deserializeJson(doc, payload);
    // Test if parsing succeeds.
    if (error)
    {
      return;
    }

    if (doc.containsKey("method") && doc["method"] == "set_properties" && doc.containsKey("params")) {
      if (doc["params"].containsKey("valve1"))
      {
        doc["params"]["valve1"] ? valve1.open() : valve1.close();
      }
      if (doc["params"].containsKey("valve2"))
      {
        doc["params"]["valve2"] ? valve2.open() : valve2.close();
      }
      if (doc["params"].containsKey("valve3"))
      {
        doc["params"]["valve3"] ? valve3.open() : valve3.close();
      }
      if (doc["params"].containsKey("pump"))
      {
        doc["params"]["pump"] ? pump.open() : pump.close();
      }

      if (doc["params"].containsKey("valve1_delay"))
      {
        valve1.set_delay(doc["params"]["valve1_delay"]);
        need_save_config = true;
      }
      if (doc["params"].containsKey("valve2_delay"))
      {
        valve2.set_delay(doc["params"]["valve2_delay"]);
        need_save_config = true;
      }
      if (doc["params"].containsKey("valve3_delay"))
      {
        valve3.set_delay(doc["params"]["valve3_delay"]);
        need_save_config = true;
      }
      if (doc["params"].containsKey("pump_delay"))
      {
        pump.set_delay(doc["params"]["pump_delay"]);
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
  }
}

void setup_wifi()
{
  delay(10);
  WiFi.mode(WIFI_STA);
  // We start by connecting to a WiFi network
  WiFi.begin(ssid, wifi_password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    delay(5000);
    ESP.restart();
  }
}

// Read sensor data
void read_data()
{
  int chk = mySensor.read();
  switch (chk)
  {
  case DHTLIB_OK:
    relative_humidity = mySensor.getHumidity();
    temperature = mySensor.getTemperature();
    break;
  default:
    return;
  }
  wifi_signal = WiFi.RSSI();
  data_readtime = timeClient.getEpochTime();
}

bool load_config()
{
  File configFile = LittleFS.open("/config.json", "r");
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

  configFile.close();
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

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile)
  {
    DEBUG_PRINTLN("Write config failed");
    return false;
  }

  serializeJson(doc, configFile);

  configFile.close();
  return true;
}

// Watchdog
void ISRwatchdog()
{
  watchdogCount++;
  if (watchdogCount > 60) // Not Responding for 60 seconds, it will reset the board.
  {
#ifdef ESP8266
    ESP.reset();
#else
    ESP.restart();
#endif
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

  // FS
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    DEBUG_PRINTLN("LittleFS mount failed");
    return;
  }
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

  // WebSockets
  DEBUG_PRINTLN("Starting WebSockets");
  webSocket.beginSSL(server_host, server_port, server_url);
  webSocket.onEvent(callback);
  webSocket.setAuthorization(devcie_id, token); // HTTP Basic Authorization

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

  // webSockets
  webSocket.loop();

  // Upload data every 10 seconds
  if (millis() - lastMillis > 10000)
  {
    read_data();
    upload(1);
  }
}
