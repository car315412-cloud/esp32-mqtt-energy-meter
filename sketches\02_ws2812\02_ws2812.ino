#include "Arduino.h"
#include "esp32-hal.h"

#define WS2812_PIN 32
#define WS2812_BITS 24

rmt_data_t led_data[WS2812_BITS];
rmt_obj_t *rmt_send = NULL;

void fillColor(uint8_t red, uint8_t green, uint8_t blue) {
  uint8_t colors[3] = {green, red, blue};
  int bitIndex = 0;

  for (int col = 0; col < 3; col++) {
    for (int bit = 0; bit < 8; bit++) {
      if (colors[col] & (1 << (7 - bit))) {
        led_data[bitIndex].level0 = 1;
        led_data[bitIndex].duration0 = 8;
        led_data[bitIndex].level1 = 0;
        led_data[bitIndex].duration1 = 4;
      } else {
        led_data[bitIndex].level0 = 1;
        led_data[bitIndex].duration0 = 4;
        led_data[bitIndex].level1 = 0;
        led_data[bitIndex].duration1 = 8;
      }
      bitIndex++;
    }
  }
}

void showColor(uint8_t red, uint8_t green, uint8_t blue, unsigned long waitMs) {
  fillColor(red, green, blue);
  rmtWrite(rmt_send, led_data, WS2812_BITS);
  delay(waitMs);
}

void setup() {
  if ((rmt_send = rmtInit(WS2812_PIN, RMT_TX_MODE, RMT_MEM_64)) == NULL) {
    while (true) {
      delay(1000);
    }
  }

  rmtSetTick(rmt_send, 100);
  fillColor(0, 0, 0);
  rmtWrite(rmt_send, led_data, WS2812_BITS);
}

void loop() {
  showColor(255, 0, 0, 500);
  showColor(0, 255, 0, 500);
  showColor(0, 0, 255, 500);
  showColor(0, 0, 0, 500);
}
