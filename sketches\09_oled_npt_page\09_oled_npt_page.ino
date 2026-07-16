/*
  OLED + DHT + NTP 分頁顯示
  按 IO0 切換頁面
  第 2 頁顯示溫濕度圖表
  第 3 頁顯示屏東站空氣品質圖表
*/

#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <U8g2lib.h>
#include <DHT.h>
#include <ArduinoJson.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

static const int PHOTO_PIN = 33;
static const int DHT_PIN = 14;
static const int BUTTON_PIN = 0;
static const int DHTTYPE = DHT11;

static const char *WIFI_SSID = "Likung_5G";
static const char *WIFI_PASSWORD = "072882800";
static const char *NTP_SERVER = "pool.ntp.org";
static const char *AQI_URL = "https://data.moenv.gov.tw/api/v2/aqx_p_432?api_key=e75b1660-e564-4107-aad5-a8be1f905dd9&limit=100&format=JSON";
static const long GMT_OFFSET_SEC = 8 * 3600;
static const int DAYLIGHT_OFFSET_SEC = 0;

DHT dht(DHT_PIN, DHTTYPE);

static int currentPage = 0;
static int lastButtonReading = HIGH;
static int buttonStableState = HIGH;
static unsigned long lastDebounceTime = 0;
static const unsigned long debounceDelay = 50;

static unsigned long lastAqiFetchMs = 0;
static const unsigned long aqiFetchIntervalMs = 10UL * 60UL * 1000UL;

static char aqiSiteName[24] = "屏東";
static char aqiStatus[24] = "--";
static int aqiValue = -1;
static int pm25Value = -1;

static int readBrightnessPercent()
{
  long sum = 0;
  const int samples = 16;

  for (int i = 0; i < samples; i++) {
    sum += analogRead(PHOTO_PIN);
    delayMicroseconds(200);
  }

  int raw = sum / samples;
  int percent = map(raw, 0, 4095, 0, 100);
  return constrain(percent, 0, 100);
}

static void formatDateString(char *buf, size_t bufSize)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 1000)) {
    snprintf(buf, bufSize, "----/--/--");
    return;
  }

  snprintf(buf, bufSize, "%04d/%02d/%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
}

static void formatTimeString(char *buf, size_t bufSize)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 1000)) {
    snprintf(buf, bufSize, "--:--:--");
    return;
  }

  snprintf(buf, bufSize, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

static void connectWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }
}

static void syncTime()
{
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  struct tm timeinfo;
  unsigned long start = millis();
  while (!getLocalTime(&timeinfo, 1000) && millis() - start < 15000) {
    delay(250);
  }
}

static int jsonToInt(const JsonVariantConst &value)
{
  if (value.isNull()) {
    return -1;
  }

  if (value.is<const char *>()) {
    const char *text = value.as<const char *>();
    if (text == nullptr || text[0] == '\0') {
      return -1;
    }
    return atoi(text);
  }

  return value.as<int>();
}

static int clampToRange(int value, int minValue, int maxValue)
{
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

static void drawProgressBar(int x, int y, int w, int h, int value, int minValue, int maxValue)
{
  int range = maxValue - minValue;
  if (range <= 0) {
    range = 1;
  }

  int clamped = clampToRange(value, minValue, maxValue);
  int fillWidth = (clamped - minValue) * (w - 2) / range;

  u8g2.drawFrame(x, y, w, h);
  if (fillWidth > 0) {
    u8g2.drawBox(x + 1, y + 1, fillWidth, h - 2);
  }
}

static bool fetchAqiData()
{
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, AQI_URL)) {
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(80 * 1024);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    return false;
  }

  JsonArray rows;
  if (doc.is<JsonArray>()) {
    rows = doc.as<JsonArray>();
  } else if (doc["records"].is<JsonArray>()) {
    rows = doc["records"].as<JsonArray>();
  } else {
    return false;
  }

  for (JsonObject row : rows) {
    const char *siteName = row["sitename"] | "";
    if (strcmp(siteName, "屏東") == 0) {
      strlcpy(aqiSiteName, siteName, sizeof(aqiSiteName));
      strlcpy(aqiStatus, row["status"] | "--", sizeof(aqiStatus));
      aqiValue = jsonToInt(row["aqi"]);
      pm25Value = jsonToInt(row["pm2.5"]);
      return true;
    }
  }

  return false;
}

