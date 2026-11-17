#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Arduino.h>
#include "config.h"

enum StatusLedColor : uint8_t { LED_OFF, LED_LOCKED, LED_UNLOCKED, LED_WIFI };

#if defined(LED_RGB_PIN) && LED_RGB_PIN >= 0
#define HAS_STATUS_LED 1

class StatusLed {
public:
  void show(StatusLedColor color) {
    if (color == current) return;
    current = color;
    uint8_t r = color == LED_LOCKED ? LED_RGB_LEVEL : 0;
    uint8_t g = color == LED_UNLOCKED ? LED_RGB_LEVEL : 0;
    uint8_t b = color == LED_WIFI ? LED_RGB_LEVEL : 0;
    rgbLedWrite(LED_RGB_PIN, r, g, b);
  }

private:
  int current = -1;
};

#else

class StatusLed {
public:
  void show(StatusLedColor) {}
};

#endif

#endif
