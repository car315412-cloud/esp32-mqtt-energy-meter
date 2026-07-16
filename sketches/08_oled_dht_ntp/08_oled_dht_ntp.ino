/*
  OLED + DHT + NTP 時鐘
  第 1 排顯示 NTP 現在時間
  第 2 排顯示光照百分比
  第 3 排顯示溫濕度
*/

#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <U8g2lib.h>
#include <DHT.h>

// 0.96 吋 SSD1306 128x64 I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

static const int PHOTO_PIN = 33;
static const int DHT_PIN = 14;
static const int DHTTYPE = DHT11;  // 若改 DHT22，請改這裡

static const char *WIFI_SSID = "Likung_5G";
static const char *WIFI_PASSWORD = "072882800";
static const char *NTP_SERVER = "pool.ntp.org";
static const long GMT_OFFSET_SEC = 8 * 3600;
static const int DAYLIGHT_OFFSET_SEC = 0;

DHT dht(DHT_PIN, DHTTYPE);

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

static void formatTemperatureHumidity(char *buf, size_t bufSize, float temperature, float humidity)
{
  if (isnan(temperature) || isnan(humidity)) {
    snprintf(buf, bufSize, "--C/--%%");
    return;
  }

  int t = (int)roundf(temperature);
  int h = (int)roundf(humidity);
  snprintf(buf, bufSize, "%dC/%d%%", t, h);
}

static void formatTimeString(char *buf, size_t bufSize)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 1000)) {
    snprintf(buf, bufSize, "--/-- --:--:--");
    return;
  }

  snprintf(
    buf,
    bufSize,
    "%02d/%02d %02d:%02d:%02d",
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  );
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

void setup()
{
  analogReadResolution(12);
  analogSetPinAttenuation(PHOTO_PIN, ADC_11db);
  pinMode(PHOTO_PIN, INPUT);

  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_unifont_t_chinese1);
  u8g2.setFontPosTop();

  dht.begin();
  connectWifi();
  syncTime();
}

void loop()
{
  int brightness = readBrightnessPercent();
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  char timeLine[24];
  char thLine[24];
  formatTimeString(timeLine, sizeof(timeLine));
  formatTemperatureHumidity(thLine, sizeof(thLine), temperature, humidity);

  u8g2.clearBuffer();

  u8g2.setCursor(0, 5);
  u8g2.print(timeLine);

  u8g2.setCursor(0, 25);
  u8g2.print("亮度:");
  u8g2.print(brightness);
  u8g2.print(" %");

  u8g2.setCursor(0, 45);
  u8g2.print("溫濕度:");
  u8g2.print(thLine);

  u8g2.sendBuffer();
  delay(1000);
}
