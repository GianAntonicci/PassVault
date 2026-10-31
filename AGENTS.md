# AGENT.md — PassVault

Context instructions for any AI or developer working on this project.

## Git usage by agents

- Use Git only when the user explicitly requests Git operations. This applies to read-only commands (such as `status`, `diff` and `log`), local changes (such as branches, commits and merges), and remote operations (such as fetch, pull, push and pull requests).
- A request to implement, review, test or fix code does not authorize Git usage. Without an explicit Git request, inspect and edit files and run builds directly without invoking Git or GitHub repository tools.
- When the user requests a Git workflow, perform the operations necessary to complete that workflow within the requested scope. Use semantic commit messages (Conventional Commits), such as `feat: ...`, `fix: ...` and `docs: ...`.
- Write all commit messages (subject and body) and pull request titles and descriptions entirely in English, regardless of the conversation language. Preserve code identifiers, file paths and literal UI labels when referencing them.
- Never infer authorization for a force push, history rewrite, branch deletion or merging a pull request from a request to commit, push or open a pull request.

## Releases with pushes and pull requests

- Whenever the user requests a push or a pull request, include publishing a GitHub Release as part of that workflow, unless the user explicitly asks to skip it. This is standing authorization to create the necessary version tag and release; do not ask for separate confirmation.
- Publish exactly one release per pull request, covering all changes and commits in that PR. Never publish separate releases for individual commits in the same PR. A workflow that includes both a push and a PR must publish only that single release. For a push-only workflow, publish one release covering all commits delivered in that push. Check existing tags and releases first to avoid duplicates. Tag the PR's exact head commit (or the exact pushed head commit for a push-only workflow), and link the release and PR in the final response.
- Use semantic versions with a `v` prefix. Choose the next version from existing releases and the highest-impact Conventional Commit across all changes included in the release: breaking changes increment the major version, features the minor version, and fixes or documentation the patch version. If no version exists, start at `v0.1.0`. Never move or overwrite an existing tag or release.
- For commits on `dev` or another branch awaiting integration into `main`, publish a GitHub prerelease with a unique suffix, such as `v0.1.0-rc.1`. Publish a stable release only for a commit already integrated into `main`. This does not authorize merging a PR automatically.
- Include release notes describing all changes in the PR (or push-only delivery), validation performed and known limitations. Complete the applicable build checks before publishing; do not claim hardware validation unless it was actually performed.

## Overview

PassVault is a hardware password manager running on ESP32-S3 boards with native USB HID. It stores encrypted credentials in NVS (flash), unlocks with a 6-digit PIN, and types login/password into a computer by emulating a keyboard (native USB HID). Credentials are managed through a web interface served on a temporary WiFi Access Point.

## Tech stack

- **Framework:** Arduino (via PlatformIO)
- **Platform:** espressif32
- **Language:** C++ (Arduino-style)
- **Display:** TFT_eSPI (Bodmer); Arduino_GFX SPI/QSPI transport for JD9853 and AMOLED
- **Encryption:** mbedTLS (AES-256-CBC, PBKDF2-HMAC-SHA256)
- **Storage:** NVS (Preferences.h)
- **USB HID:** TinyUSB (native on ESP32-S3)

## Supported boards

