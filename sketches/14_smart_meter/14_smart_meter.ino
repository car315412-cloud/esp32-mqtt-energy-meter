#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Smart meter display.
// OLED wiring matches the current AI-Thinker-style setup without the camera:
// SDA = GPIO13, SCL = GPIO14.
static const int OLED_SDA_PIN = 13;
static const int OLED_SCL_PIN = 14;
static uint8_t oledAddr = 0x3C;
static uint8_t oledBuffer[128 * 64 / 8];

// ADC input for the meter sensor. Adjust these calibration values later when
// the actual sensor and divider are confirmed.
static const int METER_PIN = 33;
static const int METER_SAMPLE_COUNT = 16;
static const int METER_RAW_MIN = 0;
static const int METER_RAW_MAX = 4095;
static const bool METER_REVERSED = false;

static const char *WIFI_SSID = "Likung_5G";
static const char *WIFI_PASSWORD = "072882800";
static const char *WIFI_HOSTNAME = "esp32-smart-meter";

static const char *MQTT_HOST = "mqttgo.io";
static const uint16_t MQTT_PORT = 1883;
static const char *MQTT_TOPIC = "fukuei/class701/meter";

static const unsigned long WIFI_RETRY_INTERVAL_MS = 5000UL;
static const unsigned long MQTT_RETRY_INTERVAL_MS = 5000UL;
static const unsigned long METER_READ_INTERVAL_MS = 1000UL;
static const unsigned long PUBLISH_INTERVAL_MS = 5000UL;
static const unsigned long OLED_REFRESH_INTERVAL_MS = 500UL;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

static unsigned long lastWifiRetryMs = 0;
static unsigned long lastMqttRetryMs = 0;
static unsigned long lastMeterReadMs = 0;
static unsigned long lastPublishMs = 0;
static unsigned long lastOledRefreshMs = 0;
static unsigned long publishCount = 0;

static int lastRaw = -1;
static int lastPercent = -1;
static float lastVoltage = 0.0f;
static bool lastPublishOk = false;
static char lastError[48] = "boot";
static char lastMqttStateText[24] = "--";

struct Glyph {
  char ch;
  uint8_t data[5];
};

static const Glyph GLYPHS[] = {
  { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00 } },
  { '%', { 0x62, 0x64, 0x08, 0x13, 0x23 } },
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
  { 'H', { 0x7F, 0x08, 0x08, 0x08, 0x7F } },
  { 'I', { 0x00, 0x41, 0x7F, 0x41, 0x00 } },
  { 'K', { 0x7F, 0x08, 0x14, 0x22, 0x41 } },
  { 'L', { 0x7F, 0x40, 0x40, 0x40, 0x40 } },
  { 'M', { 0x7F, 0x02, 0x0C, 0x02, 0x7F } },
  { 'N', { 0x7F, 0x04, 0x08, 0x10, 0x7F } },
  { 'O', { 0x3E, 0x41, 0x41, 0x41, 0x3E } },
  { 'P', { 0x7F, 0x09, 0x09, 0x09, 0x06 } },
  { 'Q', { 0x3E, 0x41, 0x51, 0x21, 0x5E } },
  { 'R', { 0x7F, 0x09, 0x19, 0x29, 0x46 } },
  { 'S', { 0x46, 0x49, 0x49, 0x49, 0x31 } },
  { 'T', { 0x01, 0x01, 0x7F, 0x01, 0x01 } },
  { 'U', { 0x3F, 0x40, 0x40, 0x40, 0x3F } },
  { 'V', { 0x1F, 0x20, 0x40, 0x20, 0x1F } },
  { 'W', { 0x7F, 0x20, 0x18, 0x20, 0x7F } },
  { 'X', { 0x63, 0x14, 0x08, 0x14, 0x63 } },
  { 'Y', { 0x07, 0x08, 0x70, 0x08, 0x07 } },
  { 'Z', { 0x61, 0x51, 0x49, 0x45, 0x43 } },
};

static void setError(const char *message)
{
  strncpy(lastError, message, sizeof(lastError) - 1);
  lastError[sizeof(lastError) - 1] = '\0';
}

static const uint8_t *lookupGlyph(char ch)
{
  for (const Glyph &glyph : GLYPHS) {
    if (glyph.ch == ch) {
      return glyph.data;
    }
  }
  return GLYPHS[0].data;
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
  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0xAE);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0xD5);
  Wire.write(0x80);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0xA8);
  Wire.write(0x3F);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0xD3);
  Wire.write(0x00);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0x40);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0x8D);
  Wire.write(0x14);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0x20);
  Wire.write(0x00);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0xA1);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0xC8);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0xDA);
  Wire.write(0x12);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0x81);
  Wire.write(0xCF);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0xD9);
  Wire.write(0xF1);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0xDB);
  Wire.write(0x40);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0xA4);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0xA6);
  Wire.endTransmission();

  Wire.beginTransmission(oledAddr);
  Wire.write(0x00);
  Wire.write(0xAF);
  Wire.endTransmission();
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

static void oledDisplayBuffer()
{
  for (uint8_t page = 0; page < 8; page++) {
    Wire.beginTransmission(oledAddr);
    Wire.write(0x00);
    Wire.write((uint8_t)(0xB0 | page));
    Wire.write(0x00);
    Wire.write(0x10);
    Wire.endTransmission();

    Wire.beginTransmission(oledAddr);
    Wire.write(0x40);
    const uint16_t start = page * 128;
    for (uint16_t i = 0; i < 128; i++) {
      Wire.write(oledBuffer[start + i]);
    }
    Wire.endTransmission();
  }
}

