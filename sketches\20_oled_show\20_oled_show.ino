#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/TomThumb.h>
#include <time.h>
#include <stdlib.h>

static const int OLED_SDA_PIN = 21;
static const int OLED_SCL_PIN = 22;
static const int SCREEN_WIDTH = 128;
static const int SCREEN_HEIGHT = 64;
static const int OLED_RESET = -1;

static const int PZEM_RX_PIN = 16;
static const int PZEM_TX_PIN = 17;
static const int RELAY_PIN = 18;
static const int SG90_PIN = 5;
static const int DHT_PIN = 14;
static const int LIGHT_PIN = 33;
static const int SG90_CHANNEL = 0;
static const int SG90_FREQ = 50;
static const int SG90_RESOLUTION = 16;
static const unsigned int SG90_MIN_US = 500;
static const unsigned int SG90_MAX_US = 2500;
static const unsigned long REFRESH_INTERVAL_MS = 10000UL;
static const unsigned long WIFI_RETRY_INTERVAL_MS = 5000UL;
static const unsigned long MQTT_RETRY_INTERVAL_MS = 5000UL;
static const unsigned long TIME_SYNC_RETRY_INTERVAL_MS = 60000UL;

static const char *WIFI_SSID = "Likung_5G";
static const char *WIFI_PASSWORD = "072882800";
static const char *WIFI_HOSTNAME = "esp32-energy-mqtt";

static const char *MQTT_HOST = "mqttgo.io";
static const uint16_t MQTT_PORT = 1883;
static const char *MQTT_TOPIC = "fukuei/class701/data";
static const char *MQTT_CTRL_TOPIC = "fukuei/class701/ctrl";
static const char *TZ_INFO = "CST-8";

#define DHTTYPE DHT11

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
DHT dht(DHT_PIN, DHTTYPE);

byte Xbuffer[50];
static uint8_t oledAddr = 0x3C;
static unsigned long lastRefreshMs = 0;
static unsigned long lastWifiRetryMs = 0;
static unsigned long lastMqttRetryMs = 0;
static unsigned long lastTimeSyncMs = 0;
static unsigned long bootStartMs = 0;
static unsigned long wifiConnectedMs = 0;
static unsigned long overlayUntilMs = 0;
static unsigned long lastDisplaySwitchMs = 0;
static float lastVoltage = NAN;
static float lastCurrent = NAN;
static float lastPower = NAN;
static float lastEnergy = NAN;
static float lastFrequency = NAN;
static float lastPf = NAN;
static float lastTemp = NAN;
static float lastHumi = NAN;
static int lastLight = -1;
static bool lastReadOk = false;
static bool lastEnvOk = false;
static bool lastPublishOk = false;
static bool relayOn = false;
static int sg90Angle = 90;
static bool wifiConnectedShown = false;
static bool normalPagesStarted = false;
static uint8_t normalPageIndex = 0;
static bool timeConfigured = false;
static bool timeShown = false;
static char overlayLine1[32] = "";
static char overlayLine2[32] = "";
static uint8_t overlaySize1 = 2;
static uint8_t overlaySize2 = 2;

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

static void setRelay(bool on)
{
  relayOn = on;
  digitalWrite(RELAY_PIN, on ? LOW : HIGH);
}

static void setSG90Angle(int angle)
{
  angle = constrain(angle, 0, 180);
  sg90Angle = angle;

  const uint32_t maxDuty = (1UL << SG90_RESOLUTION) - 1;
  const uint32_t pulseUs = map(angle, 0, 180, SG90_MIN_US, SG90_MAX_US);
  const uint32_t duty = (uint64_t)pulseUs * maxDuty / 20000UL;
  ledcWrite(SG90_CHANNEL, duty);
}

static void drawCenteredLine(const char *text, uint8_t textSize, int16_t baselineY)
{
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;

  display.setTextSize(textSize);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int16_t cursorX = (SCREEN_WIDTH - (int16_t)w) / 2 - x1;
  if (cursorX < 0) {
    cursorX = 0;
  }

  display.setCursor(cursorX, baselineY);
  display.print(text);
}

