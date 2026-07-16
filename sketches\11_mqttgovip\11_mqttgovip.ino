#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <U8g2lib.h>
#include <DHT.h>
#include <ArduinoJson.h>

// OLED: SSD1306 128x64 I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

static const int PHOTO_PIN = 33;
static const int DHT_PIN = 14;
static const int DHTTYPE = DHT11;  // Change to DHT22 if needed
static const int RED_PIN = 4;
static const int YELLOW_PIN = 2;
static const int GREEN_PIN = 15;

static const char *WIFI_SSID = "Likung_5G";
static const char *WIFI_PASSWORD = "072882800";
static const char *WIFI_HOSTNAME = "esp32-mqtt-govip";

static const char *MQTT_HOST = "mqttgo.vip";
static const uint16_t MQTT_PORT = 8883;
static const char *MQTT_USERNAME = "car1211g";
static const char *MQTT_PASSWORD = "1211Gcar";
static const char *MQTT_TOPIC = "/user/car1211g/class701/data";
static const char *MQTT_CTRL_TOPIC = "/user/car1211g/class701/ctrl";

// Set to true for certificate verification, false for test connection.
static const bool USE_INSECURE_TLS = false;

static const unsigned long PUBLISH_INTERVAL_MS = 10000UL;
static const unsigned long WIFI_RETRY_INTERVAL_MS = 5000UL;
static const unsigned long MQTT_RETRY_INTERVAL_MS = 5000UL;
static const unsigned long DHT_READ_INTERVAL_MS = 2000UL;
static const unsigned long OLED_REFRESH_INTERVAL_MS = 500UL;

// Let's Encrypt ISRG Root X1
// Source: https://letsencrypt.org/certs/isrgrootx1.pem.txt
static const char MQTT_ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);
DHT dht(DHT_PIN, DHTTYPE);

static unsigned long lastPublishMs = 0;
static unsigned long lastWifiRetryMs = 0;
static unsigned long lastMqttRetryMs = 0;
static unsigned long lastDhtReadMs = 0;
static unsigned long lastOledRefreshMs = 0;

static int lastTemp = -1;
static int lastHumi = -1;
static int lastLight = -1;
static bool lastDhtOk = false;
static bool lastPublishOk = false;
static char lastError[48] = "boot";
static char lastMqttStateText[24] = "--";
static bool redLedOn = false;
static bool yellowLedOn = false;
static bool greenLedOn = false;

static void setError(const char *message)
{
  strncpy(lastError, message, sizeof(lastError) - 1);
  lastError[sizeof(lastError) - 1] = '\0';
}

static void applyLedState(int pin, bool on)
{
  digitalWrite(pin, on ? HIGH : LOW);
}

static void refreshLeds()
{
  applyLedState(RED_PIN, redLedOn);
  applyLedState(YELLOW_PIN, yellowLedOn);
  applyLedState(GREEN_PIN, greenLedOn);
}

static void setLedStateByKey(const char *key, bool on)
{
  if (strcmp(key, "RLED") == 0) {
    redLedOn = on;
    applyLedState(RED_PIN, redLedOn);
  } else if (strcmp(key, "YLED") == 0) {
    yellowLedOn = on;
    applyLedState(YELLOW_PIN, yellowLedOn);
  } else if (strcmp(key, "GLED") == 0) {
    greenLedOn = on;
    applyLedState(GREEN_PIN, greenLedOn);
  }
}

static bool parseOnOffValue(JsonVariantConst value, bool &on)
{
  if (value.is<bool>()) {
    on = value.as<bool>();
    return true;
  }

  const char *text = value.as<const char *>();
  if (text == nullptr) {
    return false;
  }

  if (strcasecmp(text, "ON") == 0 || strcasecmp(text, "1") == 0 || strcasecmp(text, "TRUE") == 0) {
    on = true;
    return true;
  }

  if (strcasecmp(text, "OFF") == 0 || strcasecmp(text, "0") == 0 || strcasecmp(text, "FALSE") == 0) {
    on = false;
    return true;
  }

  return false;
}

static void handleControlPayload(const char *payload, size_t length)
{
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    setError("ctrl json fail");
    return;
  }

  bool changed = false;
  const char *keys[] = { "GLED", "YLED", "RLED" };
  for (const char *key : keys) {
    if (!doc.containsKey(key)) {
      continue;
    }

    bool on = false;
    if (!parseOnOffValue(doc[key], on)) {
      setError("ctrl value bad");
      continue;
    }

    setLedStateByKey(key, on);
    changed = true;
  }

  if (changed) {
    setError("ctrl ok");
  }
}

static int readLightPercent()
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