static void oledShowText(const char *line1, const char *line2, const char *line3)
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

static void drawStatusLine(int y, const char *label, bool ok, const char *okText, const char *failText)
{
  char buffer[48];
  snprintf(buffer, sizeof(buffer), "%s: %s", label, ok ? okText : failText);
  oledDrawText(0, y, buffer);
}

static void drawScreen()
{
  char rawText[24];
  char pctText[24];
  char voltText[24];
  char bottomText[48];

  if (lastRaw < 0) {
    snprintf(rawText, sizeof(rawText), "RAW: --");
  } else {
    snprintf(rawText, sizeof(rawText), "RAW: %d", lastRaw);
  }

  if (lastPercent < 0) {
    snprintf(pctText, sizeof(pctText), "PCT: --");
  } else {
    snprintf(pctText, sizeof(pctText), "PCT: %d%%", lastPercent);
  }

  snprintf(voltText, sizeof(voltText), "V: %.2f", lastVoltage);
  snprintf(bottomText, sizeof(bottomText), "%s %s", lastMqttStateText, lastError);

  oledClear();
  oledDrawText(0, 0, "SMART METER");
  drawStatusLine(12, "WiFi", WiFi.status() == WL_CONNECTED, "CONNECTED", "DISCONNECTED");
  drawStatusLine(22, "MQTT", mqttClient.connected(), "CONNECTED", "DISCONNECTED");
  oledDrawText(0, 34, rawText);
  oledDrawText(0, 44, pctText);
  oledDrawText(66, 44, voltText);
  oledDrawText(0, 54, bottomText);
  oledDisplayBuffer();
}

static int readMeterRaw()
{
  long sum = 0;
  for (int i = 0; i < METER_SAMPLE_COUNT; i++) {
    sum += analogRead(METER_PIN);
    delayMicroseconds(200);
  }
  return (int)(sum / METER_SAMPLE_COUNT);
}

static int rawToPercent(int raw)
{
  int bounded = constrain(raw, METER_RAW_MIN, METER_RAW_MAX);
  int percent = map(bounded, METER_RAW_MIN, METER_RAW_MAX, 0, 100);
  if (METER_REVERSED) {
    percent = 100 - percent;
  }
  return constrain(percent, 0, 100);
}

static float rawToVoltage(int raw)
{
  return (float)raw * 3.3f / 4095.0f;
}

static void readMeter()
{
  if (millis() - lastMeterReadMs < METER_READ_INTERVAL_MS) {
    return;
  }
  lastMeterReadMs = millis();

  lastRaw = readMeterRaw();
  lastPercent = rawToPercent(lastRaw);
  lastVoltage = rawToVoltage(lastRaw);
  setError("meter ok");
}

static void connectWifi()
{
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  setError("wifi connecting");
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
  String clientId = "esp32-smart-meter-" + String(chipId, HEX);

  bool ok = mqttClient.connect(clientId.c_str());
  if (!ok) {
    int state = mqttClient.state();
    snprintf(lastMqttStateText, sizeof(lastMqttStateText), "state=%d", state);
    char msg[48];
    snprintf(msg, sizeof(msg), "mqtt fail %d", state);
    setError(msg);
    return false;
  }

  snprintf(lastMqttStateText, sizeof(lastMqttStateText), "state=0");
  return true;
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

  if (!connectMqtt()) {
    return;
  }

  setError("mqtt connected");
}

static bool publishReading()
{
  if (!mqttClient.connected()) {
    return false;
  }

  unsigned long nextCount = publishCount + 1;
  char payload[128];
  snprintf(
    payload,
    sizeof(payload),
    "{\"raw\":%d,\"percent\":%d,\"voltage\":%.2f,\"count\":%lu}",
    lastRaw,
    lastPercent,
    lastVoltage,
    nextCount
  );

  bool ok = mqttClient.publish(MQTT_TOPIC, payload);
  if (!ok) {
    setError("mqtt publish fail");
  } else {
    lastPublishOk = true;
    publishCount = nextCount;
    setError("publish ok");
  }
  return ok;
}

static void maybePublish()
{
  if (millis() - lastPublishMs < PUBLISH_INTERVAL_MS) {
    return;
  }
  lastPublishMs = millis();

  readMeter();
  if (!mqttClient.connected()) {
    lastPublishOk = false;
    return;
  }

  lastPublishOk = publishReading();
}

void setup()
{
  analogReadResolution(12);
  analogSetPinAttenuation(METER_PIN, ADC_11db);
  pinMode(METER_PIN, INPUT);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  oledInit();
  drawScreen();

  connectWifi();
  mqttClient.setBufferSize(160);

  unsigned long waitStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - waitStart < 20000UL) {
    delay(200);
    yield();
  }

  if (WiFi.status() == WL_CONNECTED) {
    delay(500);
    ensureMqtt();
  }
}

void loop()
{
  ensureWifi();
  if (WiFi.status() == WL_CONNECTED) {
    ensureMqtt();
  }

  mqttClient.loop();
  readMeter();
  maybePublish();

  if (millis() - lastOledRefreshMs >= OLED_REFRESH_INTERVAL_MS) {
    lastOledRefreshMs = millis();
    drawScreen();
  }

  delay(10);
}