static void drawCenteredTextInArea(const char *text, int16_t areaX, int16_t areaY, int16_t areaW, int16_t areaH)
{
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;

  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int16_t cursorX = areaX + (areaW - (int16_t)w) / 2 - x1;
  int16_t cursorY = areaY + (areaH - (int16_t)h) / 2 - y1;
  if (cursorX < 0) {
    cursorX = 0;
  }
  if (cursorY < 0) {
    cursorY = 0;
  }

  display.setCursor(cursorX, cursorY);
  display.print(text);
}

static void drawDualCentered(const char *line1, uint8_t size1, const char *line2, uint8_t size2)
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  drawCenteredLine(line1, size1, size1 == 1 ? 16 : 10);
  if (line2 != nullptr && line2[0] != '\0') {
    drawCenteredLine(line2, size2, size2 == 1 ? 44 : 36);
  }
  display.display();
}

static void drawWiFiIcon(int16_t x, int16_t y, bool filled)
{
  display.drawCircle(x + 5, y + 9, 2, SSD1306_WHITE);
  display.drawCircle(x + 5, y + 9, 5, SSD1306_WHITE);
  display.drawCircle(x + 5, y + 9, 8, SSD1306_WHITE);
  if (filled) {
    display.fillCircle(x + 5, y + 9, 1, SSD1306_WHITE);
  }
}

static void drawMqttIcon(int16_t x, int16_t y, bool filled)
{
  display.drawLine(x + 5, y + 2, x + 5, y + 15, SSD1306_WHITE);
  display.drawLine(x + 1, y + 6, x + 5, y + 2, SSD1306_WHITE);
  display.drawLine(x + 9, y + 6, x + 5, y + 2, SSD1306_WHITE);
  display.drawLine(x + 1, y + 10, x + 5, y + 6, SSD1306_WHITE);
  display.drawLine(x + 9, y + 10, x + 5, y + 6, SSD1306_WHITE);
  if (filled) {
    display.fillCircle(x + 5, y + 13, 2, SSD1306_WHITE);
  } else {
    display.drawCircle(x + 5, y + 13, 2, SSD1306_WHITE);
  }
}

static void drawBoltIcon(int16_t x, int16_t y)
{
  display.fillTriangle(x + 9, y + 0, x + 3, y + 9, x + 8, y + 9, SSD1306_WHITE);
  display.fillTriangle(x + 7, y + 8, x + 13, y + 8, x + 6, y + 16, SSD1306_WHITE);
  display.fillTriangle(x + 7, y + 5, x + 14, y + 5, x + 9, y + 11, SSD1306_WHITE);
}

static void drawThermoIcon(int16_t x, int16_t y)
{
  display.drawRoundRect(x + 4, y + 1, 4, 11, 2, SSD1306_WHITE);
  display.fillCircle(x + 6, y + 14, 3, SSD1306_WHITE);
  display.drawLine(x + 6, y + 3, x + 6, y + 11, SSD1306_WHITE);
}

static void drawDropIcon(int16_t x, int16_t y)
{
  display.drawCircle(x + 6, y + 8, 4, SSD1306_WHITE);
  display.fillTriangle(x + 2, y + 8, x + 10, y + 8, x + 6, y + 0, SSD1306_WHITE);
}

static void drawSunIcon(int16_t x, int16_t y)
{
  display.drawCircle(x + 6, y + 8, 3, SSD1306_WHITE);
  display.drawLine(x + 6, y + 0, x + 6, y + 3, SSD1306_WHITE);
  display.drawLine(x + 6, y + 13, x + 6, y + 16, SSD1306_WHITE);
  display.drawLine(x + 0, y + 8, x + 3, y + 8, SSD1306_WHITE);
  display.drawLine(x + 9, y + 8, x + 12, y + 8, SSD1306_WHITE);
  display.drawLine(x + 1, y + 3, x + 3, y + 5, SSD1306_WHITE);
  display.drawLine(x + 9, y + 3, x + 7, y + 5, SSD1306_WHITE);
  display.drawLine(x + 1, y + 13, x + 3, y + 11, SSD1306_WHITE);
  display.drawLine(x + 9, y + 13, x + 7, y + 11, SSD1306_WHITE);
}

static void drawGaugeIcon(int16_t x, int16_t y)
{
  display.drawCircle(x + 6, y + 8, 6, SSD1306_WHITE);
  display.drawLine(x + 6, y + 8, x + 10, y + 4, SSD1306_WHITE);
}