| PlatformIO environment   | Board                     | Display            | Input               | Keyboard |
|--------------------------|---------------------------|--------------------|----------------------|----------|
| `tdisplay-s3-touch`      | LilyGO T-Display S3 Touch | ST7789 170×320 parallel 8-bit | CST816S (capacitive touch) | USB HID |
| `tdisplay-s3`            | LilyGO T-Display S3       | ST7789 170×320 parallel 8-bit | 2 physical buttons (GPIO 0, 14) | USB HID |
| `waveshare-s3-touch-1_69` | Waveshare ESP32-S3-Touch-LCD-1.69 | ST7789V2 240×280 SPI, 1.69″ | CST816T capacitive | USB HID |
| `waveshare-s3-touch-1_83` | Waveshare ESP32-S3-Touch-LCD-1.83 | ST7789P 240×284 SPI, 1.83″ | CST816D capacitive | USB HID |
| `waveshare-s3-touch-1_54` | Waveshare ESP32-S3-Touch-LCD-1.54 | 1.54″ ST7789, 240×240 SPI | CST816T capacitive | USB HID |
| `waveshare-s3-touch-1_47` | Waveshare ESP32-S3-Touch-LCD-1.47 | 1.47″ JD9853, 172×320 SPI, portrait | AXS5106L capacitive | USB HID |
| `waveshare-s3-amoled-1_64` | Waveshare ESP32-S3-Touch-AMOLED-1.64 **V1.1** | 1.64″ CO5300, 280×456 QSPI, portrait | FT3168 capacitive | USB HID |
| `tdisplay-s3-amoled-touch` | LilyGO T-Display-S3 AMOLED **Touch** | 1.91″ RM67162, 240×536 QSPI, portrait | CST816T capacitive | USB HID |

To build for a specific board: `pio run -e <environment>`
To upload: `pio run -e <environment> --target upload`

## Project structure

```
PassVault/
├── include/
│   ├── config.h          # Board profiles, constants, RGB565 colors
│   ├── display.h         # Direct TFT or PSRAM renderer with SPI/QSPI transport
│   ├── jd9853_init.h     # Manufacturer JD9853 register sequence
│   ├── input.h           # Input abstraction (capacitive touch, buttons)
│   ├── keyboard.h        # Native USB HID keyboard
│   ├── ui_layout.h       # Shared screen geometry
│   ├── touch_gesture.h   # Polled touch tap/swipe state machine
│   ├── vault_core.h      # AES-256-CBC encryption, PBKDF2, NVS storage
│   └── vault_web.h       # WiFi AP, WebServer, HTML SPA, REST API
├── src/
│   └── main.cpp          # UI (TFT_eSPI), state machine, numpad, menus
├── platformio.ini        # Multi-board config with TFT_eSPI build_flags
├── LICENSE               # MIT License
└── README.md
```

## Architecture

### State machine (main.cpp)

```
ST_PIN_SETUP  →  ST_PIN_CONFIRM  →  ST_PIN_ENTRY (first use)
                                          ↓
ST_PIN_ENTRY  →  ST_MENU  →  ST_DETAIL
                    ↓
                 ST_WEB
```

- `ST_PIN_SETUP` / `ST_PIN_CONFIRM`: PIN creation on first use
- `ST_PIN_ENTRY`: Unlock screen with numpad
- `ST_MENU`: Credential list with scroll, WiFi and Lock buttons
- `ST_DETAIL`: Single credential view, with typing options (All/Login/Password)
- `ST_WEB`: Active WiFi AP screen (SSID, password, IP)

### Abstraction layers

- **input.h** — Unified interface: `inputBegin()`, `inputRead(x, y, gesture)`. Each board implements its own backend (CST816 family, AXS5106L, FT3168, GPIO buttons). Polled touch emits one event on release; the LilyGO AMOLED touch transform follows native portrait coordinates. Supported gestures: tap, swipe up/down, back. Focus system for button-only boards.
- **keyboard.h** — Unified interface: `kbdBegin()`, `kbdPrint()`, `kbdWrite()`, `kbdConnected()`. Uses native USB HID on all supported ESP32-S3 boards (`KBD_USB_HID` in config.h).
- **vault_core.h** — All encryption and persistence. AES key derived via PBKDF2-HMAC-SHA256 (20k iterations) from PIN + salt. Each credential is individually encrypted with a random IV. Storage format: `name|user|pass` → AES-256-CBC → hex string in NVS.
- **vault_web.h** — WiFi AP with random 10-character password, WebServer with SPA in PROGMEM. REST API: `GET /api/list`, `GET /api/get?id=N`, `POST /api/save`, `POST /api/del?id=N`, `POST /api/export`, `POST /api/import` (encrypted JSON backup protected by a separate backup password). Limit of 1 client, auto-shutdown after 5 min idle.

### Layout

