#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ESP32 常見 I2C 預設腳位：SDA=21, SCL=22
// 若你的 OLED 是 SSD1306 以外型號，只要改這行 constructor 即可。
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

static void drawScreen()
{
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_wqy12_t_chinese2);
    u8g2.setCursor(0, 18);
    u8g2.print("ESP32 OLED");

    u8g2.setCursor(0, 38);
    u8g2.print("中文顯示測試");

    u8g2.setCursor(0, 58);
    u8g2.print("u8g2 已安裝");
  } while (u8g2.nextPage());
}

void setup()
{
  Wire.begin(21, 22);
  u8g2.begin();
  u8g2.enableUTF8Print();
}

void loop()
{
  drawScreen();
  delay(1000);
}
