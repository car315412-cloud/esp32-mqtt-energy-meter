#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_camera.h"
#include "mbedtls/base64.h"

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// OLED wiring:
// SDA = GPIO13, SCL = GPIO14
static const int OLED_SDA_PIN = 13;
static const int OLED_SCL_PIN = 14;
static uint8_t oledAddr = 0x3C;

static const char *WIFI_SSID = "Likung_5G";
static const char *WIFI_PASSWORD = "072882800";

static const char *MQTT_HOST = "mqttgo.io";
static const uint16_t MQTT_PORT = 1883;
static const char *MQTT_TOPIC = "fukuei/class701/pic";

static const unsigned long PUBLISH_INTERVAL_MS = 1000UL;
static const unsigned long WIFI_RETRY_INTERVAL_MS = 5000UL;
static const unsigned long MQTT_RETRY_INTERVAL_MS = 5000UL;

static const uint32_t MQTT_TIMEOUT_MS = 5000UL;

static uint8_t oledBuffer[128 * 64 / 8];
static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

static unsigned long lastPublishMs = 0;
static unsigned long lastWifiRetryMs = 0;
static unsigned long lastMqttRetryMs = 0;
static unsigned long captureCount = 0;
static bool hasShownIp = false;

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
  { 'B', { 0x7F, 0x49, 0x49, 0x49, 0x36 } },
  { 'C', { 0x3E, 0x41, 0x41, 0x41, 0x22 } },
  { 'D', { 0x7F, 0x41, 0x41, 0x22, 0x1C } },
  { 'E', { 0x7F, 0x49, 0x49, 0x49, 0x41 } },
  { 'F', { 0x7F, 0x09, 0x09, 0x09, 0x01 } },
  { 'G', { 0x3E, 0x41, 0x49, 0x49, 0x3A } },
  { 'I', { 0x00, 0x41, 0x7F, 0x41, 0x00 } },
  { 'K', { 0x7F, 0x08, 0x14, 0x22, 0x41 } },
  { 'M', { 0x7F, 0x02, 0x0C, 0x02, 0x7F } },
  { 'N', { 0x7F, 0x04, 0x08, 0x10, 0x7F } },
  { 'O', { 0x3E, 0x41, 0x41, 0x41, 0x3E } },
  { 'P', { 0x7F, 0x09, 0x09, 0x09, 0x06 } },
  { 'Q', { 0x3E, 0x41, 0x51, 0x21, 0x5E } },
  { 'R', { 0x7F, 0x09, 0x19, 0x29, 0x46 } },
  { 'S', { 0x46, 0x49, 0x49, 0x49, 0x31 } },
  { 'T', { 0x01, 0x01, 0x7F, 0x01, 0x01 } },
  { 'U', { 0x3F, 0x40, 0x40, 0x40, 0x3F } },
  { 'W', { 0x7F, 0x20, 0x18, 0x20, 0x7F } },
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
  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(command);
  Wire.endTransmission();
}

static bool oledProbe(uint8_t address)
{
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

static void oledInit()
{
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  Wire.setClock(400000);

  if (oledProbe(0x3C)) {
    oledAddr = 0x3C;
  } else if (oledProbe(0x3D)) {
    oledAddr = 0x3D;
  } else {
    oledAddr = 0x3C;
  }

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

    Wire.beginTransmission(oledAddr);
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
  if (ch >= 'a' && ch <= 'z') {
    ch = (char)(ch - 'a' + 'A');
  }

  const uint8_t *glyph = lookupGlyph(ch);
  for (int col = 0; col < 5; col++) {
    uint8_t bits = glyph[col];
    for (int row = 0; row < 7; row++) {
      oledPixel(x + col, y + row, (bits & (1U << row)) != 0);
    }
  }
}

static void oledDrawText(int x, int y, const char *text)
{
  int cursorX = x;
  while (*text != '\0') {
    oledDrawChar(cursorX, y, *text++);
    cursorX += 6;
  }
}

static void oledShow(const char *line1, const char *line2, const char *line3)
{
  oledClear();
  if (line1 != nullptr) {
    oledDrawText(0, 0, line1);
  }
  if (line2 != nullptr) {
    oledDrawText(0, 16, line2);
  }
  if (line3 != nullptr) {
    oledDrawText(0, 32, line3);
  }
  oledDisplayBuffer();
}

static void oledShowIp(const IPAddress &ip)
{
  char ipText[24];
  snprintf(ipText, sizeof(ipText), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  oledShow("MQTT", "IP", ipText);
}

static void oledShowCaptureStatus(unsigned long count)
{
  char countText[24];
  snprintf(countText, sizeof(countText), "COUNT %lu", count);
  oledShow("1SEC 1PIC", countText, "MQTT PIC");
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
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 14;
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  if (psramFound()) {
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.jpeg_quality = 12;
  } else {
    config.frame_size = FRAMESIZE_QQVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.jpeg_quality = 15;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    oledShow("CAM", "FAIL", "INIT");
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s != nullptr) {
    // AI Thinker boards usually mount the module upside down.
    s->set_vflip(s, 1);
  }

#if defined(LED_GPIO_NUM)
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, LOW);
#endif

  return true;
}

static bool connectWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  oledShow("WIFI", "CONNECT", WIFI_SSID);

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 20000UL) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

