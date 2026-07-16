#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <stdlib.h>

static const int OLED_SDA_PIN = 21;
static const int OLED_SCL_PIN = 22;
static const int SCREEN_WIDTH = 128;
static const int SCREEN_HEIGHT = 64;
static const int OLED_RESET = -1;

static const int PZEM_RX_PIN = 16;
static const int PZEM_TX_PIN = 17;
static const unsigned long REFRESH_INTERVAL_MS = 10000UL;
static const unsigned long WIFI_RETRY_INTERVAL_MS = 5000UL;
static const unsigned long MQTT_RETRY_INTERVAL_MS = 5000UL;

static const char *WIFI_SSID = "Likung_5G";
static const char *WIFI_PASSWORD = "072882800";
static const char *WIFI_HOSTNAME = "esp32-energy-mqtt";

static const char *MQTT_HOST = "mqttgo.io";
static const uint16_t MQTT_PORT = 1883;
static const char *MQTT_TOPIC = "fukuei/class701/data";

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

byte Xbuffer[50];
static uint8_t oledAddr = 0x3C;
static unsigned long lastRefreshMs = 0;
static unsigned long lastWifiRetryMs = 0;
static unsigned long lastMqttRetryMs = 0;
static float lastVoltage = NAN;
static float lastCurrent = NAN;
static float lastPower = NAN;
static float lastEnergy = NAN;
static float lastFrequency = NAN;
static float lastPf = NAN;
static bool lastReadOk = false;
static bool lastPublishOk = false;

static bool initDisplay(uint8_t address)
{
  if (!display.begin(SSD1306_SWITCHCAPVCC, address)) {
    return false;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setRotation(2);
  return true;
}

static void drawTextScreen(const char *title, const char *line1, const char *line2, const char *line3)
{
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(title);

  display.setCursor(0, 16);
  display.println(line1);

  display.setCursor(0, 32);
  display.println(line2);

  display.setCursor(0, 48);
  display.println(line3);

  display.display();
}

static void drawStatus()
{
  char line1[32];
  char line2[32];
  char line3[32];

  if (lastReadOk) {
    snprintf(line1, sizeof(line1), "V=%.1f I=%.2f", lastVoltage, lastCurrent);
    snprintf(line2, sizeof(line2), "W=%.1f kWh=%.3f", lastPower, lastEnergy);
    snprintf(line3, sizeof(line3), "Hz=%.1f PF=%.0f%%", lastFrequency, lastPf * 100.0f);
  } else {
    snprintf(line1, sizeof(line1), "V=-!!! I=-!!!");
    snprintf(line2, sizeof(line2), "W=-!!! kWh=-!!!");
    snprintf(line3, sizeof(line3), "Hz=-!!! PF=-!!!");
  }

  drawTextScreen("16 ENERGY MQTT", line1, line2, line3);
}

static String ReadData()
{
  Serial2.write(0xf8);
  Serial2.write(0x04);
  Serial2.write(0x00);
  Serial2.write(0x00);
  Serial2.write(0x00);
  Serial2.write(0x0a);
  Serial2.write(0x64);
  Serial2.write(0x64);
  delay(100);

  int i = 0;
  String rtext = "";
  while (Serial2.available()) {
    byte b = Serial2.read();
    if (i < (int)sizeof(Xbuffer)) {
      Xbuffer[i] = b;
    }
    if (b < 0x10) {
      rtext = rtext + "0" + String(b, HEX) + " ";
    } else {
      rtext = rtext + String(b, HEX) + " ";
    }
    i++;
  }
  return rtext;
}

static void ClearData()
{
  Serial2.write(0xf8);
  Serial2.write(0x42);
  Serial2.write(0xc2);
  Serial2.write(0x41);
  delay(100);
}

static float decodeV(const String &rtext)
{
  char CharV[5];
  String StringV = rtext.substring(9, 11) + rtext.substring(12, 14);
  StringV.toCharArray(CharV, sizeof(CharV));
  int intV = strtol(CharV, NULL, 16);
  return intV * 0.1f;
}

static float decodeA(const String &rtext)
{
  char CharA[9];
  String StringA = rtext.substring(21, 23) + rtext.substring(24, 26) + rtext.substring(15, 17) + rtext.substring(18, 20);
  StringA.toCharArray(CharA, sizeof(CharA));
  int intA = strtol(CharA, NULL, 16);
  return intA * 0.001f;
}

static float decodeW(const String &rtext)
{
  char CharW[9];
  String StringW = rtext.substring(33, 35) + rtext.substring(36, 38) + rtext.substring(27, 29) + rtext.substring(30, 32);
  StringW.toCharArray(CharW, sizeof(CharW));
  int intW = strtol(CharW, NULL, 16);
  return intW * 0.1f;
}

static float decodekWh(const String &rtext)
{
  char CharWh[9];
  String StringWh = rtext.substring(45, 47) + rtext.substring(48, 50) + rtext.substring(39, 41) + rtext.substring(42, 44);
  StringWh.toCharArray(CharWh, sizeof(CharWh));
  int intWh = strtol(CharWh, NULL, 16);
  return intWh * 0.001f;
}

static float decodeFrequency(const String &rtext)
{
  char CharHz[5];
  String StringHz = rtext.substring(49, 51) + rtext.substring(52, 54);
  StringHz.toCharArray(CharHz, sizeof(CharHz));
  int intHz = strtol(CharHz, NULL, 16);
  return intHz * 0.1f;
}

static float decodePF(const String &rtext)
{
  char CharPF[5];
  String StringPF = rtext.substring(57, 59) + rtext.substring(60, 62);
  StringPF.toCharArray(CharPF, sizeof(CharPF));
  int intPF = strtol(CharPF, NULL, 16);
  return (float)intPF / 100.0f;
}

static bool readPzemValues(float &voltage, float &current, float &power, float &energy, float &frequency, float &pf)
{
  String rtext = ReadData();
  if (rtext.length() == 0) {
    return false;
  }

  voltage = decodeV(rtext);
  current = decodeA(rtext);
  power = decodeW(rtext);
  energy = decodekWh(rtext);
  frequency = decodeFrequency(rtext);
  pf = decodePF(rtext);
  return true;
}

static bool connectWifi()
{
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  return false;
}

static void ensureWifi()
{
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (millis() - lastWifiRetryMs < WIFI_RETRY_INTERVAL_MS) {
    return;
  }
  lastWifiRetryMs = millis();
  connectWifi();
}

static bool connectMqtt()
{
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setSocketTimeout(5);
  mqttClient.setKeepAlive(60);

  uint32_t chipId = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF);
  String clientId = "esp32-energy-mqtt-" + String(chipId, HEX);
  return mqttClient.connect(clientId.c_str());
}

