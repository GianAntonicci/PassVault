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
#include "screen_fade.h"

// ---- State machine ----
enum State { ST_PIN_SETUP, ST_PIN_CONFIRM, ST_PIN_ENTRY, ST_MENU, ST_DETAIL, ST_WEB, ST_SEARCH, ST_SETTINGS, ST_THEMES, ST_BRIGHTNESS, ST_SORT,
             ST_PIN_CURRENT, ST_PIN_NEW, ST_PIN_NEW_CONFIRM };
State state = ST_PIN_ENTRY;
ScreenFade screenFade;

// O fade multiplica o brilho escolhido sem alterar a preferencia na NVS.
void applyBrightness() {
  int duty = (displayBrightness * 255 + 50) / 100;
  duty = (duty * screenFade.level + 127) / 255;
  analogWrite(TFT_BL, TFT_BACKLIGHT_ON == HIGH ? duty : 255 - duty);
}

void beginScreen() {
  // Apaga o backlight antes de substituir os pixels da tela anterior.
  if (screenFade.begin(state, millis())) applyBrightness();
}

void cancelScreenFade() {
  screenFade.cancel();
  applyBrightness();
}

char pinBuf[PIN_LEN_MAX + 1]     = "";
char pinConfirm[PIN_LEN_MAX + 1] = "";
int  pinPos   = 0;
int  pinLen   = PIN_LEN;   // vault novo usa PIN_LEN; vault existente, o tamanho salvo
int  selCred  = 0;
int  menuTop  = 0;
bool detailPasswordVisible = false;
unsigned long detailPasswordShownAt = 0;
const unsigned long DETAIL_REVEAL_MS = 3000;
bool typingFeedbackVisible = false;
unsigned long typingFeedbackShownAt = 0;
const unsigned long TYPING_FEEDBACK_MS = 900;

// Busca por nome: filtro aplicado a lista do menu
char searchBuf[33] = "";
int  filt[MAX_CREDS];
int  filtCount = 0;

unsigned long lockoutEnd = 0;
unsigned long lockoutTick = 0;
unsigned long lastAction = 0;

// ---- Layout: portrait 170x320 / 240x320 vs landscape 320x240 ----
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
void drawSearch();
void drawSettings();
void drawPinScreen(const char* title);
void lockDevice();
void clearPinBuffers();

bool changingPin() {
  return state == ST_PIN_CURRENT || state == ST_PIN_NEW || state == ST_PIN_NEW_CONFIRM;
}

void cancelPinChange() {
  clearPinBuffers();
  pinLen = storagePinLen();
  state = ST_SETTINGS;
  drawSettings();
}

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
  beginScreen();
  focusBegin(changingPin() ? state : ST_PIN_ENTRY);
  tft.fillScreen(C_BG);
  tft.setTextColor(C_ACCENT);
  tft.setTextDatum(TC_DATUM);
#ifdef USE_LANDSCAPE
  if (changingPin()) {
    tft.setTextDatum(TL_DATUM);
    tft.drawString(title, 10, PIN_TITLE_Y, 2);
  } else
#endif
    tft.drawString(title, SCR_W / 2, PIN_TITLE_Y, 4);

  tft.setTextColor(C_DIM);
  tft.setTextDatum(TC_DATUM);
  char msg[24];
  snprintf(msg, sizeof(msg), "Digite %d digitos", pinLen);
  tft.drawString(msg, SCR_W / 2, PIN_MSG_Y, 2);

  drawPinBoxes();
  drawNumpad();
  if (changingPin()) {
#ifdef USE_LANDSCAPE
    btn(SCR_W - 82, 4, 74, 26, "Cancelar", C_BTN);
#else
    btn(10, 286, SCR_W - 20, 28, "Cancelar", C_BTN);
#endif
  }
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
  cancelScreenFade();
  lockoutEnd = millis() + lockoutDuration(fails);
  lockoutTick = 0;
  focusBegin(-1);
  tft.fillScreen(C_BG);
  drawLockout();
}

