#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

// Shared drawing and hit-test geometry for every supported portrait panel.
#if SCR_H <= 240
#define PIN_TITLE_Y 4
#define PIN_MSG_Y 32
#define PIN_WARN_Y 48
#define PIN_BOX_Y 66
#define PIN_BOX_H 24
#else
#define PIN_TITLE_Y 20
#define PIN_MSG_Y 60
#define PIN_WARN_Y 75
#define PIN_BOX_Y 95
#define PIN_BOX_H 32
#endif
#define NP_BW 48
#define NP_GAP 4
#define NP_COLS 3
#define NP_ROWS 4
#define PIN_CANCEL_H 28
#define PIN_CANCEL_Y (SCR_H - PIN_CANCEL_H - 6)
#define WEB_TITLE_Y 30
#define WEB_LINE_Y 80
#define WEB_BTN_Y ((SCR_H < 260) ? SCR_H - 48 : 200)

#define DET_BTN_COUNT 7
#define DET_BTN_W 75
#define DET_COLS ((SCR_H <= 240) ? 3 : 2)
#define DET_USER_LABEL_Y ((SCR_H <= 240) ? 38 : 50)
#define DET_USER_Y ((SCR_H <= 240) ? 56 : 70)
#define DET_PASS_LABEL_Y ((SCR_H <= 240) ? 78 : 100)
#define DET_PASS_Y ((SCR_H <= 240) ? 98 : 120)
#define KB_COLS ((SCR_H <= 240) ? 9 : 6)
#define KB_Y0 74
#define KB_GAP 3
#define KB_KEYS 36
#define KB_KW ((SCR_W - 8 - (KB_COLS - 1) * KB_GAP) / KB_COLS)
#define KB_X0 ((SCR_W - (KB_COLS * KB_KW + (KB_COLS - 1) * KB_GAP)) / 2)
#define KB_ACT_H 34
#define KB_ACT_Y (SCR_H - KB_ACT_H - 6)
#define KB_ACT_W ((SCR_W - 8 - 2 * KB_GAP) / 3)

#define SETTINGS_X 10
#define SETTINGS_W (SCR_W - 20)
#define SETTINGS_EXTRA_W SETTINGS_W
#define SETTINGS_SORT_X SETTINGS_X
#define SETTINGS_BACK_Y (SCR_H - SETTINGS_H - 8)

#if SCR_H <= 240
  #define NP_BH 24
  #define NP_Y0 96
  #define DET_BTN_H 30
  #define DET_GAP 3
  #define DET_Y0 132
  #define KB_KH 26
  #define SETTINGS_H 22
  #define SETTINGS_LOCK_Y 30
  #define SETTINGS_WIFI_Y 76
  #define SETTINGS_THEME_Y 102
  #define SETTINGS_PIN_Y 128
  #define SETTINGS_BRIGHT_Y 154
  #define SETTINGS_SORT_Y 180
#elif SCR_H < 320
  #define NP_BH 26
  #define NP_Y0 130
  #define DET_BTN_H 28
  #define DET_GAP 4
  #define DET_Y0 146
  #define KB_KH 24
  #define SETTINGS_H 26
  #define SETTINGS_LOCK_Y 34
  #define SETTINGS_WIFI_Y 86
  #define SETTINGS_THEME_Y 116
  #define SETTINGS_PIN_Y 146
  #define SETTINGS_BRIGHT_Y 176
  #define SETTINGS_SORT_Y 206
#else
  #define NP_BH 32
  #define NP_Y0 140
  #define DET_BTN_H 34
  #define DET_GAP 8
  #define DET_Y0 154
  #define KB_KH 28
  #define SETTINGS_H 30
  #define SETTINGS_LOCK_Y 38
  #define SETTINGS_WIFI_Y 96
  #define SETTINGS_THEME_Y 130
  #define SETTINGS_PIN_Y 164
  #define SETTINGS_BRIGHT_Y 198
  #define SETTINGS_SORT_Y 232
#endif

#endif