static void handlePageButton()
{
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
    lastButtonReading = reading;
  }

  if ((millis() - lastDebounceTime) > debounceDelay && reading != buttonStableState) {
    buttonStableState = reading;
    if (buttonStableState == LOW) {
      currentPage = (currentPage + 1) % 3;
    }
  }
}

static void drawPage0()
{
  char dateLine[16];
  char timeLine[16];
  formatDateString(dateLine, sizeof(dateLine));
  formatTimeString(timeLine, sizeof(timeLine));

  u8g2.setCursor(0, 0);
  u8g2.print("2026秀工公民營");

  u8g2.setCursor(0, 24);
  u8g2.print("  ");
  u8g2.print(dateLine);

  u8g2.setCursor(0, 44);
  u8g2.print("  ");
  u8g2.print(timeLine);
}

static void drawPage1()
{
  int brightness = readBrightnessPercent();
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  int tempValue = isnan(temperature) ? -1 : (int)roundf(temperature);
  int humidityValue = isnan(humidity) ? -1 : (int)roundf(humidity);

  char tempLine[16];
  char humidityLine[16];

  if (tempValue < 0) {
    snprintf(tempLine, sizeof(tempLine), "--C");
  } else {
    snprintf(tempLine, sizeof(tempLine), "%dC", tempValue);
  }

  if (humidityValue < 0) {
    snprintf(humidityLine, sizeof(humidityLine), "--%%");
  } else {
    snprintf(humidityLine, sizeof(humidityLine), "%d%%", humidityValue);
  }

  u8g2.setCursor(0, 0);
  u8g2.print("溫濕度圖表");

  u8g2.setCursor(0, 12);
  u8g2.print("溫度 ");
  u8g2.print(tempLine);
  u8g2.setCursor(90, 12);
  u8g2.print(tempValue < 0 ? "--" : String(tempValue).c_str());
  u8g2.print("C");
  drawProgressBar(0, 22, 128, 7, tempValue, 0, 50);

  u8g2.setCursor(0, 31);
  u8g2.print("濕度 ");
  u8g2.print(humidityLine);
  u8g2.setCursor(90, 31);
  u8g2.print(humidityValue < 0 ? "--" : String(humidityValue).c_str());
  u8g2.print("%");
  drawProgressBar(0, 41, 128, 7, humidityValue, 0, 100);

  u8g2.setCursor(0, 52);
  u8g2.print("亮度 ");
  u8g2.print(brightness);
  u8g2.print("%");
}

static void drawPage2()
{
  u8g2.setCursor(0, 0);
  u8g2.print("空氣品質圖表");

  u8g2.setCursor(0, 12);
  u8g2.print("站名:");
  u8g2.print(aqiSiteName);

  u8g2.setCursor(0, 24);
  u8g2.print("PM2.5 ");
  if (pm25Value < 0) {
    u8g2.print("--");
  } else {
    u8g2.print(pm25Value);
  }
  u8g2.print(" ug");
  u8g2.setCursor(90, 24);
  u8g2.print(pm25Value < 0 ? "--" : String(pm25Value).c_str());
  u8g2.print("ug");
  drawProgressBar(0, 34, 128, 7, pm25Value < 0 ? 0 : pm25Value, 0, 150);

  u8g2.setCursor(0, 45);
  u8g2.print("AQI ");
  if (aqiValue < 0) {
    u8g2.print("--");
  } else {
    u8g2.print(aqiValue);
  }
  u8g2.print(" ");
  u8g2.print(aqiStatus);
  u8g2.setCursor(90, 45);
  u8g2.print(aqiValue < 0 ? "--" : String(aqiValue).c_str());
  drawProgressBar(0, 55, 128, 7, aqiValue < 0 ? 0 : aqiValue, 0, 300);
}

static void drawCurrentPage()
{
  u8g2.clearBuffer();

  if (currentPage == 0) {
    drawPage0();
  } else if (currentPage == 1) {
    drawPage1();
  } else {
    drawPage2();
  }

  u8g2.sendBuffer();
}

void setup()
{
  analogReadResolution(12);
  analogSetPinAttenuation(PHOTO_PIN, ADC_11db);
  pinMode(PHOTO_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_unifont_t_chinese1);
  u8g2.setFontPosTop();

  dht.begin();
  connectWifi();
  syncTime();
  fetchAqiData();
  lastAqiFetchMs = millis();
}

void loop()
{
  if (millis() - lastAqiFetchMs >= aqiFetchIntervalMs) {
    fetchAqiData();
    lastAqiFetchMs = millis();
  }

  handlePageButton();
  drawCurrentPage();
  delay(100);
}