static void drawBatteryIcon(int16_t x, int16_t y)
{
  display.drawRect(x + 1, y + 3, 10, 8, SSD1306_WHITE);
  display.fillRect(x + 11, y + 5, 2, 4, SSD1306_WHITE);
}

static void drawGridText(int16_t x, int16_t y, const char *text, uint8_t size = 1)
{
  display.setTextSize(size);
  display.setCursor(x, y);
  display.print(text);
}

static bool getTimeText(char *buffer, size_t length)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) {
    return false;
  }

  strftime(buffer, length, "%m/%d", &timeinfo);
  return true;
}

static void startTimeSync()
{
  if (timeConfigured || WiFi.status() != WL_CONNECTED) {
    return;
  }

  configTzTime(TZ_INFO, "pool.ntp.org", "time.nist.gov");
  timeConfigured = true;
  lastTimeSyncMs = millis();
}

static void drawHeaderBar()
{
  char dateText[16];
  const bool wifiOk = WiFi.status() == WL_CONNECTED;
  const bool mqttOk = mqttClient.connected();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setFont(NULL);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print(wifiOk ? "WiFi:OK" : "WiFi:--");

  if (getTimeText(dateText, sizeof(dateText))) {
    display.setCursor(44, 0);
    display.print(dateText);
  } else {
    display.setCursor(42, 0);
    display.print("--/--");
  }

  display.setCursor(86, 0);
  display.print(mqttOk ? "MQTT:OK" : "MQTT:--");
}

static void showOverlay(const char *line1, const char *line2, uint8_t size1, uint8_t size2, unsigned long durationMs)
{
  strncpy(overlayLine1, line1, sizeof(overlayLine1) - 1);
  overlayLine1[sizeof(overlayLine1) - 1] = '\0';

  if (line2 != nullptr) {
    strncpy(overlayLine2, line2, sizeof(overlayLine2) - 1);
    overlayLine2[sizeof(overlayLine2) - 1] = '\0';
  } else {
    overlayLine2[0] = '\0';
  }

  overlaySize1 = size1;
  overlaySize2 = size2;
  overlayUntilMs = millis() + durationMs;
}

static void drawBootScreen()
{
  drawDualCentered("System", 2, "Starting...", 2);
}

static void drawWifiScreen()
{
  drawDualCentered("WiFi", 2, "connecting...", 2);
}

static void drawIpScreen()
{
  String ip = WiFi.localIP().toString();
  drawDualCentered("IP:", 2, ip.c_str(), 1);
}

static void drawPowerVIWPage()
{
  drawHeaderBar();
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);

  if (lastReadOk) {
    char vLine[20];
    char iLine[20];
    char wLine[20];
    snprintf(vLine, sizeof(vLine), "V:%.1fv", lastVoltage);
    snprintf(iLine, sizeof(iLine), "I:%.2fA", lastCurrent);
    snprintf(wLine, sizeof(wLine), "W:%.1fw", lastPower);
    drawCenteredTextInArea(vLine, 0, 13, 128, 14);
    drawCenteredTextInArea(iLine, 0, 29, 128, 14);
    drawCenteredTextInArea(wLine, 0, 45, 128, 14);
  } else {
    drawCenteredTextInArea("V:-!!!", 0, 13, 128, 14);
    drawCenteredTextInArea("I:-!!!", 0, 29, 128, 14);
    drawCenteredTextInArea("W:-!!!", 0, 45, 128, 14);
  }
  display.setFont(NULL);
  display.display();
}

static void drawPowerKWhPage()
{
  drawHeaderBar();
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);

  drawBoltIcon(4, 22);
  if (lastReadOk) {
    char kLine[20];
    snprintf(kLine, sizeof(kLine), "%04.0fKWh", lastEnergy);
    drawCenteredTextInArea(kLine, 16, 16, 112, 40);
  } else {
    display.setFont(&FreeSans9pt7b);
    drawCenteredTextInArea("----KWh", 16, 18, 112, 36);
  }

  display.setFont(NULL);
  display.display();
}

