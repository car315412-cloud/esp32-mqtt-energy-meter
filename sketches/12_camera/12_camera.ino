#include <Wire.h>
#include <WiFi.h>
#include "esp_camera.h"

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

static const int OLED_SDA_PIN = 13;
static const int OLED_SCL_PIN = 14;
static const uint8_t OLED_ADDR = 0x3C;

static const char *WIFI_SSID = "Likung_5G";
static const char *WIFI_PASSWORD = "072882800";

static uint8_t oledBuffer[128 * 64 / 8];

void startCameraServer();
void setupLedFlash(int pin);

struct Glyph {
  char ch;
  uint8_t data[5];
};

static const Glyph GLYPHS[] = {
  { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00 } },
  { '.', { 0x00, 0x60, 0x60, 0x00, 0x00 } },
  { ':', { 0x00, 0x36, 0x36, 0x00, 0x00 } },
  { '0', { 0x3E, 0x51, 0x49, 0x45, 0x3E } },
  { '1', { 0x00, 0x42, 0x7F, 0x40, 0x00 } },
  { '2', { 0x42, 0x61, 0x51, 0x49, 0x46 } },
  { '3', { 0x21, 0x41, 0x45, 0x4B, 0x31 } },
  { '4', { 0x18, 0x14, 0x12, 0x7F, 0x10 } },
  { '5', { 0x27, 0x45, 0x45, 0x45, 0x39 } },
  { '6', { 0x3C, 0x4A, 0x49, 0x49, 0x30 } },
  { '7', { 0x01, 0x71, 0x09, 0x05, 0x03 } },
  { '8', { 0x36, 0x49, 0x49, 0x49, 0x36 } },
  { '9', { 0x06, 0x49, 0x49, 0x29, 0x1E } },
  { 'A', { 0x7E, 0x11, 0x11, 0x11, 0x7E } },
  { 'D', { 0x7F, 0x41, 0x41, 0x22, 0x1C } },
  { 'E', { 0x7F, 0x49, 0x49, 0x49, 0x41 } },
  { 'I', { 0x00, 0x41, 0x7F, 0x41, 0x00 } },
  { 'P', { 0x7F, 0x09, 0x09, 0x09, 0x06 } },
  { 'R', { 0x7F, 0x09, 0x19, 0x29, 0x46 } },
  { 'Y', { 0x07, 0x08, 0x70, 0x08, 0x07 } },
};

static const uint8_t *lookupGlyph(char ch)
{
  for (const Glyph &glyph : GLYPHS) {
    if (glyph.ch == ch) {
      return glyph.data;
    }
  }
  return GLYPHS[0].data;
}

static void oledCommand(uint8_t command)
{
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00);
  Wire.write(command);
  Wire.endTransmission();
}

static void oledInit()
{
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  Wire.setClock(400000);

  delay(100);
  oledCommand(0xAE);
  oledCommand(0xD5); oledCommand(0x80);
  oledCommand(0xA8); oledCommand(0x3F);
  oledCommand(0xD3); oledCommand(0x00);
  oledCommand(0x40);
  oledCommand(0x8D); oledCommand(0x14);
  oledCommand(0x20); oledCommand(0x00);
  oledCommand(0xA1);
  oledCommand(0xC8);
  oledCommand(0xDA); oledCommand(0x12);
  oledCommand(0x81); oledCommand(0xCF);
  oledCommand(0xD9); oledCommand(0xF1);
  oledCommand(0xDB); oledCommand(0x40);
  oledCommand(0xA4);
  oledCommand(0xA6);
  oledCommand(0xAF);
}

static void oledDisplayBuffer()
{
  for (uint8_t page = 0; page < 8; page++) {
    oledCommand(0xB0 | page);
    oledCommand(0x00);
    oledCommand(0x10);

    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x40);
    const uint16_t start = page * 128;
    for (uint16_t i = 0; i < 128; i++) {
      Wire.write(oledBuffer[start + i]);
    }
    Wire.endTransmission();
  }
}

static void oledClear()
{
  memset(oledBuffer, 0, sizeof(oledBuffer));
}

static void oledPixel(int x, int y, bool on)
{
  if (x < 0 || x >= 128 || y < 0 || y >= 64) {
    return;
  }

  const uint16_t index = x + (y / 8) * 128;
  const uint8_t mask = 1U << (y & 7);
  if (on) {
    oledBuffer[index] |= mask;
  } else {
    oledBuffer[index] &= ~mask;
  }
}

static void oledDrawChar(int x, int y, char ch)
{
  const uint8_t *glyph = lookupGlyph(ch);

  for (int col = 0; col < 5; col++) {
    uint8_t bits = glyph[col];
    for (int row = 0; row < 7; row++) {
      bool on = bits & (1U << row);
      oledPixel(x + col, y + row, on);
    }
  }
}

static void oledDrawText(int x, int y, const char *text)
{
  int cursorX = x;
  while (*text != '\0') {
    char ch = *text++;
    if (ch >= 'a' && ch <= 'z') {
      ch = (char)(ch - 'a' + 'A');
    }
    oledDrawChar(cursorX, y, ch);
    cursorX += 6;
  }
}

static void showBootScreen(const char *line1, const char *line2, const char *line3)
{
  oledClear();
  oledDrawText(0, 0, line1);
  oledDrawText(0, 16, line2);
  oledDrawText(0, 32, line3);
  oledDisplayBuffer();
}

static void showIpScreen(const IPAddress &ip)
{
  char ipText[24];
  snprintf(ipText, sizeof(ipText), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

  oledClear();
  oledDrawText(0, 0, "READY");
  oledDrawText(0, 18, "IP");
  oledDrawText(0, 34, ipText);
  oledDisplayBuffer();
}

static bool initCamera()
{
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    showBootScreen("CAMERA", "INIT", "FAILED");
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s != nullptr) {
    if (s->id.PID == OV3660_PID) {
      s->set_vflip(s, 1);
      s->set_brightness(s, 1);
      s->set_saturation(s, -2);
    }

    if (config.pixel_format == PIXFORMAT_JPEG) {
      s->set_framesize(s, FRAMESIZE_QVGA);
    }
  }

#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  return true;
}

static bool connectWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 20000UL) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  oledInit();
  showBootScreen("CAMERA", "BOOT", "START");

  if (!initCamera()) {
    while (true) {
      delay(1000);
    }
  }

  showBootScreen("WIFI", "CONNECTING", WIFI_SSID);
  if (!connectWifi()) {
    Serial.println("WiFi connect failed");
    showBootScreen("WIFI", "FAILED", "CHECK SSID");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("WiFi connected");
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  startCameraServer();
  showIpScreen(WiFi.localIP());
}

void loop()
{
  delay(10000);
}