Layout is selected at compile time:
- **Portrait (170×320)**: T-Display S3 / T-Display S3 Touch
- **Portrait (240×280 / 240×284)**: Waveshare 1.69″ / 1.83″, using compact controls from `ui_layout.h`
- **Square (240×240)**: Waveshare 1.54″; compact PIN/settings, 3-column detail actions, 9-column search keyboard
- **Portrait (172×320 / 280×456 / 240×536)**: Waveshare 1.47″ / AMOLED 1.64″ V1.1 / LilyGO AMOLED Touch 1.91″
- All new profiles are build-tested only until physical display, touch, battery and USB HID validation is performed. Manufacturer pin references are linked in README.md.
- **`MENU_SCROLL_BTNS`**: On-screen scroll buttons for boards without swipe (T-Display S3 with buttons)

## Security

- **PIN**: 6 digits (configurable up to 8). Double SHA-256 hash stored in NVS.
- **Key derivation**: PBKDF2-HMAC-SHA256, 20,000 iterations, random 16-byte salt.
- **Encryption**: AES-256-CBC with random IV per credential.
- **Progressive lockout**: After 5 wrong attempts, 30s lockout doubling on each error. On the 10th error, full vault wipe.
- **Auto-lock**: Locks and clears RAM after 60s of inactivity.
- **Persistent counters**: Failed attempts survive reboot.

### Known limitations

- Web UI uses plain HTTP (protected only by the AP's WPA2 password)
- Flash dump allows offline PIN brute-force
- Flash encryption and Secure Boot are NOT enabled (they burn irreversible eFuses)
- Only ASCII characters are supported by HID
- HID keyboard layout assumes US International

## Code conventions

- `.h` headers in `include/` contain implementation (not just declarations)
- `vault_core.h` and `vault_web.h` use global variables with `extern` across modules
- All UI uses the TFT_eSPI drawing API through `VaultDisplay` (no UI framework); `display.h` selects direct rendering or a PSRAM sprite with Arduino_GFX transport
- Interface strings are in **Portuguese** (PT-BR)
- Colors defined as RGB565 constants in `config.h` (prefix `C_`)
- Board macros (`BOARD_*`) control conditional compilation across all headers
- `PROGMEM` used for the SPA HTML (saves RAM)

## How to add a new board

1. Create a new `BOARD_*` profile in `config.h` with pins, screen size, and input/keyboard type
2. Add the input backend in `input.h` with `#ifdef BOARD_*`
3. Require native USB HID support and define `KBD_USB_HID`; boards needing an external keyboard module are not supported
4. Adapt the portrait layout if needed and select whether it needs `MENU_SCROLL_BTNS`
5. Create a new `[env:name]` in `platformio.ini` with the correct TFT_eSPI `build_flags`
6. Test: PIN setup → PIN entry → add credential via web → type via USB HID

## How to add a new feature

- Storage/crypto features go in `vault_core.h`
- Web API/UI features go in `vault_web.h`
- Screen/interaction features go in `main.cpp`
- Maintain compatibility with all eight supported profiles (use `#ifdef` when needed)
- New API endpoints must be registered in `startWeb()` inside `vault_web.h`
- Credential limits: name 32 chars, user/pass 64 chars, max 30 credentials

## Build and upload

```bash
# Build for a specific board
pio run -e tdisplay-s3-touch

# Upload (ESP32-S3 may require BOOT+RST)
pio run -e tdisplay-s3-touch --target upload

# Serial monitor
pio device monitor -b 115200
```

**ESP32-S3 note**: If upload fails with "No serial data received", hold BOOT → press/release RST → release BOOT → try uploading again.

## Dependencies (lib_deps)

- `bodmer/TFT_eSPI` — Display/fonts (all boards)
- `moononournation/GFX Library for Arduino@1.6.9` — JD9853 SPI and CO5300/RM67162 QSPI transport
- `fbiego/CST816S` — Capacitive touch (T-Display S3 Touch only)

## Links

- **Repository**: https://github.com/GianAntonicci/PassVault
- **License**: MIT