static void drawEnvPage()
{
  drawHeaderBar();
  display.setFont(&FreeSans12pt7b);
  display.setTextSize(1);

  drawThermoIcon(4, 20);
  if (lastEnvOk) {
    char tempLine[20];
    snprintf(tempLine, sizeof(tempLine), "Temp:%.0f\xC2\xB0" "C", lastTemp);
    drawCenteredTextInArea(tempLine, 16, 16, 112, 32);
  } else {
    drawCenteredTextInArea("Temp:-!!!", 16, 16, 112, 32);
  }
  display.setFont(NULL);
  display.setTextSize(1);
  if (lastEnvOk) {
    char infoLine[32];
    snprintf(infoLine, sizeof(infoLine), "Humi:%.0f%%  Light:%d%%", lastHumi, lastLight);
    drawGridText(0, 54, infoLine, 1);
  } else {
    drawGridText(0, 54, "Humi:-!!!  Light:-!!!", 1);
  }
  display.setFont(NULL);
  display.display();
}

static void renderDisplay()
{
  unsigned long now = millis();

  if (now < bootStartMs + 1800UL) {
    drawBootScreen();
    return;
  }

  if (overlayUntilMs != 0 && now < overlayUntilMs) {
    drawDualCentered(overlayLine1, overlaySize1, overlayLine2, overlaySize2);
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    wifiConnectedShown = false;
    normalPagesStarted = false;
    drawWifiScreen();
    return;
  }

  if (!timeConfigured && (millis() - lastTimeSyncMs >= TIME_SYNC_RETRY_INTERVAL_MS)) {
    startTimeSync();
  }

  if (!wifiConnectedShown) {
    wifiConnectedShown = true;
    wifiConnectedMs = now;
    normalPagesStarted = false;
    startTimeSync();
  }

  if (now < wifiConnectedMs + 3000UL) {
    drawIpScreen();
    return;
  }

  if (!normalPagesStarted) {
    normalPagesStarted = true;
    lastDisplaySwitchMs = now;
    normalPageIndex = 0;
  }

  if (now - lastDisplaySwitchMs >= 5000UL) {
    lastDisplaySwitchMs = now;
    normalPageIndex = (normalPageIndex + 1) % 3;
  }

  if (normalPageIndex == 0) {
    drawPowerVIWPage();
  } else if (normalPageIndex == 1) {
    drawPowerKWhPage();
  } else {
    drawEnvPage();
  }
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

static int readLightPercent()
{
  int raw = analogRead(LIGHT_PIN);
  int percent = map(raw, 0, 4095, 100, 0);
  return constrain(percent, 0, 100);
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

static bool readEnvironmentValues(float &temp, float &humi, int &lightPercent)
{
  lightPercent = readLightPercent();

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    temp = NAN;
    humi = NAN;
    return false;
  }

  temp = t;
  humi = h;
  return true;
}

static void handleSG90Command(const JsonVariantConst &value)
{
  if (value.isNull()) {
    return;
  }

  int angle = 0;
  if (value.is<int>()) {
    angle = value.as<int>();
  } else if (value.is<float>()) {
    angle = (int)roundf(value.as<float>());
  } else {
    return;
  }

  if (angle < 0 || angle > 180) {
    return;
  }

  setSG90Angle(angle);
  Serial.print(F("SG90="));
  Serial.println(sg90Angle);
  char line1[32];
  char line2[32];
  snprintf(line1, sizeof(line1), "Servo");
  snprintf(line2, sizeof(line2), "%d deg", sg90Angle);
  showOverlay(line1, line2, 2, 2, 3000UL);
}

static void handleRelayCommand(const JsonVariantConst &value)
{
  if (value.isNull()) {
    return;
  }

  if (value.is<bool>()) {
    bool on = value.as<bool>();
    setRelay(on);
    Serial.print(F("RELAY="));
    Serial.println(on ? F("ON") : F("OFF"));
    showOverlay("Relay", on ? "ON" : "OFF", 2, 2, 3000UL);
    return;
  }

  if (value.is<int>()) {
    int raw = value.as<int>();
    if (raw == 1) {
      setRelay(true);
      Serial.println(F("RELAY=ON"));
      showOverlay("Relay", "ON", 2, 2, 3000UL);
    } else if (raw == 0) {
      setRelay(false);
      Serial.println(F("RELAY=OFF"));
      showOverlay("Relay", "OFF", 2, 2, 3000UL);
    }
    return;
  }

  const char *state = value.as<const char *>();
  if (state == nullptr) {
    return;
  }

  if (strcasecmp(state, "ON") == 0 || strcasecmp(state, "1") == 0 || strcasecmp(state, "TRUE") == 0) {
    setRelay(true);
    Serial.println(F("RELAY=ON"));
    showOverlay("Relay", "ON", 2, 2, 3000UL);
  } else if (strcasecmp(state, "OFF") == 0 || strcasecmp(state, "0") == 0 || strcasecmp(state, "FALSE") == 0) {
    setRelay(false);
    Serial.println(F("RELAY=OFF"));
    showOverlay("Relay", "OFF", 2, 2, 3000UL);
  }
}

static void handleControlCommand(const char *payload, size_t length)
{
  DynamicJsonDocument doc(128);
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    return;
  }

  if (doc.containsKey("RELAY")) {
    handleRelayCommand(doc["RELAY"]);
  }

  if (doc.containsKey("SG90")) {
    handleSG90Command(doc["SG90"]);
  }
}

