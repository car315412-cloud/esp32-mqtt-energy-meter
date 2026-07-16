#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdlib.h>

static const int OLED_SDA_PIN = 21;
static const int OLED_SCL_PIN = 22;
static const int SCREEN_WIDTH = 128;
static const int SCREEN_HEIGHT = 64;
static const int OLED_RESET = -1;

static const int PZEM_RX_PIN = 16;
static const int PZEM_TX_PIN = 17;
static const unsigned long REFRESH_INTERVAL_MS = 10000UL;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

byte Xbuffer[50];
static uint8_t oledAddr = 0x3C;
static unsigned long lastRefreshMs = 0;
static float lastVoltage = NAN;
static float lastCurrent = NAN;
static float lastPower = NAN;
static float lastEnergy = NAN;
static float lastFrequency = NAN;
static float lastPf = NAN;

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

static void drawError(const char *message)
{
  (void)message;
  drawTextScreen("15 ENERGY", "V=-!!! I=-!!!", "W=-!!! kWh=-!!!", "Hz=-!!! PF=-!!!");
}

static void drawEnergyScreen(float voltage, float current, float power, float energy, float frequency, float pf)
{
  char line1[32];
  char line2[32];
  char line3[32];

  snprintf(line1, sizeof(line1), "V=%.0fV, I=%.2fA", voltage, current);
  snprintf(line2, sizeof(line2), "W=%.1fW, kWh=%.3f", power, energy);
  snprintf(line3, sizeof(line3), "Hz=%.1f PF=%.0f%%", frequency, pf * 100.0f);
  drawTextScreen("15 ENERGY", line1, line2, line3);
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

static float decodePF(const String &rtext)
{
  char CharPF[5];
  String StringPF = rtext.substring(57, 59) + rtext.substring(60, 62);
  StringPF.toCharArray(CharPF, sizeof(CharPF));
  int intPF = strtol(CharPF, NULL, 16);
  return (float)intPF / 100.0f;
}

static float decodeFrequency(const String &rtext)
{
  char CharHz[5];
  String StringHz = rtext.substring(49, 51) + rtext.substring(52, 54);
  StringHz.toCharArray(CharHz, sizeof(CharHz));
  int intHz = strtol(CharHz, NULL, 16);
  return intHz * 0.1f;
}

static float *decodeALL(const String &rtext)
{
  static float enData[6] = { 0, 0, 0, 0, 0, 0 };
  if (rtext.length() > 0) {
    enData[0] = decodeV(rtext);
    enData[1] = decodeA(rtext);
    enData[2] = decodeW(rtext);
    enData[3] = decodekWh(rtext);
    enData[4] = decodeFrequency(rtext);
    enData[5] = decodePF(rtext);
  }
  return enData;
}

static bool readPzemValues(float &voltage, float &current, float &power, float &energy, float &frequency, float &pf)
{
  String rtext = ReadData();
  float *enData = decodeALL(rtext);

  if (rtext.length() == 0) {
    return false;
  }

  voltage = enData[0];
  current = enData[1];
  power = enData[2];
  energy = enData[3];
  frequency = enData[4];
  pf = enData[5];
  return true;
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("15_ENERGY boot"));

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
  drawTextScreen("15 ENERGY", "Starting...", "Waiting PZEM", "Hz=-- PF=--");
  ClearData();
}

void loop()
{
  if (millis() - lastRefreshMs < REFRESH_INTERVAL_MS) {
    return;
  }
  lastRefreshMs = millis();

  float voltage = 0;
  float current = 0;
  float power = 0;
  float energy = 0;
  float frequency = 0;
  float pf = 0;

  if (readPzemValues(voltage, current, power, energy, frequency, pf)) {
    lastVoltage = voltage;
    lastCurrent = current;
    lastPower = power;
    lastEnergy = energy;
    lastFrequency = frequency;
    lastPf = pf;

    drawEnergyScreen(lastVoltage, lastCurrent, lastPower, lastEnergy, lastFrequency, lastPf);
    Serial.println("電壓=" + String(lastVoltage) + " V,電流=" + String(lastCurrent) + " A,瓦特=" + String(lastPower) + " W,度數=" + String(lastEnergy) + " kWh,頻率=" + String(lastFrequency) + " Hz,PF=" + String(lastPf * 100.0f) + "%");
  } else {
    drawError("check wiring");
    Serial.println(F("PZEM read failed"));
  }
}