// Limite atingido: apaga o vault e reinicia em "Criar PIN"
void wipeVault() {
  cancelScreenFade();
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
  if (changingPin()) {
#ifdef USE_LANDSCAPE
    bool cancel = inBox(tx, ty, SCR_W - 82, 4, 74, 26);
#else
    bool cancel = inBox(tx, ty, 10, 286, SCR_W - 20, 28);
#endif
    if (cancel) { cancelPinChange(); return; }
  }
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

    } else if (state == ST_PIN_NEW) {
      memcpy(pinConfirm, pinBuf, sizeof(pinConfirm));
      mbedtls_platform_zeroize(pinBuf, sizeof(pinBuf));
      pinPos = 0;
      state = ST_PIN_NEW_CONFIRM;
      drawPinScreen("Confirme PIN");
    } else if (state == ST_PIN_NEW_CONFIRM) {
      if (strcmp(pinBuf, pinConfirm) != 0) {
        clearPinBuffers();
        state = ST_PIN_NEW;
        drawPinScreen("Novo PIN");
        drawPinWarn("PINs diferentes!");
        return;
      }
      drawBusy("Salvando PIN...");
      PinChangeResult result = changePin(pinBuf);
      clearPinBuffers();
      if (result == PIN_CHANGE_PENDING) {
        clearMemory();
        ESP.restart();  // replay da transacao antes de permitir novo desbloqueio
        return;
      }
      lockDevice();
      if (result == PIN_CHANGE_OK) drawBusy("PIN alterado!");
      else drawPinWarn("Falha ao salvar");
    } else if (state == ST_PIN_ENTRY || state == ST_PIN_CURRENT) {
      bool change = state == ST_PIN_CURRENT;
      // Conta o erro ANTES de verificar: desligar no meio nao zera o contador
      int fails = storageAddFail();
      drawBusy("Verificando...");
      bool ok = verifyPin(pinBuf);
      clearPinBuffers();

      if (ok) {
        storageResetFails();
        if (change) {
          pinLen = PIN_LEN;
          state = ST_PIN_NEW;
          drawPinScreen("Novo PIN");
        } else {
          loadCredentials();
          state = ST_MENU;
          drawMenu();
        }
      } else {
        // A verificacao invalida a chave em RAM: termina a sessao desbloqueada.
        if (change) lockDevice();
        if (WIPE_AFTER > 0 && fails >= WIPE_AFTER) { wipeVault(); return; }
        if (fails >= LOCKOUT_AFTER) { startLockout(fails); return; }
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
#define MENU_VISIBLE ((SCR_H - 65 - MENU_LIST_Y) / MENU_ITEM_H)
#define MENU_BTN_W   75
#define MENU_BTN_H   38

// Campo de busca: landscape fica no cabecalho, portrait ganha uma linha propria
#ifdef USE_LANDSCAPE
  #define SRCH_X       132
  #define SRCH_Y       6
  #define SRCH_W       (SCR_W - 50 - 6 - SRCH_X)
  #define MENU_LIST_Y  45
#else
  #define SRCH_X       4
  #define SRCH_Y       40
  #define SRCH_W       (SCR_W - 8)
  #define MENU_LIST_Y  72
#endif
#define SRCH_H       26

// Placas sem gesto de swipe ganham uma coluna de botoes cima/baixo ao lado da lista
#ifdef MENU_SCROLL_BTNS
  #define MENU_SCR_W   44
  // Cards so cedem espaco aos botoes quando ha rolagem
  #define MENU_ITEM_W  (filtCount > MENU_VISIBLE ? SCR_W - 8 - MENU_SCR_W - 4 : SCR_W - 8)
  #define MENU_SCR_X   (SCR_W - 4 - MENU_SCR_W)
  #define MENU_SCR_H   ((MENU_VISIBLE * MENU_ITEM_H - 2 - 4) / 2)
  #define MENU_UP_Y    MENU_LIST_Y
  #define MENU_DN_Y    (MENU_LIST_Y + MENU_SCR_H + 4)

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

// Substring sem diferenciar maiusculas
bool nameMatches(const char* name, const char* q) {
  int nl = strlen(name), ql = strlen(q);
  for (int i = 0; i + ql <= nl; i++) {
    int j = 0;
    while (j < ql && tolower(name[i + j]) == tolower(q[j])) j++;
    if (j == ql) return true;
  }
  return false;
}

// Refeito a cada desenho: credenciais podem mudar pela web
void applyFilter() {
  filtCount = 0;
  for (int position = 0; position < credCount; position++) {
    int i = credentialDisplayIndex(position);
    if (nameMatches(creds[i].name, searchBuf)) filt[filtCount++] = i;
  }
  if (menuTop > max(0, filtCount - MENU_VISIBLE)) menuTop = max(0, filtCount - MENU_VISIBLE);
}

void clearSearch() {
  mbedtls_platform_zeroize(searchBuf, sizeof(searchBuf));
  menuTop = 0;
}

void drawMenu() {
  beginScreen();
  detailPasswordVisible = false;
  focusBegin(ST_MENU);
  applyFilter();
  tft.fillScreen(C_BG);
  tft.setTextColor(C_ACCENT);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("PassVault", 10, 8, 4);

  drawKbdStatus();

  tft.fillRoundRect(SRCH_X, SRCH_Y, SRCH_W, SRCH_H, 4, C_CARD);
  tft.drawRoundRect(SRCH_X, SRCH_Y, SRCH_W, SRCH_H, 4, searchBuf[0] ? C_ACCENT : C_DIM);
  tft.setTextColor(searchBuf[0] ? C_TEXT : C_DIM);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(searchBuf[0] ? searchBuf : "Buscar...", SRCH_X + 8, SRCH_Y + SRCH_H / 2, 2);
  focusAdd(SRCH_X, SRCH_Y, SRCH_W, SRCH_H);

  int y = MENU_LIST_Y;
  int visible = MENU_VISIBLE;
  if (filtCount == 0) {
    tft.setTextColor(C_DIM);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(credCount ? "Nada encontrado" : "Nenhuma credencial", SCR_W / 2, y + 20, 2);
  } else {
    for (int i = menuTop; i < filtCount && i < menuTop + visible; i++) {
      tft.fillRoundRect(4, y, MENU_ITEM_W, MENU_ITEM_H - 2, 4, C_CARD);
      tft.setTextColor(C_TEXT);
      tft.setTextDatum(TL_DATUM);
      tft.drawString(String(creds[filt[i]].favorite ? "* " : "") + creds[filt[i]].name, 12, y + 7, 2);
      // Bordas da lista rolam ao inves de mover o foco
      uint8_t flags = 0;
      if (i == menuTop && menuTop > 0) flags |= FOCUS_SCROLL_UP;
      if (i == menuTop + visible - 1 && i + 1 < filtCount) flags |= FOCUS_SCROLL_DOWN;
      focusAdd(4, y, MENU_ITEM_W, MENU_ITEM_H - 2, flags);
      y += MENU_ITEM_H;
    }
  }

#ifdef MENU_SCROLL_BTNS
  if (filtCount > MENU_VISIBLE) {
    drawScrollBtn(MENU_UP_Y, true,  menuTop > 0);
    drawScrollBtn(MENU_DN_Y, false, menuTop + MENU_VISIBLE < filtCount);
  }
#endif

  int btnY = SCR_H - MENU_BTN_H - 6;
  btn(6, btnY, MENU_BTN_W, MENU_BTN_H, "Config.", C_ACCENT);
  btn(SCR_W - MENU_BTN_W - 6, btnY, MENU_BTN_W, MENU_BTN_H, "Bloquear", C_WARN);
  focusEnd();
}

void handleMenuTouch(int tx, int ty, int gesture) {
  int btnY = SCR_H - MENU_BTN_H - 6;

#ifdef MENU_SCROLL_BTNS
  // Botoes de rolagem equivalem aos gestos de swipe
  if (filtCount > MENU_VISIBLE) {
    if (inBox(tx, ty, MENU_SCR_X, MENU_UP_Y, MENU_SCR_W, MENU_SCR_H)) gesture = GESTURE_SWIPE_DOWN;
    if (inBox(tx, ty, MENU_SCR_X, MENU_DN_Y, MENU_SCR_W, MENU_SCR_H)) gesture = GESTURE_SWIPE_UP;
  }
#endif

  if (gesture == GESTURE_SWIPE_DOWN && menuTop > 0) {
    menuTop--;
    drawMenu();
    return;
  }
  if (gesture == GESTURE_SWIPE_UP && menuTop + MENU_VISIBLE < filtCount) {
    menuTop++;
    drawMenu();
    return;
  }

  if (inBox(tx, ty, SRCH_X, SRCH_Y, SRCH_W, SRCH_H)) {
    state = ST_SEARCH;
    drawSearch();
    return;
  }

  if (inBox(tx, ty, 6, btnY, MENU_BTN_W, MENU_BTN_H)) {
    state = ST_SETTINGS;
    drawSettings();
    return;
  }

  if (inBox(tx, ty, SCR_W - MENU_BTN_W - 6, btnY, MENU_BTN_W, MENU_BTN_H)) {
    lockDevice();
    return;
  }

  int y = MENU_LIST_Y;
  for (int i = menuTop; i < filtCount && i < menuTop + MENU_VISIBLE; i++) {
    if (inBox(tx, ty, 4, y, MENU_ITEM_W, MENU_ITEM_H - 2)) {
      selCred = filt[i];
      state = ST_DETAIL;
      drawDetail();
      return;
    }
    y += MENU_ITEM_H;
  }
}

// ---- Busca (teclado na tela) ----
// 36 teclas A-Z/0-9 + linha de acoes: 0=apagar 1=limpar 2=OK
#ifdef USE_LANDSCAPE
  #define KB_COLS  9
  #define KB_KH    27
  #define KB_Y0    72
#else
  #define KB_COLS  6
  #define KB_KH    28
  #define KB_Y0    74
#endif
#define KB_GAP   3
#define KB_KEYS  36
#define KB_KW    ((SCR_W - 8 - (KB_COLS - 1) * KB_GAP) / KB_COLS)
#define KB_X0    ((SCR_W - (KB_COLS * KB_KW + (KB_COLS - 1) * KB_GAP)) / 2)
#define KB_ACT_H 34
#define KB_ACT_Y (SCR_H - KB_ACT_H - 6)
#define KB_ACT_W ((SCR_W - 8 - 2 * KB_GAP) / 3)

const char kbChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const char* kbActLabels[3] = {"<", "Limpar", "OK"};
const uint16_t kbActColors[3] = {C_WARN, C_WARN, C_OK};

void kbKeyPos(int i, int &x, int &y) {
  x = KB_X0 + (i % KB_COLS) * (KB_KW + KB_GAP);
  y = KB_Y0 + (i / KB_COLS) * (KB_KH + KB_GAP);
}

void drawSearchField() {
  tft.fillRoundRect(4, 36, SCR_W - 8, 30, 4, C_CARD);
  tft.setTextColor(C_TEXT, C_CARD);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(searchBuf, 12, 51, 2);
  char buf[16];
  applyFilter();
  snprintf(buf, sizeof(buf), "%d", filtCount);
  tft.setTextColor(filtCount ? C_DIM : C_WARN, C_CARD);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(buf, SCR_W - 12, 51, 2);
}

void drawSearch() {
  beginScreen();
  focusBegin(ST_SEARCH);
  tft.fillScreen(C_BG);
  tft.setTextColor(C_ACCENT);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Buscar", 10, 8, 4);
  drawSearchField();
  for (int i = 0; i < KB_KEYS; i++) {
    int x, y;
    kbKeyPos(i, x, y);
    char label[2] = { kbChars[i], 0 };
    btn(x, y, KB_KW, KB_KH, label, C_BTN);
  }
  for (int i = 0; i < 3; i++)
    btn(4 + i * (KB_ACT_W + KB_GAP), KB_ACT_Y, KB_ACT_W, KB_ACT_H, kbActLabels[i], kbActColors[i]);
  focusEnd();
}

void closeSearch() {
  menuTop = 0;
  state = ST_MENU;
  drawMenu();
}

void handleSearchTouch(int tx, int ty) {
  int len = strlen(searchBuf);
  for (int i = 0; i < KB_KEYS; i++) {
    int x, y;
    kbKeyPos(i, x, y);
    if (!inBox(tx, ty, x, y, KB_KW, KB_KH)) continue;
    if (len < (int)sizeof(searchBuf) - 1) {
      searchBuf[len] = kbChars[i];
      searchBuf[len + 1] = 0;
      drawSearchField();
    }
    return;
  }
  for (int i = 0; i < 3; i++) {
    if (!inBox(tx, ty, 4 + i * (KB_ACT_W + KB_GAP), KB_ACT_Y, KB_ACT_W, KB_ACT_H)) continue;
    if (i == 0 && len > 0) searchBuf[len - 1] = 0;
    if (i == 1) clearSearch();
    if (i == 2) closeSearch();
    else drawSearchField();
    return;
  }
}

// ---- Detail ----
void drawTypingFeedback(const char* message, uint16_t color) {
  cancelScreenFade();
  // Oculta os dados e retira o foco dos botoes durante o envio.
  detailPasswordVisible = false;
  focusBegin(-1);
  tft.fillScreen(C_BG);
  tft.drawRoundRect(8, SCR_H / 2 - 44, SCR_W - 16, 88, 6, color);
  tft.setTextColor(color, C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(message, SCR_W / 2, SCR_H / 2 - 10, 2);
  tft.setTextColor(C_DIM, C_BG);
  tft.drawString(kbdLabel(), SCR_W / 2, SCR_H / 2 + 16, 2);
}

void typeCredential(int mode) {
  if (!kbdConnected()) {
    drawTypingFeedback("Conecte o teclado", C_WARN);
    typingFeedbackVisible = true;
    typingFeedbackShownAt = millis();
    return;
  }

  drawTypingFeedback("Digitando...", C_ACCENT);
  if (mode == 0 || mode == 1 || mode == 3) {
    kbdPrint(creds[selCred].user);
    if (mode == 0 || mode == 3) {
      kbdWrite(KEY_TAB);
    }
  }
  if (mode == 0 || mode == 2 || mode == 3) {
    kbdPrint(creds[selCred].pass);
  }
  if (mode == 3) kbdWrite(KEY_RETURN);
  if (kbdConnected()) recordCredentialUse(selCred);
  drawTypingFeedback(kbdConnected() ? "Envio finalizado" : "Teclado desconectado",
                     kbdConnected() ? C_OK : C_WARN);
  typingFeedbackVisible = true;
  typingFeedbackShownAt = millis();
  lastAction = millis();
}

// Botoes do detalhe: 0=Tudo 1=Login 2=Senha 3=Voltar 4=Revelar 5=Favoritar 6=Entrar
// Entrar envia login + Tab + senha + Enter (modo 3).
#define DET_BTN_COUNT 7
#define DET_GAP    8
#ifdef USE_LANDSCAPE
  #define DET_BTN_H  38
  #define DET_BTN_W  ((SCR_W - 5 * DET_GAP) / 4)
#else
  #define DET_BTN_H  34
  #define DET_BTN_W  75
#endif

void detailBtnPos(int i, int &x, int &y) {
#ifdef USE_LANDSCAPE
  if (i >= 4) {
    x = (SCR_W - 3 * DET_BTN_W - 2 * DET_GAP) / 2 + (i - 4) * (DET_BTN_W + DET_GAP);
    y = 150;
  } else {
    x = DET_GAP + i * (DET_BTN_W + DET_GAP);
    y = SCR_H - DET_BTN_H - 6;
  }
#else
  int x1 = (SCR_W - 2 * DET_BTN_W - DET_GAP) / 2;
  x = i == 6 ? (SCR_W - DET_BTN_W) / 2 : x1 + (i % 2) * (DET_BTN_W + DET_GAP);
  y = 154 + (i / 2) * (DET_BTN_H + DET_GAP);
#endif
}

const char* detLabels[DET_BTN_COUNT] = {"Tudo", "Login", "Senha", "Voltar", "Revelar", "Favoritar", "Entrar"};

void drawDetailPassword() {
  // Fonte de 6x8: ate 64 caracteres cabem em 3 linhas no portrait.
  // Limpa toda a area para remover tambem as linhas da senha revelada.
  tft.fillRect(0, 120, SCR_W, 24, C_BG);
  tft.setTextColor(C_TEXT, C_BG);
  const int cols = (SCR_W - 20) / 6;
  const char* pass = creds[selCred].pass;
  for (int i = 0; pass[i] && i < 64; i++) {
    tft.drawChar(detailPasswordVisible ? pass[i] : '*',
                 10 + (i % cols) * 6, 120 + (i / cols) * 8, 1);
  }
}

void drawDetail() {
  beginScreen();
  const uint16_t detColors[DET_BTN_COUNT] = {C_OK, C_ACCENT, C_ACCENT, C_DIM, C_ACCENT, C_WARN, C_OK};
  typingFeedbackVisible = false;
  detailPasswordVisible = false;
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
  drawDetailPassword();

  for (int i = 0; i < DET_BTN_COUNT; i++) {
    int x, y;
    detailBtnPos(i, x, y);
    btn(x, y, DET_BTN_W, DET_BTN_H,
        i == 5 && creds[selCred].favorite ? "Desmarcar" : detLabels[i], detColors[i]);
  }
  focusEnd();
}

void handleDetailTouch(int tx, int ty) {
  for (int i = 0; i < DET_BTN_COUNT; i++) {
    int x, y;
    detailBtnPos(i, x, y);
    if (!inBox(tx, ty, x, y, DET_BTN_W, DET_BTN_H)) continue;
    if (i < 3) {
      typeCredential(i);
    } else if (i == 6) {
      typeCredential(3);
    } else if (i == 5) {
      setCredentialFavorite(selCred, !creds[selCred].favorite);
      menuTop = 0;
      drawDetail();
    } else if (i == 4) {
      detailPasswordVisible = true;
      detailPasswordShownAt = millis();
      drawDetailPassword();
    } else {
      state = ST_MENU;
      drawMenu();
    }
    return;
  }
}

// ---- Configuracoes ----
#define SETTINGS_X       10
#define SETTINGS_W       (SCR_W - 20)
#ifdef USE_LANDSCAPE
  #define SETTINGS_H       26
  #define SETTINGS_LOCK_Y  34
  #define SETTINGS_WIFI_Y  66
  #define SETTINGS_THEME_Y 98
  #define SETTINGS_PIN_Y   130
  #define SETTINGS_BRIGHT_Y 162
#else
#define SETTINGS_H       30
#define SETTINGS_LOCK_Y  38
#define SETTINGS_WIFI_Y  96
#define SETTINGS_THEME_Y 130
#define SETTINGS_PIN_Y   164
#define SETTINGS_BRIGHT_Y 198
#endif
#ifdef USE_LANDSCAPE
  #define SETTINGS_EXTRA_W ((SETTINGS_W - 6) / 2)
  #define SETTINGS_SORT_X (SETTINGS_X + SETTINGS_EXTRA_W + 6)
  #define SETTINGS_SORT_Y SETTINGS_BRIGHT_Y
#else
  #define SETTINGS_EXTRA_W SETTINGS_W
  #define SETTINGS_SORT_X SETTINGS_X
  #define SETTINGS_SORT_Y 232
#endif
#define SETTINGS_BACK_Y  (SCR_H - SETTINGS_H - 8)

void drawSettings() {
  beginScreen();
  focusBegin(ST_SETTINGS);
  tft.fillScreen(C_BG);
  tft.setTextColor(C_ACCENT);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Configuracoes", SCR_W / 2, 12, 2);

  char label[24];
  if (autoLockMs < 60000UL)
    snprintf(label, sizeof(label), "Auto-lock: %lu s", autoLockMs / 1000UL);
  else
    snprintf(label, sizeof(label), "Auto-lock: %lu min", autoLockMs / 60000UL);
  btn(SETTINGS_X, SETTINGS_LOCK_Y, SETTINGS_W, SETTINGS_H, label, C_ACCENT);
#ifndef USE_LANDSCAPE
  tft.setTextColor(C_DIM);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Pressione para mudar", SCR_W / 2, SETTINGS_LOCK_Y + SETTINGS_H + 6, 2);
#endif
  btn(SETTINGS_X, SETTINGS_WIFI_Y, SETTINGS_W, SETTINGS_H, "Ativar WiFi", C_BTN);
  btn(SETTINGS_X, SETTINGS_THEME_Y, SETTINGS_W, SETTINGS_H, "Temas", C_ACCENT);
  btn(SETTINGS_X, SETTINGS_PIN_Y, SETTINGS_W, SETTINGS_H, "Alterar PIN", C_BTN);
  btn(SETTINGS_X, SETTINGS_BRIGHT_Y, SETTINGS_EXTRA_W, SETTINGS_H, "Brilho", C_ACCENT);
  btn(SETTINGS_SORT_X, SETTINGS_SORT_Y, SETTINGS_EXTRA_W, SETTINGS_H, "Ordenar", C_BTN);
  btn(SETTINGS_X, SETTINGS_BACK_Y, SETTINGS_W, SETTINGS_H, "Voltar", C_BTN);
  focusEnd();
}

#define BRIGHT_BTN_Y  108
#define BRIGHT_BTN_W  ((SETTINGS_W - 12) / 2)
#define BRIGHT_BTN_H  38

void drawBrightness(const char* status = "", bool failed = false) {
  beginScreen();
  focusBegin(ST_BRIGHTNESS);
  tft.fillScreen(C_BG);
  tft.setTextColor(C_ACCENT);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Brilho", SCR_W / 2, 12, 2);
  char label[8];
  snprintf(label, sizeof(label), "%u%%", (unsigned int)displayBrightness);
  tft.setTextColor(C_TEXT);
  tft.drawString(label, SCR_W / 2, 56, 4);
  btn(SETTINGS_X, BRIGHT_BTN_Y, BRIGHT_BTN_W, BRIGHT_BTN_H, "-", C_BTN);
  btn(SCR_W - SETTINGS_X - BRIGHT_BTN_W, BRIGHT_BTN_Y, BRIGHT_BTN_W, BRIGHT_BTN_H, "+", C_BTN);
  tft.setTextColor(failed ? C_WARN : C_TEXT);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(status, SCR_W / 2, BRIGHT_BTN_Y + BRIGHT_BTN_H + 12, 2);
  btn(SETTINGS_X, SETTINGS_BACK_Y, SETTINGS_W, SETTINGS_H, "Voltar", C_BTN);
  focusEnd();
}

void handleBrightnessTouch(int tx, int ty) {
  int next = displayBrightness;
  if (inBox(tx, ty, SETTINGS_X, BRIGHT_BTN_Y, BRIGHT_BTN_W, BRIGHT_BTN_H))
    next = max(BRIGHTNESS_MIN, next - BRIGHTNESS_STEP);
  else if (inBox(tx, ty, SCR_W - SETTINGS_X - BRIGHT_BTN_W, BRIGHT_BTN_Y, BRIGHT_BTN_W, BRIGHT_BTN_H))
    next = min(BRIGHTNESS_MAX, next + BRIGHTNESS_STEP);
  else if (inBox(tx, ty, SETTINGS_X, SETTINGS_BACK_Y, SETTINGS_W, SETTINGS_H)) {
    state = ST_SETTINGS;
    drawSettings();
    return;
  } else return;
  if (next == displayBrightness) return;
  bool saved = setBrightness(next);
  if (saved) applyBrightness();
  drawBrightness(saved ? "Brilho salvo" : "Falha ao salvar", !saved);
}

// ---- Aba de temas: selecao com aplicacao imediata ----
#define THEME_Y      38
#define THEME_H      28
#define THEME_STEP   34

void drawThemes(const char* status = "", bool failed = false) {
  beginScreen();
  focusBegin(ST_THEMES);
  tft.fillScreen(C_BG);
  tft.setTextColor(C_ACCENT);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Temas", SCR_W / 2, 12, 2);
  for (size_t i = 0; i < THEME_COUNT; i++) {
    char label[24];
    snprintf(label, sizeof(label), "%s%s", i == activeTheme ? "* " : "", COLOR_THEMES[i].name);
    btn(SETTINGS_X, THEME_Y + i * THEME_STEP, SETTINGS_W, THEME_H,
        label, COLOR_THEMES[i].accent);
  }
  tft.setTextColor(failed ? C_WARN : C_TEXT);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(status, SCR_W / 2, THEME_Y + THEME_COUNT * THEME_STEP, 2);
  btn(SETTINGS_X, SETTINGS_BACK_Y, SETTINGS_W, SETTINGS_H, "Voltar", C_BTN);
  focusEnd();
}

void handleThemesTouch(int tx, int ty) {
  for (size_t i = 0; i < THEME_COUNT; i++) {
    if (!inBox(tx, ty, SETTINGS_X, THEME_Y + i * THEME_STEP, SETTINGS_W, THEME_H)) continue;
    bool saved = setTheme(i);
    drawThemes(saved ? "Tema salvo" : "Falha ao salvar", !saved);
    return;
  }
  if (inBox(tx, ty, SETTINGS_X, SETTINGS_BACK_Y, SETTINGS_W, SETTINGS_H)) {
    state = ST_SETTINGS;
    drawSettings();
  }
}

void drawSort(const char* status = "", bool failed = false) {
  beginScreen();
  focusBegin(ST_SORT);
  tft.fillScreen(C_BG);
  tft.setTextColor(C_ACCENT);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Ordenacao", SCR_W / 2, 12, 2);
  for (int i = 0; i < SORT_COUNT; i++) {
    char label[24];
    snprintf(label, sizeof(label), "%s%s", i == credentialSort ? "* " : "", SORT_LABELS[i]);
    btn(SETTINGS_X, THEME_Y + i * THEME_STEP, SETTINGS_W, THEME_H, label, C_ACCENT);
  }
  tft.setTextColor(failed ? C_WARN : C_DIM);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(status, SCR_W / 2, THEME_Y + SORT_COUNT * THEME_STEP, 2);
  btn(SETTINGS_X, SETTINGS_BACK_Y, SETTINGS_W, SETTINGS_H, "Voltar", C_BTN);
  focusEnd();
}

void handleSortTouch(int tx, int ty) {
  for (int i = 0; i < SORT_COUNT; i++) {
    if (!inBox(tx, ty, SETTINGS_X, THEME_Y + i * THEME_STEP, SETTINGS_W, THEME_H)) continue;
    bool saved = setCredentialSort(i);
    if (saved) menuTop = 0;
    drawSort(saved ? "Ordem salva" : "Falha ao salvar", !saved);
    return;
  }
  if (inBox(tx, ty, SETTINGS_X, SETTINGS_BACK_Y, SETTINGS_W, SETTINGS_H)) {
    state = ST_SETTINGS;
    drawSettings();
  }
}

void handleSettingsTouch(int tx, int ty) {
  if (inBox(tx, ty, SETTINGS_X, SETTINGS_LOCK_Y, SETTINGS_W, SETTINGS_H)) {
    size_t next = 0;
    for (size_t i = 0; i < AUTOLOCK_OPTION_COUNT; i++)
      if (autoLockMs == AUTOLOCK_OPTIONS_MS[i]) next = (i + 1) % AUTOLOCK_OPTION_COUNT;
    bool saved = setAutoLock(AUTOLOCK_OPTIONS_MS[next]);
    drawSettings();
    if (!saved) {
#ifdef USE_LANDSCAPE
      tft.fillRect(0, 190, SCR_W, 16, C_BG);
      tft.setTextColor(C_WARN);
      tft.setTextDatum(TC_DATUM);
      tft.drawString("Falha ao salvar", SCR_W / 2, 190, 2);
#else
      tft.fillRect(0, SETTINGS_LOCK_Y + SETTINGS_H + 6, SCR_W, 16, C_BG);
      tft.setTextColor(C_WARN);
      tft.setTextDatum(TC_DATUM);
      tft.drawString("Falha ao salvar", SCR_W / 2, SETTINGS_LOCK_Y + SETTINGS_H + 6, 2);
#endif
    }
  } else if (inBox(tx, ty, SETTINGS_X, SETTINGS_WIFI_Y, SETTINGS_W, SETTINGS_H)) {
    startWeb();
    state = ST_WEB;
    drawWebScreen();
  } else if (inBox(tx, ty, SETTINGS_X, SETTINGS_THEME_Y, SETTINGS_W, SETTINGS_H)) {
    state = ST_THEMES;
    drawThemes();
  } else if (inBox(tx, ty, SETTINGS_X, SETTINGS_BRIGHT_Y, SETTINGS_EXTRA_W, SETTINGS_H)) {
    state = ST_BRIGHTNESS;
    drawBrightness();
  } else if (inBox(tx, ty, SETTINGS_SORT_X, SETTINGS_SORT_Y, SETTINGS_EXTRA_W, SETTINGS_H)) {
    state = ST_SORT;
    drawSort();
  } else if (inBox(tx, ty, SETTINGS_X, SETTINGS_PIN_Y, SETTINGS_W, SETTINGS_H)) {
    clearPinBuffers();
    pinLen = storagePinLen();
    state = ST_PIN_CURRENT;
    drawPinScreen("PIN atual");
  } else if (inBox(tx, ty, SETTINGS_X, SETTINGS_BACK_Y, SETTINGS_W, SETTINGS_H)) {
    state = ST_MENU;
    drawMenu();
  }
}

// ---- Web Screen ----
#define WEB_BTN_W  140
#define WEB_BTN_H  42

void drawWebScreen() {
  beginScreen();
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
  state = ST_SETTINGS;
  drawSettings();
}

void handleWebTouch(int tx, int ty) {
  if (inBox(tx, ty, (SCR_W - WEB_BTN_W) / 2, WEB_BTN_Y, WEB_BTN_W, WEB_BTN_H)) {
    closeWeb();
  }
}

// ---- Gesto "voltar" (botao fisico segurado) ----
void handleBack() {
  switch (state) {
    case ST_PIN_CURRENT:
    case ST_PIN_NEW:
    case ST_PIN_NEW_CONFIRM:
      cancelPinChange();
      break;
    case ST_PIN_SETUP:
    case ST_PIN_CONFIRM:
    case ST_PIN_ENTRY:
      pinBackspace();
      break;
    case ST_MENU:
      // Com filtro ativo, "voltar" limpa a busca antes de bloquear
      if (searchBuf[0]) { clearSearch(); drawMenu(); }
      else lockDevice();
      break;
    case ST_SEARCH:
      closeSearch();
      break;
    case ST_DETAIL:
    case ST_SETTINGS:
      state = ST_MENU;
      drawMenu();
      break;
    case ST_WEB:
      closeWeb();
      break;
    case ST_THEMES:
    case ST_BRIGHTNESS:
    case ST_SORT:
      state = ST_SETTINGS;
      drawSettings();
      break;
  }
}

// ---- Lock ----
void lockDevice() {
  typingFeedbackVisible = false;
  detailPasswordVisible = false;
  clearMemory();
  clearPinBuffers();
  clearSearch();
  pinLen = storagePinLen();
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

  if (!storageBegin()) {
    clearMemory();
    focusBegin(-1);
    tft.fillScreen(C_BG);
    tft.setTextColor(C_WARN);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Falha no storage", SCR_W / 2, SCR_H / 2 - 12, 2);
    tft.drawString("Reinicie o aparelho", SCR_W / 2, SCR_H / 2 + 12, 2);
    return;
  }

  // TFT_eSPI inicializa o backlight ligado: restaura o PWM depois do init.
  applyBrightness();

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
  if (!storageReady) return;
  if (screenFade.tick(millis())) applyBrightness();
  // Mantem a confirmacao visivel sem bloquear o loop ou o auto-lock.
  if (state == ST_DETAIL && typingFeedbackVisible &&
      millis() - typingFeedbackShownAt >= TYPING_FEEDBACK_MS) {
    drawDetail();
  }
  // Sem delay: o dispositivo continua respondendo durante os 3 segundos.
  // Subtracao sem sinal mantem o prazo correto mesmo quando millis() transborda.
  if (state == ST_DETAIL && detailPasswordVisible &&
      millis() - detailPasswordShownAt >= DETAIL_REVEAL_MS) {
    detailPasswordVisible = false;
    drawDetailPassword();
  }

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

  if ((state == ST_MENU || state == ST_DETAIL || state == ST_SEARCH || state == ST_SETTINGS || state == ST_THEMES || state == ST_BRIGHTNESS || state == ST_SORT || changingPin()) && millis() - lastAction >= autoLockMs) {
    lockDevice();
    return;
  }

  // Atualiza o indicador quando o host BLE conecta/desconecta
  if (state == ST_MENU && kbdConnected() != kbdShownConnected) drawKbdStatus();

  int tx, ty, gesture;
  bool fading = screenFade.active;
  if (!inputRead(tx, ty, gesture)) return;
  // Consome toques durante o fade para nao ativar a tela recem-aberta.
  if (fading) return;
  if (state == ST_DETAIL && typingFeedbackVisible) return;

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
    case ST_PIN_CURRENT:
    case ST_PIN_NEW:
    case ST_PIN_NEW_CONFIRM:
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
    case ST_SEARCH:
      handleSearchTouch(tx, ty);
      break;
    case ST_SETTINGS:
      handleSettingsTouch(tx, ty);
      break;
    case ST_THEMES:
      handleThemesTouch(tx, ty);
      break;
    case ST_SORT:
      handleSortTouch(tx, ty);
      break;
    case ST_BRIGHTNESS:
      handleBrightnessTouch(tx, ty);
      break;
  }
}