static void ensureMqtt()
{
  if (WiFi.status() != WL_CONNECTED) {
    mqttClient.disconnect();
    return;
  }

  if (mqttClient.connected()) {
    return;
  }

  if (millis() - lastMqttRetryMs < MQTT_RETRY_INTERVAL_MS) {
    return;
  }
  lastMqttRetryMs = millis();
  connectMqtt();
}

static bool publishJson()
{
  if (!mqttClient.connected() || !lastReadOk) {
    return false;
  }

  char payload[128];
  snprintf(
    payload,
    sizeof(payload),
    "{\"V\":%.1f,\"I\":%.2f,\"W\":%.1f,\"kWh\":%.3f}",
    lastVoltage,
    lastCurrent,
    lastPower,
    lastEnergy
  );

  bool ok = mqttClient.publish(MQTT_TOPIC, payload);
  lastPublishOk = ok;
  return ok;
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("16_energy_mqtt boot"));

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  Wire.setClock(400000);

  if (!initDisplay(0x3C)) {
    oledAddr = 0x3D;
    if (!initDisplay(0x3D)) {
      Serial.println(F("SSD1306 init failed"));
      while (true) {
        delay(1000);
      }
    }
  }

  Serial2.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
  ClearData();
  connectWifi();
}

void loop()
{
  ensureWifi();
  ensureMqtt();
  mqttClient.loop();

  if (millis() - lastRefreshMs < REFRESH_INTERVAL_MS) {
    drawStatus();
    delay(10);
    return;
  }
  lastRefreshMs = millis();

  float voltage = 0;
  float current = 0;
  float power = 0;
  float energy = 0;
  float frequency = 0;
  float pf = 0;

  lastReadOk = readPzemValues(voltage, current, power, energy, frequency, pf);
  if (lastReadOk) {
    lastVoltage = voltage;
    lastCurrent = current;
    lastPower = power;
    lastEnergy = energy;
    lastFrequency = frequency;
    lastPf = pf;

    publishJson();
    Serial.println("JSON published");
  } else {
    Serial.println(F("PZEM read failed"));
  }

  drawStatus();
  delay(10);
}
