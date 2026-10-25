#ifndef CONFIG_H
#define CONFIG_H

// =====================================================================
//  Configuracao por placa (BOARD_XXX vem do platformio.ini)
// =====================================================================

#if defined(BOARD_TDISPLAY_S3_TOUCH)
  #define BOARD_NAME      "T-Display S3 Touch"
  #define SCR_W           170
  #define SCR_H           320
  #define TFT_ROTATION    0
  #define PIN_POWER_ON    15
  // Touch CST816S (I2C)
  #define TOUCH_SDA       18
  #define TOUCH_SCL       17
  #define TOUCH_RST       21
  #define TOUCH_INT       16
  #define TOUCH_DEBOUNCE  300
  #define KBD_USB_HID

#elif defined(BOARD_TDISPLAY_S3)
  #define BOARD_NAME      "T-Display S3"
  #define SCR_W           170
  #define SCR_H           320
  #define TFT_ROTATION    0
  #define PIN_POWER_ON    15
  // Botoes fisicos (ativos em LOW)
  #define BTN_DOWN        0    // baixo / confirma (segurar)
  #define BTN_UP          14   // cima / volta (segurar)
  #define BTN_LONG_MS     600
  #define TOUCH_DEBOUNCE  0    // botoes ja tem debounce proprio em input.h
  #define KBD_USB_HID

#elif defined(BOARD_CYD)
  #define BOARD_NAME      "CYD 2432S028"
  #define SCR_W           320
  #define SCR_H           240
  #define TFT_ROTATION    1
  #define USE_LANDSCAPE
  #define PIN_POWER_ON    -1
  // XPT2046 em barramento SPI separado (VSPI)
  #define XPT_CLK         25
  #define XPT_MISO        39
  #define XPT_MOSI        32
  #define XPT_CS          33
  #define XPT_IRQ         36
  // Faixa crua do ADC — ajuste se o toque estiver deslocado
  #define XPT_X_MIN       200
  #define XPT_X_MAX       3700
  #define XPT_Y_MIN       240
  #define XPT_Y_MAX       3800
  #define TOUCH_DEBOUNCE  300
  #define KBD_BLE
  #define MENU_SCROLL_BTNS   // touch resistivo sem gesto: botoes de rolagem no menu

#elif defined(BOARD_GENERIC_ILI9341)
  #define BOARD_NAME      "ESP32 + ILI9341"
  #define SCR_W           320
  #define SCR_H           240
  #define TFT_ROTATION    1
  #define USE_LANDSCAPE
  #define PIN_POWER_ON    -1
  // Calibracao do XPT2046 (gere com o exemplo TFT_eSPI "Touch_calibrate")
  #define TOUCH_CAL_DATA  { 300, 3500, 300, 3500, 1 }
  #define TOUCH_DEBOUNCE  300
  #define KBD_BLE
  #define MENU_SCROLL_BTNS   // touch resistivo sem gesto: botoes de rolagem no menu

#else
  #error "Defina uma placa: -DBOARD_TDISPLAY_S3_TOUCH, -DBOARD_TDISPLAY_S3, -DBOARD_CYD ou -DBOARD_GENERIC_ILI9341"
#endif

// =====================================================================
//  Constantes compartilhadas
// =====================================================================

// Settings
#define MAX_CREDS      30
#define AUTOLOCK_MS     60000UL  // padrao quando nao ha preferencia salva
constexpr unsigned long AUTOLOCK_OPTIONS_MS[] = {30000UL, 60000UL, 120000UL, 300000UL, 600000UL};
constexpr size_t AUTOLOCK_OPTION_COUNT = sizeof(AUTOLOCK_OPTIONS_MS) / sizeof(AUTOLOCK_OPTIONS_MS[0]);

// ---- Seguranca ----
// Tamanho do PIN de vaults NOVOS (vaults antigos continuam com 4)
#define PIN_LEN          6
#define PIN_LEN_MAX      8
// Iteracoes do PBKDF2-HMAC-SHA256 (mais = mais lento p/ brute force e p/ desbloquear)
#define KDF_ITERS        20000

// Tentativas erradas de PIN (contador persistente, sobrevive a reboot):
//   a partir de LOCKOUT_AFTER erros, espera LOCKOUT_MS, dobrando a cada erro
//   ao chegar em WIPE_AFTER erros, o vault inteiro e APAGADO (0 = nunca apagar)
#define LOCKOUT_AFTER    5
#define LOCKOUT_MS       30000
#define LOCKOUT_MAX_MS   600000
#define WIPE_AFTER       10

// WiFi AP: senha aleatoria nova a cada ativacao, 1 cliente por vez,
// desliga e bloqueia apos WEB_IDLE_MS sem uso
#define AP_SSID          "PassVault"
#define AP_PASS_LEN      10
#define WEB_IDLE_MS      300000

// Nome anunciado pelo teclado BLE
#define BLE_KBD_NAME  "PassVault"

// Paletas RGB565. A ordem tambem identifica o tema salvo na NVS.
struct ColorTheme {
  const char* name;
  uint16_t text, accent, button, dim, card;
};
const ColorTheme COLOR_THEMES[] = {
  {"Cyber Green", 0x07E0, 0x07FF, 0xBDF7, 0x8410, 0x18E3},
  {"Azul",        0x9DFF, 0x5DFF, 0xC6FF, 0x8494, 0x0847},
  {"Roxo",        0xD5FF, 0xC39F, 0xE67F, 0x9495, 0x2086},
  {"Ambar",       0xFEA0, 0xFD20, 0xFF18, 0x9C70, 0x28C0}
};
constexpr size_t THEME_COUNT = sizeof(COLOR_THEMES) / sizeof(COLOR_THEMES[0]);
uint8_t activeTheme = 0;

// Cores semanticas: avisos e confirmacoes mantem vermelho/verde.
#define C_BG      0x0000
#define C_TEXT    (COLOR_THEMES[activeTheme].text)
#define C_ACCENT  (COLOR_THEMES[activeTheme].accent)
#define C_BTN     (COLOR_THEMES[activeTheme].button)
#define C_WARN    0xF800
#define C_OK      0x07E0
#define C_DIM     (COLOR_THEMES[activeTheme].dim)
#define C_CARD    (COLOR_THEMES[activeTheme].card)

#endif