static void readSensors()
{
  if (millis() - lastDhtReadMs < DHT_READ_INTERVAL_MS) {
    return;
  }
  lastDhtReadMs = millis();

  float temp = dht.readTemperature();
  float humi = dht.readHumidity();
  int light = readLightPercent();

  lastLight = light;

  if (isnan(temp) || isnan(humi)) {
    lastDhtOk = false;
    lastTemp = -1;
    lastHumi = -1;
    setError("dht read failed");
    return;
  }

  lastDhtOk = true;
  lastTemp = (int)roundf(temp);
  lastHumi = (int)roundf(humi);
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

static void configureTls()
{
  if (USE_INSECURE_TLS) {
    wifiClient.setInsecure();
  } else {
    wifiClient.setCACert(MQTT_ROOT_CA);
  }
}

static bool connectMqtt()
{
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  if (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    setError("wifi no ip");
    return false;
  }

  configureTls();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setSocketTimeout(10);
  mqttClient.setKeepAlive(60);

  uint32_t chipId = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF);
  String clientId = "esp32-mqtt-govip-" + String(chipId, HEX);

  bool ok = mqttClient.connect(
    clientId.c_str(),
    MQTT_USERNAME,
    MQTT_PASSWORD
  );

  if (!ok) {
    int state = mqttClient.state();
    snprintf(lastMqttStateText, sizeof(lastMqttStateText), "state=%d", state);
    char msg[48];
    snprintf(msg, sizeof(msg), "mqtt fail %d", state);
    setError(msg);
    return false;
  }

  mqttClient.subscribe(MQTT_CTRL_TOPIC);
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

  char payload[96];
  snprintf(
    payload,
    sizeof(payload),
    "{\"temp\":%d,\"humi\":%d,\"light\":%d}",
    lastTemp,
    lastHumi,
    lastLight
  );

  bool ok = mqttClient.publish(MQTT_TOPIC, payload);
  if (!ok) {
    setError("mqtt publish fail");
  } else {
    lastPublishOk = true;
    setError("publish ok");
  }
  return ok;
}

static void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  if (strcmp(topic, MQTT_CTRL_TOPIC) != 0) {
    return;
  }

  char message[257];
  unsigned int copyLength = length;
  if (copyLength >= sizeof(message)) {
    copyLength = sizeof(message) - 1;
  }

  memcpy(message, payload, copyLength);
  message[copyLength] = '\0';
  handleControlPayload(message, copyLength);
}

static void maybePublish()
{
  if (millis() - lastPublishMs < PUBLISH_INTERVAL_MS) {
    return;
  }
  lastPublishMs = millis();

  readSensors();
  if (!mqttClient.connected()) {
    lastPublishOk = false;
    return;
  }

  publishReading();
}

static void drawStatusLine(int y, const char *label, bool ok, const char *okText, const char *failText)
{
  u8g2.setCursor(0, y);
  u8g2.print(label);
  u8g2.print(": ");
  u8g2.print(ok ? okText : failText);
}

static void drawScreen()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontPosTop();

  u8g2.setCursor(0, 0);
  u8g2.print("11 MQTT GOVIP");

  drawStatusLine(14, "WiFi", WiFi.status() == WL_CONNECTED, "CONNECTED", "DISCONNECTED");
  drawStatusLine(24, "MQTT", mqttClient.connected(), "CONNECTED", "DISCONNECTED");

  u8g2.setCursor(0, 36);
  u8g2.print("T:");
  if (lastTemp < 0) {
    u8g2.print("--");
  } else {
    u8g2.print(lastTemp);
  }
  u8g2.print("C  H:");
  if (lastHumi < 0) {
    u8g2.print("--");
  } else {
    u8g2.print(lastHumi);
  }
  u8g2.print("%");

  u8g2.setCursor(0, 46);
  u8g2.print("Light:");
  if (lastLight < 0) {
    u8g2.print("--");
  } else {
    u8g2.print(lastLight);
  }
  u8g2.print("%");

  u8g2.setCursor(0, 56);
  u8g2.print("LED:");
  u8g2.print(redLedOn ? "R1" : "R0");
  u8g2.print(" ");
  u8g2.print(yellowLedOn ? "Y1" : "Y0");
  u8g2.print(" ");
  u8g2.print(greenLedOn ? "G1" : "G0");

  u8g2.setCursor(84, 56);
  u8g2.print(lastMqttStateText);

  u8g2.sendBuffer();
}

void setup()
{
  analogReadResolution(12);
  analogSetPinAttenuation(PHOTO_PIN, ADC_11db);
  pinMode(PHOTO_PIN, INPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  refreshLeds();

  u8g2.begin();

  dht.begin();
  connectWifi();
  mqttClient.setBufferSize(128);
  mqttClient.setCallback(mqttCallback);

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
  readSensors();
  maybePublish();

  if (millis() - lastOledRefreshMs >= OLED_REFRESH_INTERVAL_MS) {
    lastOledRefreshMs = millis();
    drawScreen();
  }

  delay(10);
}