static void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  if (strcmp(topic, MQTT_CTRL_TOPIC) != 0) {
    return;
  }

  char message[160];
  unsigned int copyLength = length;
  if (copyLength >= sizeof(message)) {
    copyLength = sizeof(message) - 1;
  }
  memcpy(message, payload, copyLength);
  message[copyLength] = '\0';
  Serial.print(F("MQTT CTRL: "));
  Serial.println(message);
  handleControlCommand(message, copyLength);
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
  mqttClient.setCallback(mqttCallback);

  uint32_t chipId = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF);
  String clientId = "esp32-energy-mqtt-relay-" + String(chipId, HEX);
  bool ok = mqttClient.connect(clientId.c_str());
  if (ok) {
    mqttClient.subscribe(MQTT_CTRL_TOPIC);
    Serial.print(F("Subscribed: "));
    Serial.println(MQTT_CTRL_TOPIC);
  }
  return ok;
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

  char payload[192];
  snprintf(
    payload,
    sizeof(payload),
    "{\"V\":%.1f,\"I\":%.2f,\"W\":%.1f,\"kWh\":%.3f,\"temp\":%.0f,\"humi\":%.0f,\"light\":%d}",
    lastVoltage,
    lastCurrent,
    lastPower,
    lastEnergy,
    isnan(lastTemp) ? -1.0f : lastTemp,
    isnan(lastHumi) ? -1.0f : lastHumi,
    lastLight
  );

  bool ok = mqttClient.publish(MQTT_TOPIC, payload);
  lastPublishOk = ok;
  return ok;
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("17_energy_mqtt_relay boot"));
  bootStartMs = millis();

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

  pinMode(RELAY_PIN, OUTPUT);
  setRelay(true);

  ledcSetup(SG90_CHANNEL, SG90_FREQ, SG90_RESOLUTION);
  ledcAttachPin(SG90_PIN, SG90_CHANNEL);
  setSG90Angle(90);

  analogReadResolution(12);
  analogSetPinAttenuation(LIGHT_PIN, ADC_11db);
  dht.begin();

  Serial2.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
  ClearData();
  connectWifi();
  drawBootScreen();
}

void loop()
{
  ensureWifi();
  ensureMqtt();
  mqttClient.loop();

  if (millis() - lastRefreshMs >= REFRESH_INTERVAL_MS) {
    lastRefreshMs = millis();

    float voltage = 0;
    float current = 0;
    float power = 0;
    float energy = 0;
    float frequency = 0;
    float pf = 0;
    float temp = NAN;
    float humi = NAN;
    int lightPercent = -1;

    lastReadOk = readPzemValues(voltage, current, power, energy, frequency, pf);
    lastEnvOk = readEnvironmentValues(temp, humi, lightPercent);

    if (lastReadOk) {
      lastVoltage = voltage;
      lastCurrent = current;
      lastPower = power;
      lastEnergy = energy;
      lastFrequency = frequency;
      lastPf = pf;
    }

    if (lastEnvOk) {
      lastTemp = temp;
      lastHumi = humi;
    } else {
      lastTemp = NAN;
      lastHumi = NAN;
    }
    lastLight = lightPercent;

    if (lastReadOk) {
      publishJson();
    }
  }

  renderDisplay();

  delay(10);
}
