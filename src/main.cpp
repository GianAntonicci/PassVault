/*
 *  PassVault — Password Manager
 *  Multi-placa: ver BOARD_XXX em platformio.ini / config.h
 *  TFT_eSPI + input.h (touch/botoes) + keyboard.h (USB HID/BLE)
 *  PlatformIO build
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"

// ---- Objects ----
TFT_eSPI tft = TFT_eSPI();

#include "input.h"
#include "keyboard.h"
#include "vault_core.h"
#include "vault_web.h"

// ---- State machine ----
enum State { ST_PIN_SETUP, ST_PIN_CONFIRM, ST_PIN_ENTRY, ST_MENU, ST_DETAIL, ST_WEB };
State state = ST_PIN_ENTRY;

char pinBuf[PIN_LEN_MAX + 1]     = "";
char pinConfirm[PIN_LEN_MAX + 1] = "";
int  pinPos   = 0;
int  pinLen   = PIN_LEN;   // vault novo usa PIN_LEN; vault existente, o tamanho salvo
int  selCred  = 0;
int  menuTop  = 0;

unsigned long lockoutEnd = 0;
unsigned long lockoutTick = 0;
unsigned long lastAction = 0;

// ---- Layout: portrait 170x320 vs landscape 320x240 ----
#ifdef USE_LANDSCAPE
  #define PIN_TITLE_Y   6
  #define PIN_MSG_Y     36
  #define PIN_BOX_Y     56
  #define PIN_BOX_H     28
  #define NP_BW         64
  #define NP_BH         32
  #define NP_Y0         92
  #define WEB_TITLE_Y   16
  #define WEB_LINE_Y    60
  #define WEB_BTN_Y     178
#else
  #define PIN_TITLE_Y   20
  #define PIN_MSG_Y     60
  #define PIN_BOX_Y     95
  #define PIN_BOX_H     32
  #define NP_BW         48
  #define NP_BH         32
  #define NP_Y0         140
  #define WEB_TITLE_Y   30
  #define WEB_LINE_Y    80
  #define WEB_BTN_Y     200
#endif
// Aviso ("PIN incorreto!") — no landscape substitui a linha de instrucao
#ifdef USE_LANDSCAPE
  #define PIN_WARN_Y    PIN_MSG_Y
#else
  #define PIN_WARN_Y    75
#endif

// ---- Forward declarations ----
void drawMenu();
void drawDetail();
void drawWebScreen();
void drawPinScreen(const char* title);
void lockDevice();

bool inBox(int tx, int ty, int bx, int by, int bw, int bh) {
  return tx >= bx && tx < bx + bw && ty >= by && ty < by + bh;
}

// Botao clicavel — tambem vira alvo de foco na placa so com botoes
void btn(int x, int y, int w, int h, const char* label, uint16_t bg) {
  tft.fillRoundRect(x, y, w, h, 5, bg);
  tft.setTextColor(C_BG, bg);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(label, x + w / 2, y + h / 2, 2);
  focusAdd(x, y, w, h);
}

// ---- Numpad ----
#define NP_GAP  4
#define NP_COLS  3
#define NP_ROWS  4

const char* npLabels[12] = {"1","2","3","4","5","6","7","8","9","C","0","<"};

int npX0() { return (SCR_W - (NP_COLS * NP_BW + (NP_COLS - 1) * NP_GAP)) / 2; }
int npY0() { return NP_Y0; }

void drawNumpad() {
  int x0 = npX0(), y0 = npY0();
  for (int r = 0; r < NP_ROWS; r++) {
    for (int c = 0; c < NP_COLS; c++) {
      int idx = r * NP_COLS + c;
      int bx = x0 + c * (NP_BW + NP_GAP);
      int by = y0 + r * (NP_BH + NP_GAP);
      uint16_t bg = (idx == 9 || idx == 11) ? C_WARN : C_BTN;
      btn(bx, by, NP_BW, NP_BH, npLabels[idx], bg);
    }
  }
}

int hitNumpad(int tx, int ty) {
  int x0 = npX0(), y0 = npY0();
  for (int r = 0; r < NP_ROWS; r++) {
    for (int c = 0; c < NP_COLS; c++) {
      int bx = x0 + c * (NP_BW + NP_GAP);
      int by = y0 + r * (NP_BH + NP_GAP);
      if (inBox(tx, ty, bx, by, NP_BW, NP_BH))
        return r * NP_COLS + c;
    }
  }
  return -1;
}

// ---- PIN Screen ----
void drawPinBoxes() {
  // Encolhe as caixas quando o PIN e longo (6+ digitos no portrait)
  int gap = pinLen > 4 ? 5 : 8;
  int bw  = min(28, (SCR_W - 16 - (pinLen - 1) * gap) / pinLen);
  int x0 = (SCR_W - (pinLen * bw + (pinLen - 1) * gap)) / 2;
  int y0 = PIN_BOX_Y;
  for (int i = 0; i < pinLen; i++) {
    int bx = x0 + i * (bw + gap);
    tft.drawRoundRect(bx, y0, bw, PIN_BOX_H, 4, C_ACCENT);
    if (i < pinPos) {
      tft.fillCircle(bx + bw / 2, y0 + PIN_BOX_H / 2, 6, C_TEXT);
    } else {
      tft.fillRect(bx + 2, y0 + 2, bw - 4, PIN_BOX_H - 4, C_BG);
    }
  }
}

void drawPinScreen(const char* title) {
  focusBegin(ST_PIN_ENTRY);
  tft.fillScreen(C_BG);
  tft.setTextColor(C_ACCENT);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(title, SCR_W / 2, PIN_TITLE_Y, 4);

  tft.setTextColor(C_DIM);
  tft.setTextDatum(TC_DATUM);
  char msg[24];
  snprintf(msg, sizeof(msg), "Digite %d digitos", pinLen);
  tft.drawString(msg, SCR_W / 2, PIN_MSG_Y, 2);

  drawPinBoxes();
  drawNumpad();
  focusEnd();
}

void drawPinWarn(const char* msg) {
#ifdef USE_LANDSCAPE
  tft.fillRect(0, PIN_WARN_Y, SCR_W, 16, C_BG);
#endif
  tft.setTextColor(C_WARN);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(msg, SCR_W / 2, PIN_WARN_Y, 2);
}

void pinBackspace() {
  if (pinPos > 0) {
    pinPos--;
    pinBuf[pinPos] = 0;
    drawPinBoxes();
  }
}

void clearPinBuffers() {
  mbedtls_platform_zeroize(pinBuf, sizeof(pinBuf));
  mbedtls_platform_zeroize(pinConfirm, sizeof(pinConfirm));
  pinPos = 0;
}

// Espera para um dado numero de erros: LOCKOUT_MS, dobrando a cada erro extra
unsigned long lockoutDuration(int fails) {
  if (fails < LOCKOUT_AFTER) return 0;
  unsigned long ms = LOCKOUT_MS;
  for (int i = LOCKOUT_AFTER; i < fails && ms < LOCKOUT_MAX_MS; i++) ms *= 2;
  return min(ms, (unsigned long)LOCKOUT_MAX_MS);
}

void drawLockout() {
  int fails = storageFails();
  unsigned long left = lockoutEnd > millis() ? (lockoutEnd - millis() + 999) / 1000 : 0;
  char buf[32];

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_WARN, C_BG);
  tft.drawString("BLOQUEADO", SCR_W / 2, SCR_H / 2 - 30, 4);

  tft.fillRect(0, SCR_H / 2 - 4, SCR_W, 18, C_BG);
  snprintf(buf, sizeof(buf), "Aguarde %lus", left);
  tft.drawString(buf, SCR_W / 2, SCR_H / 2 + 5, 2);

  if (WIPE_AFTER > 0) {
    tft.setTextColor(C_DIM, C_BG);
    snprintf(buf, sizeof(buf), "Apaga apos %d erros", WIPE_AFTER - fails);
    tft.drawString(buf, SCR_W / 2, SCR_H / 2 + 30, 2);
  }
}

void startLockout(int fails) {
  lockoutEnd = millis() + lockoutDuration(fails);
  lockoutTick = 0;
  focusBegin(-1);
  tft.fillScreen(C_BG);
  drawLockout();
}

// Limite atingido: apaga o vault e reinicia em "Criar PIN"
void wipeVault() {
  storageWipe();
  clearPinBuffers();
  focusBegin(-1);
  tft.fillScreen(C_BG);
  tft.setTextColor(C_WARN);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("VAULT", SCR_W / 2, SCR_H / 2 - 20, 4);
  tft.drawString("APAGADO", SCR_W / 2, SCR_H / 2 + 10, 4);
  delay(4000);
  ESP.restart();
}

void drawBusy(const char* msg) {
  tft.fillRect(0, PIN_WARN_Y, SCR_W, 16, C_BG);
  tft.setTextColor(C_ACCENT);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(msg, SCR_W / 2, PIN_WARN_Y, 2);
}

void handlePinTouch(int tx, int ty) {
  int key = hitNumpad(tx, ty);
  if (key < 0) return;

  if (key == 9) { // C = clear
    pinPos = 0;
    pinBuf[0] = 0;
    drawPinBoxes();
    return;
  }
  if (key == 11) { // < = backspace
    pinBackspace();
    return;
  }

  char digit = npLabels[key][0];
  if (pinPos < pinLen) {
    pinBuf[pinPos++] = digit;
    pinBuf[pinPos] = 0;
    drawPinBoxes();
  }

  if (pinPos == pinLen) {
    delay(200);

    if (state == ST_PIN_SETUP) {
      strncpy(pinConfirm, pinBuf, sizeof(pinConfirm));
      mbedtls_platform_zeroize(pinBuf, sizeof(pinBuf));
      pinPos = 0;
      state = ST_PIN_CONFIRM;
      drawPinScreen("Confirme PIN");

    } else if (state == ST_PIN_CONFIRM) {
      if (strcmp(pinBuf, pinConfirm) == 0) {
        drawBusy("Gerando chave...");
        setupPin(pinBuf);
        storageResetFails();
        loadCredentials();
        clearPinBuffers();
        state = ST_MENU;
        drawMenu();
      } else {
        clearPinBuffers();
        state = ST_PIN_SETUP;
        drawPinScreen("Criar PIN");
        drawPinWarn("PINs diferentes!");
      }

    } else if (state == ST_PIN_ENTRY) {
      // Conta o erro ANTES de verificar: desligar no meio nao zera o contador
      int fails = storageAddFail();
      drawBusy("Verificando...");
      bool ok = verifyPin(pinBuf);
      clearPinBuffers();

      if (ok) {
        storageResetFails();
        loadCredentials();
        state = ST_MENU;
        drawMenu();
      } else if (WIPE_AFTER > 0 && fails >= WIPE_AFTER) {
        wipeVault();
      } else if (fails >= LOCKOUT_AFTER) {
        startLockout(fails);
      } else {
        drawPinScreen("Digite PIN");
        char msg[24];
        snprintf(msg, sizeof(msg), "PIN errado (%d/%d)", fails, LOCKOUT_AFTER);
        drawPinWarn(msg);
      }
    }
  }
}

// ---- Menu ----
#define MENU_ITEM_H  30
#define MENU_VISIBLE ((SCR_H - 110) / MENU_ITEM_H)
#define MENU_BTN_W   75
#define MENU_BTN_H   38

// Placas sem gesto de swipe ganham uma coluna de botoes cima/baixo ao lado da lista
#ifdef MENU_SCROLL_BTNS
  #define MENU_SCR_W   44
  #define MENU_ITEM_W  (SCR_W - 8 - MENU_SCR_W - 4)
  #define MENU_SCR_X   (SCR_W - 4 - MENU_SCR_W)
  #define MENU_SCR_H   ((MENU_VISIBLE * MENU_ITEM_H - 2 - 4) / 2)
  #define MENU_UP_Y    45
  #define MENU_DN_Y    (45 + MENU_SCR_H + 4)

void drawScrollBtn(int y, bool up, bool enabled) {
  uint16_t bg = enabled ? C_BTN : C_CARD;
  tft.fillRoundRect(MENU_SCR_X, y, MENU_SCR_W, MENU_SCR_H, 5, bg);
  int cx = MENU_SCR_X + MENU_SCR_W / 2, cy = y + MENU_SCR_H / 2;
  if (up) tft.fillTriangle(cx - 9, cy + 5, cx + 9, cy + 5, cx, cy - 6, C_BG);
  else    tft.fillTriangle(cx - 9, cy - 5, cx + 9, cy - 5, cx, cy + 6, C_BG);
}
#else
  #define MENU_ITEM_W  (SCR_W - 8)
#endif

// Indicador do teclado (BLE muda em tempo de execucao)
bool kbdShownConnected = false;

void drawKbdStatus() {
  kbdShownConnected = kbdConnected();
  tft.fillRect(SCR_W - 50, 8, 44, 22, C_BG);
  tft.setTextColor(kbdShownConnected ? C_OK : C_DIM);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(kbdLabel(), SCR_W - 10, 12, 2);
}

void drawMenu() {
  focusBegin(ST_MENU);
  tft.fillScreen(C_BG);
  tft.setTextColor(C_ACCENT);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("PassVault", 10, 8, 4);

  drawKbdStatus();

  int y = 45;
  int visible = MENU_VISIBLE;
  if (credCount == 0) {
    tft.setTextColor(C_DIM);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Nenhuma credencial", SCR_W / 2, y + 20, 2);
  } else {
    for (int i = menuTop; i < credCount && i < menuTop + visible; i++) {
      tft.fillRoundRect(4, y, MENU_ITEM_W, MENU_ITEM_H - 2, 4, C_CARD);
      tft.setTextColor(C_TEXT);
      tft.setTextDatum(TL_DATUM);
      tft.drawString(creds[i].name, 12, y + 7, 2);
      // Bordas da lista rolam ao inves de mover o foco
      uint8_t flags = 0;
      if (i == menuTop && menuTop > 0) flags |= FOCUS_SCROLL_UP;
      if (i == menuTop + visible - 1 && i + 1 < credCount) flags |= FOCUS_SCROLL_DOWN;
      focusAdd(4, y, MENU_ITEM_W, MENU_ITEM_H - 2, flags);
      y += MENU_ITEM_H;
    }
  }

#ifdef MENU_SCROLL_BTNS
  if (credCount > MENU_VISIBLE) {
    drawScrollBtn(MENU_UP_Y, true,  menuTop > 0);
    drawScrollBtn(MENU_DN_Y, false, menuTop + MENU_VISIBLE < credCount);
  }
#endif

  int btnY = SCR_H - MENU_BTN_H - 6;
  btn(6, btnY, MENU_BTN_W, MENU_BTN_H, "WiFi", C_ACCENT);
  btn(SCR_W - MENU_BTN_W - 6, btnY, MENU_BTN_W, MENU_BTN_H, "Bloquear", C_WARN);
  focusEnd();
}

void handleMenuTouch(int tx, int ty, int gesture) {
  int btnY = SCR_H - MENU_BTN_H - 6;

#ifdef MENU_SCROLL_BTNS
  // Botoes de rolagem equivalem aos gestos de swipe
  if (credCount > MENU_VISIBLE) {
    if (inBox(tx, ty, MENU_SCR_X, MENU_UP_Y, MENU_SCR_W, MENU_SCR_H)) gesture = GESTURE_SWIPE_DOWN;
    if (inBox(tx, ty, MENU_SCR_X, MENU_DN_Y, MENU_SCR_W, MENU_SCR_H)) gesture = GESTURE_SWIPE_UP;
  }
#endif

  if (gesture == GESTURE_SWIPE_DOWN && menuTop > 0) {
    menuTop--;
    drawMenu();
    return;
  }
  if (gesture == GESTURE_SWIPE_UP && menuTop + MENU_VISIBLE < credCount) {
    menuTop++;
    drawMenu();
    return;
  }

  if (inBox(tx, ty, 6, btnY, MENU_BTN_W, MENU_BTN_H)) {
    startWeb();
    state = ST_WEB;
    drawWebScreen();
    return;
  }

  if (inBox(tx, ty, SCR_W - MENU_BTN_W - 6, btnY, MENU_BTN_W, MENU_BTN_H)) {
    lockDevice();
    return;
  }

  int y = 45;
  for (int i = menuTop; i < credCount && i < menuTop + MENU_VISIBLE; i++) {
    if (inBox(tx, ty, 4, y, MENU_ITEM_W, MENU_ITEM_H - 2)) {
      selCred = i;
      state = ST_DETAIL;
      drawDetail();
      return;
    }
    y += MENU_ITEM_H;
  }
}

// ---- Detail ----
void typeCredential(int mode) {
  if (mode == 0 || mode == 1) {
    kbdPrint(creds[selCred].user);
    if (mode == 0) {
      kbdWrite(KEY_TAB);
    }
  }
  if (mode == 0 || mode == 2) {
    kbdPrint(creds[selCred].pass);
  }
}

// Botoes do detalhe: 0=Tudo 1=Login 2=Senha 3=Voltar
// Portrait: grade 2x2 | Landscape: uma linha no rodape
#define DET_BTN_H  38
#define DET_GAP    8
#ifdef USE_LANDSCAPE
  #define DET_BTN_W  ((SCR_W - 5 * DET_GAP) / 4)
#else
  #define DET_BTN_W  75
#endif

void detailBtnPos(int i, int &x, int &y) {
#ifdef USE_LANDSCAPE
  x = DET_GAP + i * (DET_BTN_W + DET_GAP);
  y = SCR_H - DET_BTN_H - 6;
#else
  int x1 = (SCR_W - 2 * DET_BTN_W - DET_GAP) / 2;
  x = x1 + (i % 2) * (DET_BTN_W + DET_GAP);
  y = 170 + (i / 2) * (DET_BTN_H + DET_GAP);
#endif
}

const char* detLabels[4] = {"Tudo", "Login", "Senha", "Voltar"};
const uint16_t detColors[4] = {C_OK, C_ACCENT, C_ACCENT, C_DIM};

void drawDetail() {
  focusBegin(ST_DETAIL);
  tft.fillScreen(C_BG);
  tft.setTextColor(C_ACCENT);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(creds[selCred].name, SCR_W / 2, 10, 4);

  tft.setTextColor(C_DIM);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Usuario:", 10, 50, 2);
  tft.setTextColor(C_TEXT);
  tft.drawString(creds[selCred].user, 10, 70, 2);

  tft.setTextColor(C_DIM);
  tft.drawString("Senha:", 10, 100, 2);
  tft.setTextColor(C_TEXT);
  String masked = "";
  for (int i = 0; i < (int)strlen(creds[selCred].pass); i++) masked += "*";
  tft.drawString(masked, 10, 120, 2);

  for (int i = 0; i < 4; i++) {
    int x, y;
    detailBtnPos(i, x, y);
    btn(x, y, DET_BTN_W, DET_BTN_H, detLabels[i], detColors[i]);
  }
  focusEnd();
}

void handleDetailTouch(int tx, int ty) {
  for (int i = 0; i < 4; i++) {
    int x, y;
    detailBtnPos(i, x, y);
    if (!inBox(tx, ty, x, y, DET_BTN_W, DET_BTN_H)) continue;
    if (i < 3) {
      typeCredential(i);
    } else {
      state = ST_MENU;
      drawMenu();
    }
    return;
  }
}

// ---- Web Screen ----
#define WEB_BTN_W  140
#define WEB_BTN_H  42

void drawWebScreen() {
  focusBegin(ST_WEB);
  tft.fillScreen(C_BG);
  tft.setTextColor(C_ACCENT);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("WiFi Ativo", SCR_W / 2, WEB_TITLE_Y, 4);

  tft.setTextColor(C_TEXT);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("SSID: " AP_SSID, SCR_W / 2, WEB_LINE_Y, 2);
  tft.drawString(String("Senha: ") + webPass, SCR_W / 2, WEB_LINE_Y + 25, 2);

  IPAddress ip = WiFi.softAPIP();
  tft.drawString("http://" + ip.toString(), SCR_W / 2, WEB_LINE_Y + 60, 2);

  btn((SCR_W - WEB_BTN_W) / 2, WEB_BTN_Y, WEB_BTN_W, WEB_BTN_H, "Desligar WiFi", C_WARN);
  focusEnd();
}

void closeWeb() {
  stopWeb();
  state = ST_MENU;
  drawMenu();
}

void handleWebTouch(int tx, int ty) {
  if (inBox(tx, ty, (SCR_W - WEB_BTN_W) / 2, WEB_BTN_Y, WEB_BTN_W, WEB_BTN_H)) {
    closeWeb();
  }
}

// ---- Gesto "voltar" (botao fisico segurado) ----
void handleBack() {
  switch (state) {
    case ST_PIN_SETUP:
    case ST_PIN_CONFIRM:
    case ST_PIN_ENTRY:
      pinBackspace();
      break;
    case ST_MENU:
      lockDevice();
      break;
    case ST_DETAIL:
      state = ST_MENU;
      drawMenu();
      break;
    case ST_WEB:
      closeWeb();
      break;
  }
}

// ---- Lock ----
void lockDevice() {
  clearMemory();
  clearPinBuffers();
  menuTop = 0;
  state = ST_PIN_ENTRY;
  drawPinScreen("Digite PIN");
}

// ===================== SETUP =====================
void setup() {
#if PIN_POWER_ON >= 0
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
#endif

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

  tft.init();
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(TFT_BLACK);

  inputBegin();
  kbdBegin();

  storageBegin();

  if (storageHasPin()) {
    pinLen = storagePinLen();
    state = ST_PIN_ENTRY;
    // Reiniciar nao pula o bloqueio: a espera recomeca do zero
    if (storageFails() >= LOCKOUT_AFTER) startLockout(storageFails());
    else drawPinScreen("Digite PIN");
  } else {
    pinLen = PIN_LEN;
    state = ST_PIN_SETUP;
    drawPinScreen("Criar PIN");
  }

  lastAction = millis();
}

// ===================== LOOP =====================
void loop() {
  if (lockoutEnd > 0 && millis() < lockoutEnd) {
    // Contagem regressiva, 1x por segundo
    if (millis() - lockoutTick >= 1000) {
      lockoutTick = millis();
      drawLockout();
    }
    return;
  }
  if (lockoutEnd > 0) {
    lockoutEnd = 0;
    drawPinScreen("Digite PIN");
  }

  webTick();

  // WiFi ligado sem uso (tela e navegador): desliga o AP e bloqueia
  if (state == ST_WEB && millis() - max(lastAction, webLastRequest) > WEB_IDLE_MS) {
    stopWeb();
    lockDevice();
    return;
  }

  if ((state == ST_MENU || state == ST_DETAIL) && millis() - lastAction > AUTOLOCK_MS) {
    lockDevice();
    return;
  }

  // Atualiza o indicador quando o host BLE conecta/desconecta
  if (state == ST_MENU && kbdConnected() != kbdShownConnected) drawKbdStatus();

  int tx, ty, gesture;
  if (!inputRead(tx, ty, gesture)) return;

  static unsigned long lastTouch = 0;
  if (millis() - lastTouch < TOUCH_DEBOUNCE) return;
  lastTouch = millis();
  lastAction = millis();

  if (gesture == GESTURE_BACK) {
    handleBack();
    return;
  }

  switch (state) {
    case ST_PIN_SETUP:
    case ST_PIN_CONFIRM:
    case ST_PIN_ENTRY:
      handlePinTouch(tx, ty);
      break;
    case ST_MENU:
      handleMenuTouch(tx, ty, gesture);
      break;
    case ST_DETAIL:
      handleDetailTouch(tx, ty);
      break;
    case ST_WEB:
      handleWebTouch(tx, ty);
      break;
  }
}
