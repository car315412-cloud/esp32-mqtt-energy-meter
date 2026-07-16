/*
  縮網址：twgo.io/oleds
  此為OLED 範例程式，須使用U8G2程式庫
  光敏接在 IO33，亮度以百分比顯示
  溫溼度感測器接在 IO14
*/

#include "Wire.h"
#include "U8g2lib.h"
#include <DHT.h>

// 0.96 吋 SSD1306 128x64 I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

static const int PHOTO_PIN = 33;
static const int DHT_PIN = 14;
static const int DHTTYPE = DHT11;  // 若你的感測器是 DHT22，改成 DHT22

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
}

void loop()
{
  int brightness = readBrightnessPercent();
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  char thLine[24];
  formatTemperatureHumidity(thLine, sizeof(thLine), temperature, humidity);

  u8g2.clearBuffer();

  u8g2.setCursor(0, 5);
  u8g2.print("2027秀工公民營");

  u8g2.setCursor(0, 25);
  u8g2.print("亮度：");
  u8g2.print(brightness);
  u8g2.print(" %");

  u8g2.setCursor(0, 45);
  u8g2.print("溫溼度：");
  u8g2.print(thLine);

  u8g2.sendBuffer();
  delay(2000);
}
