#ifndef INPUT_H
#define INPUT_H

#include <Arduino.h>
#include "display.h"
#include "config.h"

extern VaultDisplay tft;

#define GESTURE_NONE        0
#define GESTURE_SWIPE_DOWN  1
#define GESTURE_SWIPE_UP    2
#define GESTURE_BACK        100

#define FOCUS_SCROLL_UP     0x01
#define FOCUS_SCROLL_DOWN   0x02

#if defined(BOARD_TDISPLAY_S3_TOUCH)

#include <CST816S.h>

CST816S touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);

void inputBegin() { touch.begin(); }

bool inputRead(int &x, int &y, int &gesture) {
  if (!touch.available()) return false;
  x = touch.data.x;
  y = touch.data.y;
  gesture = touch.data.gestureID;
  return true;
}

#elif defined(INPUT_CST816_POLL) || defined(INPUT_FT3168_POLL) || defined(INPUT_AXS5106_POLL)
#include <Wire.h>
#include "touch_gesture.h"

TouchGesture touchGesture;

void inputBegin() {
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setTimeOut(20);
#if TOUCH_RST >= 0
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, HIGH);
  delay(50);
  digitalWrite(TOUCH_RST, LOW);
#ifdef INPUT_AXS5106_POLL
  delay(200);
#else
  delay(10);
#endif
  digitalWrite(TOUCH_RST, HIGH);
#ifdef INPUT_AXS5106_POLL
  delay(300);
#else
  delay(200);
#endif
#endif
#ifdef INPUT_CST816_POLL
  Wire.beginTransmission(0x15);
  Wire.write(0xFE);
  Wire.write(0x01);
  Wire.endTransmission();
#elif defined(INPUT_FT3168_POLL)
  Wire.beginTransmission(0x38);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();
#endif
}

bool inputRead(int &x, int &y, int &gesture) {
  static unsigned long lastPoll = 0;
  if (millis() - lastPoll < 10) return false;
  lastPoll = millis();
#if defined(INPUT_AXS5106_POLL)
  constexpr uint8_t address = 0x63, reg = 0x01, count = 6;
#elif defined(INPUT_FT3168_POLL)
  constexpr uint8_t address = 0x38, reg = 0x02, count = 5;
#else
  constexpr uint8_t address = 0x15, reg = 0x02, count = 5;
#endif
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(address, count) != count) return false;
#ifdef INPUT_AXS5106_POLL
  Wire.read();
#endif
  uint8_t points = Wire.read() & 0x0F;
  int px = (Wire.read() & 0x0F) << 8;
  px |= Wire.read();
  int py = (Wire.read() & 0x0F) << 8;
  py |= Wire.read();
#ifdef BOARD_TDISPLAY_S3_AMOLED_TOUCH
  int rawX = px;
  px = SCR_W - 1 - py;
  py = rawX;
#endif
#ifdef INPUT_AXS5106_POLL
  px = SCR_W - 1 - px;
#endif
  if (points && (px < 0 || py < 0 || px >= SCR_W || py >= SCR_H)) return false;
  return touchGesture.sample(points != 0, px, py, x, y, gesture);
}

#elif defined(BOARD_TDISPLAY_S3)

#define FOCUS_MAX  48

struct FocusTarget { int16_t x, y, w, h; uint8_t flags; };

FocusTarget focusList[FOCUS_MAX];
int  focusCount  = 0;
int  focusIdx    = 0;
int  focusScreen = -1;
bool focusDrawn  = false;

void focusOutline(int i, uint16_t color) {
  const FocusTarget &f = focusList[i];
  tft.drawRect(f.x - 2, f.y - 2, f.w + 4, f.h + 4, color);
  tft.drawRect(f.x - 1, f.y - 1, f.w + 2, f.h + 2, color);
}

void focusBegin(int screen) {
  if (screen != focusScreen) focusIdx = 0;
  focusScreen = screen;
  focusCount  = 0;
  focusDrawn  = false;
}

void focusAdd(int x, int y, int w, int h, uint8_t flags = 0) {
  if (focusCount >= FOCUS_MAX) return;
  focusList[focusCount++] = { (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, flags };
}

void focusEnd() {
  if (focusCount == 0) return;
  if (focusIdx >= focusCount) focusIdx = focusCount - 1;
  focusOutline(focusIdx, C_ACCENT);
  focusDrawn = true;
}

void focusMove(int dir) {
  if (focusCount == 0) return;
  if (focusDrawn) focusOutline(focusIdx, C_BG);
  focusIdx = (focusIdx + dir + focusCount) % focusCount;
  focusOutline(focusIdx, C_ACCENT);
  focusDrawn = true;
}

void inputBegin() {
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
}

struct BtnState { uint8_t pin; bool down; bool longFired; unsigned long t0; };
BtnState btnDown = { BTN_DOWN, false, false, 0 };
BtnState btnUp   = { BTN_UP,   false, false, 0 };

int pollButton(BtnState &b) {
  bool pressed = digitalRead(b.pin) == LOW;
  unsigned long now = millis();
  if (pressed && !b.down) {
    if (now - b.t0 < 30) return 0;
    b.down = true; b.longFired = false; b.t0 = now;
    return 0;
  }
  if (pressed && b.down && !b.longFired && now - b.t0 >= BTN_LONG_MS) {
    b.longFired = true;
    return 2;
  }
  if (!pressed && b.down) {
    b.down = false;
    bool wasShort = !b.longFired && now - b.t0 >= 30;
    b.t0 = now;
    return wasShort ? 1 : 0;
  }
  return 0;
}

bool inputRead(int &x, int &y, int &gesture) {
  x = -1; y = -1; gesture = GESTURE_NONE;

  int d = pollButton(btnDown);
  int u = pollButton(btnUp);

  if (d == 2) {
    if (focusCount == 0) return true;
    const FocusTarget &f = focusList[focusIdx];
    x = f.x + f.w / 2;
    y = f.y + f.h / 2;
    return true;
  }
  if (u == 2) { gesture = GESTURE_BACK; return true; }

  if (d == 1) {
    if (focusCount && (focusList[focusIdx].flags & FOCUS_SCROLL_DOWN))
      gesture = GESTURE_SWIPE_UP;
    else
      focusMove(+1);
    return true;
  }
  if (u == 1) {
    if (focusCount && (focusList[focusIdx].flags & FOCUS_SCROLL_UP))
      gesture = GESTURE_SWIPE_DOWN;
    else
      focusMove(-1);
    return true;
  }
  return false;
}

#endif

#if !defined(BOARD_TDISPLAY_S3)
inline void focusBegin(int) {}
inline void focusAdd(int, int, int, int, uint8_t = 0) {}
inline void focusEnd() {}
#endif

#endif