static bool connectMqtt()
{
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setSocketTimeout(MQTT_TIMEOUT_MS / 1000);

  uint32_t chipId = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF);
  String clientId = "esp32cam-mqtt-" + String(chipId, HEX);

  if (!mqttClient.connect(clientId.c_str())) {
    Serial.printf("MQTT connect failed, state=%d\n", mqttClient.state());
    return false;
  }

  return true;
}

static char *allocBuffer(size_t size)
{
  char *buf = (char *)ps_malloc(size);
  if (buf == nullptr) {
    buf = (char *)malloc(size);
  }
  return buf;
}

static bool publishFrameBase64()
{
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb == nullptr) {
    Serial.println("Camera capture failed");
    return false;
  }

  size_t encodedLen = 4 * ((fb->len + 2) / 3);
  char *encoded = allocBuffer(encodedLen + 1);
  if (encoded == nullptr) {
    Serial.println("Base64 buffer alloc failed");
    esp_camera_fb_return(fb);
    return false;
  }

  size_t actualLen = 0;
  int ret = mbedtls_base64_encode(
    (unsigned char *)encoded,
    encodedLen + 1,
    &actualLen,
    fb->buf,
    fb->len
  );
  esp_camera_fb_return(fb);

  if (ret != 0) {
    Serial.printf("Base64 encode failed: %d\n", ret);
    free(encoded);
    return false;
  }

  encoded[actualLen] = '\0';

  bool ok = false;
  if (mqttClient.beginPublish(MQTT_TOPIC, actualLen, false)) {
    size_t written = mqttClient.write((const uint8_t *)encoded, actualLen);
    ok = (written == actualLen) && mqttClient.endPublish();
  }

  free(encoded);
  return ok;
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

static void ensureMqtt()
{
  if (WiFi.status() != WL_CONNECTED) {
    if (mqttClient.connected()) {
      mqttClient.disconnect();
    }
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

static void maybePublish()
{
  if (millis() - lastPublishMs < PUBLISH_INTERVAL_MS) {
    return;
  }
  lastPublishMs = millis();

  if (!mqttClient.connected()) {
    Serial.println("MQTT not connected");
    return;
  }

  if (!publishFrameBase64()) {
    Serial.println("Publish failed");
  } else {
    captureCount++;
    oledShowCaptureStatus(captureCount);
    Serial.println("Frame published");
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  oledInit();
  oledShow("CAM", "BOOT", "START");

  if (!initCamera()) {
    while (true) {
      delay(1000);
    }
  }

  if (!connectWifi()) {
    Serial.println("WiFi connect failed");
    oledShow("WIFI", "FAIL", "CHECK");
    while (true) {
      delay(1000);
    }
  }

  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());
  oledShowIp(WiFi.localIP());

  if (!connectMqtt()) {
    Serial.println("MQTT connect failed");
    oledShow("MQTT", "FAIL", "WAIT");
  }
}

void loop()
{
  ensureWifi();
  ensureMqtt();
  mqttClient.loop();

  if (WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
    maybePublish();
  }

  if (WiFi.status() == WL_CONNECTED && !hasShownIp) {
    oledShowIp(WiFi.localIP());
    hasShownIp = true;
  }

  delay(10);
}
