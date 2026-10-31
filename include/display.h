#pragma once

#include <TFT_eSPI.h>
#include "config.h"

#ifdef DISPLAY_GFX
#include <Arduino_GFX_Library.h>
#ifdef BOARD_WAVESHARE_S3_TOUCH_147
#include "jd9853_init.h"
#endif

// Preserve the existing TFT_eSPI fonts and RGB565 drawing API. Only the
// transport changes: render into PSRAM, then send one completed frame.
class VaultDisplay {
  TFT_eSPI fontHost;
  TFT_eSprite canvas{&fontHost};
#if defined(BOARD_WAVESHARE_S3_TOUCH_147)
  Arduino_ESP32SPI bus{45, 21, 38, 39, -1};
  Arduino_ST7789 panel{&bus, 40, 0, false, 172, 320, 34, 0, 34, 0};
#elif defined(BOARD_WAVESHARE_S3_AMOLED_164)
  Arduino_ESP32QSPI bus{46, 10, 11, 12, 13, 14};
  Arduino_CO5300 panel{&bus, 21, 0, 280, 456, 20, 0, 20, 0};
#elif defined(BOARD_TDISPLAY_S3_AMOLED_TOUCH)
  Arduino_ESP32QSPI bus{6, 47, 18, 7, 48, 5};
  Arduino_RM67162 panel{&bus, 17, 0};
#endif
  bool dirty = false;
  bool ready = false;
public:
  bool init() {
    if (!psramFound()) return false;
    canvas.setColorDepth(16);
    if (!canvas.createSprite(SCR_W, SCR_H)) return false;
    if (!panel.begin(40000000)) return false;
#ifdef BOARD_WAVESHARE_S3_TOUCH_147
    initJD9853(&bus);
#endif
    ready = true;
    return true;
  }
  void setRotation(uint8_t) {} // All profiles use native portrait coordinates.
  void present() {
    if (!ready || !dirty) return;
    // TFT_eSprite stores 16-bit pixels in big-endian wire order.
    panel.draw16bitBeRGBBitmap(0, 0, static_cast<uint16_t*>(canvas.getPointer()), SCR_W, SCR_H);
    dirty = false;
  }
  void brightness(uint8_t value) {
#ifdef DISPLAY_AMOLED
    if (ready) panel.setBrightness(value);
#else
    analogWrite(TFT_BL, TFT_BACKLIGHT_ON == HIGH ? value : 255 - value);
#endif
  }
  void fillScreen(uint32_t color) { dirty = true; canvas.fillSprite(color); }
  template <typename... Args> void setTextColor(Args... args) { canvas.setTextColor(args...); }
  void setTextDatum(uint8_t datum) { canvas.setTextDatum(datum); }
#define VAULT_DRAW_METHOD(name) \
  template <typename... Args> auto name(Args... args) { \
    dirty = true; return canvas.name(args...); \
  }
  VAULT_DRAW_METHOD(fillRoundRect)
  VAULT_DRAW_METHOD(drawRoundRect)
  VAULT_DRAW_METHOD(drawString)
  VAULT_DRAW_METHOD(fillCircle)
  VAULT_DRAW_METHOD(drawRect)
  VAULT_DRAW_METHOD(fillRect)
  VAULT_DRAW_METHOD(drawChar)
  VAULT_DRAW_METHOD(fillTriangle)
#undef VAULT_DRAW_METHOD
};
#else
class VaultDisplay : public TFT_eSPI {
public:
  bool init() { TFT_eSPI::init(); return true; }
  void present() {}
  void brightness(uint8_t value) {
    analogWrite(TFT_BL, TFT_BACKLIGHT_ON == HIGH ? value : 255 - value);
  }
};
#endif

extern VaultDisplay tft;
// Ensure every return path in setup/loop publishes a completed frame.
struct DisplayFrame { ~DisplayFrame() { tft.present(); } };
