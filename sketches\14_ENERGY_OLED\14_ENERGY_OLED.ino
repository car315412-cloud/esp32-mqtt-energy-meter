#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static const int OLED_SDA_PIN = 21;
static const int OLED_SCL_PIN = 22;
static const int SCREEN_WIDTH = 128;
static const int SCREEN_HEIGHT = 64;
static const int OLED_RESET = -1;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static uint8_t oledAddr = 0x3C;

static bool initDisplay(uint8_t address)
{
  if (!display.begin(SSD1306_SWITCHCAPVCC, address)) {
    return false;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  return true;
}

static void drawHello(unsigned long seconds)
{
  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(F("Hello"));
  display.println(F("World"));

  display.setTextSize(1);
  display.setCursor(0, 42);
  display.print(F("I2C: "));
  display.print(oledAddr == 0x3C ? F("0x3C") : F("0x3D"));

  display.setCursor(72, 42);
  display.print(seconds);
  display.print(F(" s"));

  display.display();
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("14_ENERGY_OLED boot"));

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

  display.setRotation(2);
  drawHello(0);
}

void loop()
{
  static unsigned long lastDrawMs = 0;
  if (millis() - lastDrawMs >= 1000UL) {
    lastDrawMs = millis();
    drawHello(millis() / 1000UL);
  }
}
