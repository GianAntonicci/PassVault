#ifndef INPUT_H
#define INPUT_H

// =====================================================================
//  Abstracao de input
//
//  inputBegin()                 — inicializa o hardware
//  inputRead(x, y, gesture)     — true quando ha um evento novo
//
//  Gestos (mesmos IDs da CST816S):
//    GESTURE_NONE        toque simples em (x, y)
//    GESTURE_SWIPE_DOWN  rolar lista para cima
//    GESTURE_SWIPE_UP    rolar lista para baixo
//    GESTURE_BACK        "voltar" (botao fisico segurado)
//  Evento com x = y = -1 nao acerta nenhum botao: serve so para
//  contar atividade (auto-lock).
//
//  Foco (usado apenas na placa so com botoes; no-op nas demais):
//    focusBegin(tela) / focusAdd(x, y, w, h, flags) / focusEnd()
//  Cada tela registra seus alvos clicaveis na ordem de navegacao.
// =====================================================================

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"

extern TFT_eSPI tft;

#define GESTURE_NONE        0
#define GESTURE_SWIPE_DOWN  1
#define GESTURE_SWIPE_UP    2
#define GESTURE_BACK        100

// Flags de foco: alvo na borda de uma lista rolavel
#define FOCUS_SCROLL_UP     0x01
#define FOCUS_SCROLL_DOWN   0x02

// ---------------------------------------------------------------------
#if defined(BOARD_TDISPLAY_S3_TOUCH)
// ---- CST816S capacitivo, com gestos ----

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

// ---------------------------------------------------------------------
#elif defined(BOARD_CYD)
// ---- XPT2046 em SPI proprio (VSPI): tft.getTouch() nao alcanca ----

#include <SPI.h>
#include <XPT2046_Touchscreen.h>

SPIClass            touchSpi(VSPI);
XPT2046_Touchscreen touch(XPT_CS, XPT_IRQ);

void inputBegin() {
  touchSpi.begin(XPT_CLK, XPT_MISO, XPT_MOSI, XPT_CS);
  touch.begin(touchSpi);
  touch.setRotation(TFT_ROTATION);
}

bool inputRead(int &x, int &y, int &gesture) {
  // Resistivo: so reporta a borda de descida (novo toque), nao o "segurar"
  static bool wasDown = false;
  bool down = touch.touched();
  bool edge = down && !wasDown;
  wasDown = down;
  if (!edge) return false;

  TS_Point p = touch.getPoint();
  x = constrain(map(p.x, XPT_X_MIN, XPT_X_MAX, 0, SCR_W), 0, SCR_W - 1);
  y = constrain(map(p.y, XPT_Y_MIN, XPT_Y_MAX, 0, SCR_H), 0, SCR_H - 1);
  gesture = GESTURE_NONE;
  return true;
}

// ---------------------------------------------------------------------
#elif defined(BOARD_GENERIC_ILI9341)
// ---- XPT2046 no mesmo barramento do display, via TFT_eSPI ----

void inputBegin() {
  uint16_t cal[5] = TOUCH_CAL_DATA;
  tft.setTouch(cal);
}

bool inputRead(int &x, int &y, int &gesture) {
  static bool wasDown = false;
  uint16_t tx, ty;
  bool down = tft.getTouch(&tx, &ty);
  bool edge = down && !wasDown;
  wasDown = down;
  if (!edge) return false;

  x = tx;
  y = ty;
  gesture = GESTURE_NONE;
  return true;
}

// ---------------------------------------------------------------------
#elif defined(BOARD_TDISPLAY_S3)
// ---- 2 botoes fisicos navegando um anel de foco ----
//  BTN_DOWN (GPIO0):  clique = proximo alvo   | segurar = confirma (toque)
//  BTN_UP   (GPIO14): clique = alvo anterior  | segurar = voltar

#define FOCUS_MAX  48   // teclado da busca: 36 teclas + 3 acoes

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

// Tela nova reinicia o foco; redesenho da mesma tela mantem a posicao
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

// Estado de um botao: 0 = nada, 1 = clique curto, 2 = segurou
struct BtnState { uint8_t pin; bool down; bool longFired; unsigned long t0; };
BtnState btnDown = { BTN_DOWN, false, false, 0 };
BtnState btnUp   = { BTN_UP,   false, false, 0 };

int pollButton(BtnState &b) {
  bool pressed = digitalRead(b.pin) == LOW;
  unsigned long now = millis();
  if (pressed && !b.down) {
    if (now - b.t0 < 30) return 0;          // debounce
    b.down = true; b.longFired = false; b.t0 = now;
    return 0;
  }
  if (pressed && b.down && !b.longFired && now - b.t0 >= BTN_LONG_MS) {
    b.longFired = true;                     // dispara ainda segurando
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

  if (d == 2) {                             // confirma: toque virtual no alvo
    if (focusCount == 0) return true;
    const FocusTarget &f = focusList[focusIdx];
    x = f.x + f.w / 2;
    y = f.y + f.h / 2;
    return true;
  }
  if (u == 2) { gesture = GESTURE_BACK; return true; }

  if (d == 1) {
    if (focusCount && (focusList[focusIdx].flags & FOCUS_SCROLL_DOWN))
      gesture = GESTURE_SWIPE_UP;           // tela rola e o foco fica no lugar
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

// Placas com touch nao usam foco
#if !defined(BOARD_TDISPLAY_S3)
inline void focusBegin(int) {}
inline void focusAdd(int, int, int, int, uint8_t = 0) {}
inline void focusEnd() {}
#endif

#endif
