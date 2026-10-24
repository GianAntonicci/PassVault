# PassVault

A hardware password manager built on the ESP32. PassVault stores your credentials encrypted on the device. It unlocks with a PIN and **types the username and/or password into any computer** by acting as a keyboard: USB HID on ESP32-S3, Bluetooth LE on the classic ESP32. You add and edit credentials from a phone or laptop through a temporary Wi-Fi access point with a built-in web UI.

It is a PlatformIO project with one firmware and a build environment for each supported board.

---

## Features

- **PIN lock:** a 6-digit PIN entered on an on-screen numpad. The PIN is created on first boot.
- **Attempt limit:** the wrong-PIN counter is stored in flash, so rebooting does not reset it. After 5 wrong PINs the device locks for 30 s, and the wait doubles with each further error. On the 10th wrong PIN, **the whole vault is erased**.
- **Auto-lock:** after 60 s of inactivity in the menu or detail screen, the device locks and the decrypted credentials are wiped from RAM.
- **Encrypted storage:** each credential is AES-256-CBC encrypted with a random IV and stored in NVS flash.
- **Auto-typing:** each credential offers *All* (user + TAB + password), *Login* (user only) or *Password* (password only).
- **Wi-Fi management:** an on-demand Wi-Fi access point with a single-page web app to list, add, edit and delete credentials. Each time it is turned on it gets a **new random password**, accepts only one client, and shuts down after 5 minutes without use.
- **Search by name:** filter the menu list with an on-screen A–Z/0–9 keyboard. Matching ignores case and finds the text anywhere in the name.
- **Password generator:** the web UI creates random passwords with a chosen length and character types.
- **Encrypted backup:** export all credentials to a JSON file protected by a backup password, and import them again on the same or another device.
- **Up to 30 credentials:** name ≤ 32 chars, user ≤ 64, password ≤ 64.
- **Several boards:** portrait and landscape layouts, capacitive, resistive or button-only input, and a USB or BLE keyboard.

---

## Supported boards

| Environment | Board | MCU | Display | Input | Keyboard |
|---|---|---|---|---|---|
| `tdisplay-s3-touch` | LilyGO T-Display S3 Touch | ESP32-S3 | ST7789 170×320, 8-bit parallel, portrait | CST816S capacitive (I²C) with gestures | USB HID |
| `tdisplay-s3` | LilyGO T-Display S3 (no touch) | ESP32-S3 | ST7789 170×320, 8-bit parallel, portrait | 2 physical buttons (GPIO0 / GPIO14) | USB HID |
| `cyd-2432s028` | "Cheap Yellow Display" ESP32-2432S028 | ESP32 | ILI9341 320×240 SPI, landscape | XPT2046 resistive (own SPI bus) | BLE |
| `generic-ili9341` | Any ESP32 + ILI9341 module | ESP32 | ILI9341 320×240 SPI, landscape | XPT2046 resistive (shared SPI bus) | BLE |

---

## Installation

### 1. What you need

| Item | Notes |
|---|---|
| A supported board | See [Supported boards](#supported-boards) |
| USB cable **with data lines** | Many cheap cables only carry power. If the board is not detected, try another cable first |
| USB-serial driver (classic ESP32 boards only) | The CYD and most generic ESP32 boards use a **CH340** or **CP210x** chip. macOS and Linux usually work out of the box. On Windows, install the [CH340](https://www.wch-ic.com/downloads/CH341SER_EXE.html) or [CP210x](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) driver. The T-Display S3 uses native USB and needs no driver |
| [VS Code](https://code.visualstudio.com/) + [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) | **or** the [PlatformIO Core CLI](https://docs.platformio.org/en/latest/core/installation/index.html), which needs Python 3 |
| Git (optional) | To clone the repository |

### 2. Get the code

```bash
git clone <repository-url> PassVault
cd PassVault
```

You can also download and unzip the folder. The PlatformIO project root is the folder that contains `platformio.ini`.

### 3. Wire the display (generic ESP32 + ILI9341 only)

The other boards come assembled and you can skip this step. For the generic build, wire the module following the [pin table](#generic-esp32--ili9341--generic-ili9341). The touch controller shares the SPI bus with the display, so MOSI, MISO and SCLK each go to both. Power the module from 3.3 V.

### 4. Build and flash

**Option A — VS Code**

1. Open the `PassVault` folder in VS Code (*File → Open Folder*). PlatformIO detects the project and downloads the toolchain and libraries. The first time takes a few minutes.
2. In the status bar at the bottom, click the environment selector (`Default (PassVault)`) and choose your board, for example `env:tdisplay-s3-touch`.
3. Connect the board and click **→ Upload** in the status bar.
4. Optional: click the **plug icon** (Serial Monitor) to see logs at 115200 baud.

**Option B — command line**

```bash
pio device list                              # find the board's serial port
pio run -e tdisplay-s3-touch -t upload       # build + flash (pick your env)
pio device monitor                           # optional, 115200 baud
```

| Board | Environment |
|---|---|
| T-Display S3 Touch | `tdisplay-s3-touch` |
| T-Display S3 (no touch) | `tdisplay-s3` |
| CYD ESP32-2432S028 | `cyd-2432s028` |
| CYD ESP32-2432S028 with ST7789 (micro-USB + USB-C variant) | `cyd-2432s028-2usb` |
| Generic ESP32 + ILI9341 | `generic-ili9341` |

**Notes**

- **Upload to a single board:** always pass `-e <env>`, for example `pio run -e tdisplay-s3-touch --target upload` (`-t` is short for `--target`). Without `-e`, PlatformIO builds and tries to upload **every** environment in `platformio.ini`.
- **Multiple serial ports:** if several ports are connected, choose one with `--upload-port /dev/cu.usbmodem1101` (on Windows, `COM5`), or set `upload_port` in the env.
- **T-Display S3 upload fails:** because the board runs as a USB keyboard, its serial port may not respond. Put it in download mode by holding **BOOT**, pressing and releasing **RST**, then releasing **BOOT**, and upload again. Press **RST** after flashing to start the firmware.
- **Classic ESP32 upload fails** with "Failed to connect": hold **BOOT** while the upload says `Connecting....`.
- **Build every board:** `pio run` with no `-e`. All libraries download automatically, and the BLE Keyboard fix is applied automatically.

### 5. Board-specific first steps

- **T-Display S3 / T-Display S3 Touch:** plug the board into the computer you want to type into. It shows up as a USB keyboard; no pairing is needed.
- **CYD / generic:**
  1. Check the touch first. If taps land in the wrong place, calibrate it ([CYD](#cyd-esp32-2432s028--cyd-2432s028), [generic](#generic-esp32--ili9341--generic-ili9341)), then flash again.
  2. On the computer or phone, open the Bluetooth settings and pair with **"PassVault"**.
  3. The `BLE` label in the menu turns green when the host is connected.

### 6. First use

1. **Create a PIN:** on first boot, enter a 6-digit PIN twice. Unlocking takes a moment ("Verificando...") because of the key derivation.
2. **Turn on Wi-Fi:** tap **WiFi**. The screen shows the network `PassVault`, a **random password** and the URL (normally `http://192.168.4.1`).
3. **Add credentials:** connect your phone or laptop to that network, open the URL and add your credentials.
4. **Turn off Wi-Fi:** tap **Desligar WiFi**. The AP also turns itself off after 5 minutes without use.
5. **Type a credential:** put the cursor in a login field on the computer, select a credential on the device and tap **Tudo** (user + TAB + password), **Login** or **Senha**.

> The on-screen UI is in Portuguese: *Tudo* = All, *Login* = Login, *Senha* = Password, *Voltar* = Back, *Bloquear* = Lock, *Desligar WiFi* = Turn off Wi-Fi.

> The host keyboard layout should be **US**, because HID sends US key codes. With another layout, symbols in passwords may come out wrong.

### 7. Updating the firmware

Flash again with the same `-t upload` command. **Your credentials and PIN are kept.** They live in the NVS partition, which a normal upload does not overwrite. Vaults from older firmware are converted to the new format on the first unlock (see [Upgrading from the old format](#security-model)).

Credentials can be lost in these cases:

- The partition table (`board_build.partitions`) changes.
- You run `-t erase`.
- You flash a different board environment that uses another layout.

Before such changes, **export a backup** from the web UI (see [Backup](#backup-export--import)) and import it again afterwards.

### 8. Resetting the vault

There are two ways to erase everything (PIN and all credentials):

- **On the device:** enter a wrong PIN `WIPE_AFTER` times (10 by default).
- **From the computer:** erase the whole flash, then flash the firmware again.

  ```bash
  pio run -e <env> -t erase
  pio run -e <env> -t upload
  ```

---

## Usage

### Controls by board

| Action | Capacitive touch (T-Display S3 Touch) | Resistive touch (CYD / generic) | Buttons (T-Display S3) |
|---|---|---|---|
| Press a button | Tap | Tap firmly (one tap = one press; holding does not repeat) | Move the focus with short presses, then **hold GPIO0** |
| Scroll the menu | Swipe up / down | Tap the **▲ / ▼** buttons beside the list | Move the focus past the first or last visible item |
| Back | *Voltar* button | *Voltar* button | **Hold GPIO14** |

> **Resistive boards (CYD / generic):** these boards have no swipe gesture, so the menu shows **▲ / ▼ buttons** to the right of the list when there are more than 4 credentials. A button is greyed out when the list cannot scroll further in that direction.

On the button board, the focused button has a **cyan outline**. A short press on **GPIO0** moves to the next button, and a short press on **GPIO14** moves to the previous one. Holding GPIO14 does the following:

| Screen | Result of holding GPIO14 |
|---|---|
| PIN | Deletes the last digit |
| Detail | Goes back to the menu |
| Wi-Fi | Turns Wi-Fi off |
| Search | Closes the keyboard and keeps the filter |
| Menu | Clears the search if a filter is active, otherwise locks the device |

### Unlocking

1. Enter your PIN on the numpad. **C** clears everything and **<** deletes the last digit.
2. When the last digit is entered, the device checks the PIN automatically. "Verificando..." stays on screen briefly.
3. A wrong PIN shows "PIN errado (n/5)". See [Wrong-PIN limit](#security-model) for what happens after repeated errors. **On the 10th wrong PIN in a row, the vault is erased.**

### Searching by name

1. In the menu, tap the **Buscar...** field.
2. Type part of the name on the on-screen keyboard (A–Z, 0–9). The field shows how many credentials match.
3. **<** deletes the last character, **Limpar** clears the search, and **OK** goes back to the filtered list.

Matching ignores case and finds the text anywhere in the name, so `MAIL` matches *Gmail* and *Hotmail*. While a filter is active, the menu shows only matching credentials, or "Nada encontrado" if there are none. To clear the filter, open the search and tap **Limpar**, or press **Voltar** in the menu (hold GPIO14 on the button board). The search is also cleared when the device locks.

### Typing a credential

1. On the computer, **click the field** where the text should go. For *Tudo*, click the username field.
2. On the device, select the credential in the menu to open its detail screen. It shows the name and username, with the password masked.
3. Choose what to type:

| Button | Types | Typical use |
|---|---|---|
| **Tudo** | username, **TAB**, password | Login forms where TAB moves to the password field |
| **Login** | username only | Two-step logins (username first, then the next page) |
| **Senha** | password only | Password-only prompts, or the second step of a two-step login |
| **Voltar** | — | Back to the menu |

Use **Revelar** on the detail screen to display the password for **3 seconds**. It is then masked automatically; pressing the button again restarts the timer. Leaving the screen also hides the password. Long passwords wrap to fit the display.

The device does **not** press Enter, so you submit the form yourself. This avoids sending a password to the wrong window by accident.

**BLE boards:** typing only works while the `BLE` label in the menu is **green** (a host is connected). When it is grey, nothing is sent.

### Managing credentials (web UI)

1. In the menu, tap **WiFi**.
2. On your phone or laptop, connect to the network **PassVault** using the password shown on the device. The password changes every time Wi-Fi is turned on.
3. Open the URL shown on screen, normally `http://192.168.4.1`.

| Task | How |
|---|---|
| Add | **+ Nova Credencial** → fill in *Nome*, *Usuário / Email* and *Senha* → **Salvar** |
| View | Tap a credential in the list. The password is shown in plain text |
| Edit | Open it → **Editar** → change the fields → **Salvar** |
| Delete | Open it → **Excluir** → confirm |
| Generate a password | In the add/edit form, set the length and character types, then tap **Gerar** |
| Cancel / back | **Cancelar** / **Voltar** |

**Password generator**

The **Gerar** button next to the password field fills it with a random password. The options below the field are:

| Option | Default | Effect |
|---|---|---|
| *Tamanho* | 16 | Length, from 4 to 64 |
| *A-Z* | on | Upper-case letters |
| *0-9* | on | Digits |
| *Símbolos* | on | `!@#$%&*()-_=+[]{};:,.<>?/` |

Lower-case letters are always used, and the password contains at least one character of every selected type. The browser's secure random generator (`crypto.getRandomValues`) is used without modulo bias. The symbol set leaves out quotes, `` ` ``, `\`, `|`, `^` and `~`, which can be stored or typed incorrectly (`^` and `~` are dead keys on US International).

**Limits**

| Field | Limit |
|---|---|
| Name | 32 characters |
| Username | 64 characters |
| Password | 64 characters |
| Credentials | 30 in total |

When the vault is full, new credentials are not saved.

### Backup (export & import)

At the bottom of the web UI, type a **backup password** (at least 8 characters) in *Senha do backup*, then:

- **Exportar** downloads `passvault-backup-YYYY-MM-DD.json` with all credentials.
- **Importar** asks for a backup file and decrypts it with the password in the field.

Imported credentials are **added** to the vault. A credential with the same name and username as an existing one is skipped, and so is anything past the 30-credential limit. The page reports how many were imported and how many were skipped. A wrong password shows "Senha do backup incorreta". A damaged or modified file shows "Arquivo inválido".

The backup password is separate from the PIN, so a backup can be restored after changing the PIN, after a vault wipe, or on another board. The encryption runs on the device, because browsers block Web Crypto on plain-HTTP pages. The file format is:

```json
{"app":"PassVault","v":1,"iter":20000,"salt":"<hex>","ver":"<hex>","mac":"<hex>","creds":["<hex>", "..."]}
```

- The backup key is derived with **PBKDF2-HMAC-SHA256** (`iter` rounds) from the backup password and a new random 16-byte `salt` on each export.
- Each `creds` entry is `name|user|pass`, encrypted with **AES-256-CBC** and a random IV (IV + ciphertext, hex).
- `ver` checks the password, and `mac` (**HMAC-SHA256** over the salt and all entries) detects a damaged or modified file.

> Anyone who has the file can try to guess the backup password offline. Use a long, unique password and keep the file somewhere safe.

When you are done, tap **Desligar WiFi** on the device. If you forget, Wi-Fi turns off and the device locks after **5 minutes** without use.

> Only one device can be connected to the Wi-Fi at a time. If the connection is refused, disconnect the other phone or laptop first.

### Locking

| How | When |
|---|---|
| **Bloquear** button in the menu | Immediately |
| Auto-lock | After **60 s** without input in the menu or detail screen |
| Wi-Fi idle | After **5 min** without input or web requests on the Wi-Fi screen |
| Power off / reset | The decrypted credentials only exist in RAM, so they are gone |

When the device locks, the encryption key and all decrypted credentials are wiped from memory.

### Tips and limitations

- **Keyboard layout:** set the computer's keyboard layout to **US** (or US International without dead keys). The device sends US key codes. With another layout, for example ABNT2 or AZERTY, symbols such as `@ " ; / ?` can come out as different characters.
- **ASCII only:** use letters, digits and standard symbols. Accented characters (`á ç ã`), emoji and other non-ASCII characters **cannot be typed**. Avoid them in usernames and passwords.
- **Check the focus first:** the device types into whatever window has focus.
- **Back up your credentials.** If the vault is erased (10 wrong PINs, `-t erase`), the credentials are gone. Export a [backup](#backup-export--import) regularly.

### Screen text reference (Portuguese → English)

| On screen | Meaning |
|---|---|
| Criar PIN / Confirme PIN | Create PIN / Confirm PIN |
| Digite PIN / Digite N digitos | Enter PIN / Enter N digits |
| PINs diferentes! | PINs don't match |
| PIN errado (n/5) | Wrong PIN (n of 5 before lockout) |
| Verificando... / Gerando chave... | Checking... / Generating key... |
| BLOQUEADO · Aguarde Ns | LOCKED · Wait N s |
| Apaga apos N erros | Erased after N more errors |
| VAULT APAGADO | Vault erased |
| Nenhuma credencial | No credentials |
| Buscar... / Buscar | Search |
| Limpar / OK | Clear / OK |
| Nada encontrado | No matches |
| WiFi / Bloquear | Wi-Fi / Lock |
| Usuario / Senha | Username / Password |
| Tudo / Login / Senha / Voltar | All / Username / Password / Back |
| WiFi Ativo / Desligar WiFi | Wi-Fi on / Turn off Wi-Fi |
| Web: Nova Credencial, Nome, Salvar, Editar, Excluir, Cancelar | New credential, Name, Save, Edit, Delete, Cancel |
| Web: Gerar, Tamanho, Símbolos | Generate, Length, Symbols |
| Web: Senha do backup, Exportar, Importar | Backup password, Export, Import |

---

## How it works

### Screens (state machine in `src/main.cpp`)

```
          ┌──────────────┐  first boot   ┌──────────────┐
  boot ──►│  Create PIN  │──────────────►│ Confirm PIN  │──┐
          └──────────────┘               └──────────────┘  │
          ┌──────────────┐   correct PIN                    ▼
  boot ──►│  Enter PIN   │─────────────────────────────►┌────────┐
          └──────────────┘◄──── Lock / auto-lock ───────│  Menu  │
                 │ 5 wrong                              └────────┘
                 ▼                                       │      │
          30 s lockout                         select item│      │WiFi
                                                         ▼      ▼
                                                   ┌────────┐ ┌──────────┐
                                                   │ Detail │ │ Wi-Fi AP │
                                                   └────────┘ └──────────┘

Menu ──► Search (tap "Buscar...") ──► OK ──► Menu (filtered)
```

- **PIN screens:** a numpad with digits, `C` (clear) and `<` (backspace).
- **Menu:** a scrollable credential list (swipe on the touch board, or button navigation) plus *WiFi* and *Bloquear* (Lock) buttons. The top-right corner shows `USB` or `BLE`. For BLE it is green only while a host is paired.
- **Detail:** shows the name and username with the password masked, plus the *Tudo / Login / Senha / Voltar* buttons.
- **Wi-Fi:** shows the SSID, password and URL of the access point.

### Security model

**Key derivation**

```
master      = PBKDF2-HMAC-SHA256(PIN, salt, KDF_ITERS = 20000)
AES key     = SHA-256(master || "key")
verifier    = SHA-256(master || "ver")      ← stored in NVS; the PIN never is
```

- **Salt:** 16 random bytes created when the PIN is set.
- **PIN check:** the verifier is compared in constant time.
- **Credential storage:** each credential is encrypted with AES-256-CBC and a random IV, then stored as hex in the NVS namespace `pv`. When decrypting, the PKCS#7 padding is fully validated.
- **Memory wipe:** the key, PIN buffers and decrypted plaintext are cleared with `mbedtls_platform_zeroize` after use and when the device locks.

**Wrong-PIN limit**

| Wrong PINs in a row | Result |
|---|---|
| 1–4 | "PIN errado (n/5)" |
| 5 | 30 s lockout |
| 6, 7, 8, 9 | lockout of 60 s, 120 s, 240 s, 480 s (capped by `LOCKOUT_MAX_MS`) |
| 10 | **vault erased**, device restarts on "Criar PIN" |

- **Survives reboots:** the counter lives in NVS. It is incremented *before* the PIN is checked, so cutting power during the check does not help. If you reboot while locked out, the wait starts again.
- **Countdown:** the lockout screen counts down and shows how many errors remain before the vault is erased.
- **Configuration:** the limits are set with `LOCKOUT_AFTER`, `LOCKOUT_MS`, `LOCKOUT_MAX_MS` and `WIPE_AFTER` in `config.h`. Set `WIPE_AFTER 0` to never erase.
- **Trade-off:** erasing protects the data, but anyone holding the device can also erase it on purpose.

**Wi-Fi access point**

- **Password:** a 10-character random password is generated each time the AP starts, shown on screen, and wiped when the AP stops.
- **Client limit:** only 1 client can connect at a time.
- **Idle shutdown:** after `WEB_IDLE_MS` (5 min) with no touches and no web requests, the AP turns off and the device locks.
- **No caching:** responses carry `Cache-Control: no-store`.

**Upgrading from the old format (v1)**

Vaults created by earlier firmware (4-digit PIN, single SHA-256) are migrated automatically on the first correct unlock, and keep the same PIN. The migration survives power loss: credentials are first re-encrypted into temporary keys, the new verifier is written as the commit point, and only then are the old entries replaced. To get a 6-digit PIN, erase the vault and set it up again.

**Remaining limitations**

- **Offline guessing is slowed, not stopped.** Someone who dumps the flash can still try every 6-digit PIN offline. PBKDF2 makes each guess slower, but 1,000,000 PINs is still within reach of a GPU. The on-device attempt limit does not apply to a flash dump. For real protection against physical attacks, enable ESP32 **flash encryption** (with NVS encryption) and **secure boot**. These are one-way eFuse operations; see [Hardening: flash encryption & secure boot](#hardening-flash-encryption--secure-boot).
- **The web UI is plain HTTP.** It is protected only by the WPA2 password of the AP.
- **A paired BLE host receives keystrokes.** Pair the device only with computers you trust.
- **Unlock speed:** `KDF_ITERS` sets how long an unlock takes. More iterations make offline guessing slower but also make unlocking slower. Tune it on your board.

### Code layout

```
platformio.ini              one [env:...] per board + shared base sections
scripts/
  patch_blekeyboard.py      pre-build fix of the BLE Keyboard lib for core 3.x
include/
  config.h                  per-board settings (selected by BOARD_xxx) + shared constants
  input.h                   input abstraction: CST816S / XPT2046 / buttons
  keyboard.h                keyboard abstraction: USB HID / BLE
  vault_core.h              crypto + NVS storage (hardware-independent)
  vault_web.h               Wi-Fi AP + web server + HTML single-page app (hardware-independent)
src/
  main.cpp                  UI, layout (portrait/landscape) and state machine
```

**`input.h`** exposes two functions:

```cpp
void inputBegin();
bool inputRead(int &x, int &y, int &gesture);   // true on a new event
```

Gestures use the CST816S IDs (`GESTURE_SWIPE_DOWN = 1`, `GESTURE_SWIPE_UP = 2`), plus `GESTURE_BACK` for the button board. It also contains a small **focus ring** (`focusBegin / focusAdd / focusEnd`). Every on-screen button registers itself there, so the button-only board can highlight and "tap" it. On touch boards these calls do nothing.

**`keyboard.h`** exposes these functions:

```cpp
void kbdBegin();
void kbdPrint(const char*);
void kbdWrite(uint8_t);
bool kbdConnected();
const char* kbdLabel();
```

### Web API (while the access point is on)

| Method | Path | Purpose |
|---|---|---|
| GET | `/` | Single-page web app |
| GET | `/api/list` | List credential names |
| GET | `/api/get` | Get one credential |
| POST | `/api/save` | Add or update a credential |
| POST | `/api/del` | Delete a credential |
| POST | `/api/export` | Download an encrypted backup (`pw` = backup password) |
| POST | `/api/import` | Import a backup (`pw`, `iter`, `salt`, `ver`, `mac`, `n`, `c0`…`cN`) |

---

## Board-specific setup

### LilyGO T-Display S3 Touch — `tdisplay-s3-touch`

- **Pins:** display TFT_CS 6, DC 7, RST 5, WR 8, RD 9, D0–D7 = 39, 40, 41, 42, 45, 46, 47, 48, backlight 38. Touch SDA 18, SCL 17, RST 21, INT 16. Power-enable on GPIO15, driven HIGH at boot.
- **Input:** tap buttons, and swipe up or down to scroll the menu.
- **USB:** plug the board into the target computer through the USB-C port. It enumerates as a USB keyboard and a CDC serial port at the same time (`ARDUINO_USB_MODE=0`, `ARDUINO_USB_CDC_ON_BOOT=1`).
- **Flashing:** if upload fails because the port disappeared (TinyUSB took over USB), hold **BOOT (GPIO0)**, press **RST**, release BOOT, and upload again.

### LilyGO T-Display S3 (non-touch) — `tdisplay-s3`

The display pins are the same as on the Touch version. Navigation uses the two side buttons:

| Button | Short press | Hold (≥ 600 ms) |
|---|---|---|
| **GPIO0** (BOOT) | move focus to the next button | **confirm** (taps the focused button) |
| **GPIO14** (KEY) | move focus to the previous button | **back**: backspace on the PIN screen, Detail → Menu, turns Wi-Fi off, locks from the Menu |

The focused button has a cyan outline. In the menu, moving past the first or last visible item scrolls the list. You can change the hold time with `BTN_LONG_MS` in `config.h`.

### CYD ESP32-2432S028 — `cyd-2432s028`

- **Display:** ILI9341 on HSPI (`USE_HSPI_PORT`) with MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, RST –1 and backlight 21, at 27 MHz SPI.
- **Touch:** the XPT2046 is on its **own VSPI bus**: CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36. `tft.getTouch()` cannot reach it, so this board uses the `PaulStoffregen/XPT2046_Touchscreen` library.
- **Calibration:** if taps land in the wrong place, adjust the raw ADC range in `config.h`:

  ```cpp
  #define XPT_X_MIN 200
  #define XPT_X_MAX 3700
  #define XPT_Y_MIN 240
  #define XPT_Y_MAX 3800
  ```

  Print `touch.getPoint()` over Serial while touching the corners to find your values. If an axis is mirrored, swap its MIN and MAX.
- **Display variants:** some CYD revisions need `-DILI9341_2_DRIVER=1` instead of `-DILI9341_DRIVER=1`, or need `-DTFT_INVERSION_ON=1`. Use those if the colours look inverted or the image is garbled.
- **ST7789 variant (often micro-USB + USB-C):** use `pio run -e cyd-2432s028-2usb --target upload`. This profile replaces the ILI9341 driver with ST7789, uses BGR colours and keeps the 320×240 landscape layout and separate touch SPI bus. Either USB connector can be used for flashing. USB connector count alone does not guarantee the panel type; keep `cyd-2432s028` for ILI9341 panels. If only the colours are inverted, change `TFT_INVERSION_OFF` to `TFT_INVERSION_ON` in the ST7789 profile.
- **Keyboard:** BLE (see [BLE keyboard](#ble-keyboard-classic-esp32-boards)).

### Generic ESP32 + ILI9341 — `generic-ili9341`

Wiring (VSPI, touch shares the bus):

| Signal | GPIO |
|---|---|
| TFT MOSI / T_DIN | 23 |
| TFT MISO / T_DO | 19 |
| TFT SCLK / T_CLK | 18 |
| TFT CS | 5 |
| TFT DC | 16 |
| TFT RST | 17 |
| TFT backlight (LED) | 4 |
| Touch CS (T_CS) | 27 |

**Touch calibration is required.** Run the TFT_eSPI example `Touch_calibrate` with rotation 1 and the same build flags. Then copy the five values it prints into `config.h`:

```cpp
#define TOUCH_CAL_DATA { 300, 3500, 300, 3500, 1 }
```

### BLE keyboard (classic ESP32 boards)

- **Pairing:** pair the computer or phone with the Bluetooth device **"PassVault"**. You can change the name with `BLE_KBD_NAME` in `config.h`. The menu shows `BLE` in green when a host is connected. Typing does nothing while no host is connected.
- **Library fix:** these boards use [`t-vk/ESP32 BLE Keyboard`](https://github.com/T-vK/ESP32-BLE-Keyboard), which does not compile on Arduino-ESP32 core 3.x without changes. `scripts/patch_blekeyboard.py` runs before every build and fixes it automatically. It is safe to run repeatedly. It does the following:
  - `BLEDevice::init(deviceName)` → `BLEDevice::init(String(deviceName.c_str()))`
  - `setValue(deviceManufacturer)` → `setValue(String(deviceManufacturer.c_str()))`
  - removes the unused `#include <driver/adc.h>`, which triggers a deprecation warning
- **Flash size:** BLE + Wi-Fi together take about 1.7 MB of flash, so all boards use the `huge_app.csv` partition table, which has no OTA.

---

## Configuration reference (`include/config.h`)

Settings shared by every board:

| Constant | Default | Meaning |
|---|---|---|
| `MAX_CREDS` | 30 | Maximum number of stored credentials |
| `PIN_LEN` | 6 | PIN length for new vaults (max `PIN_LEN_MAX` = 8) |
| `KDF_ITERS` | 20000 | PBKDF2 iterations |
| `LOCKOUT_AFTER` | 5 | Wrong PINs before the first lockout |
| `LOCKOUT_MS` / `LOCKOUT_MAX_MS` | 30000 / 600000 | First lockout duration (doubles each error) and its cap |
| `WIPE_AFTER` | 10 | Wrong PINs that erase the vault (0 = never) |
| `AP_SSID` / `AP_PASS_LEN` | `PassVault` / 10 | AP name and length of the random password |
| `WEB_IDLE_MS` | 300000 | Wi-Fi idle time before the AP shuts down and the device locks |
| `AUTOLOCK_MS` | 60000 | Inactivity time before auto-lock |
| `BLE_KBD_NAME` | `PassVault` | Bluetooth device name |
| `C_*` | — | UI colours (RGB565) |

Settings for each board (inside the `#if defined(BOARD_xxx)` blocks): `SCR_W`, `SCR_H`, `TFT_ROTATION`, `USE_LANDSCAPE`, `PIN_POWER_ON`, the touch or button pins, `TOUCH_DEBOUNCE`, calibration, the keyboard type (`KBD_USB_HID` or `KBD_BLE`), and `MENU_SCROLL_BTNS`, which shows ▲/▼ menu buttons on boards without swipe gestures.

Display driver and pin settings for TFT_eSPI are passed as `build_flags` in `platformio.ini` (`USER_SETUP_LOADED=1`), so the TFT_eSPI library's own `User_Setup.h` is never edited.

---

## Adding a new board

1. **Add an environment:** in `platformio.ini`, add an `[env:my-board]` that `extends` the closest base section (`tdisplay_s3_base` or `ili9341_ble_base`). Set the TFT_eSPI driver and pin flags and add `-DBOARD_MY_BOARD=1`. Include only the libraries this board needs in `lib_deps`.
2. **Add the board settings:** in `include/config.h`, add an `#elif defined(BOARD_MY_BOARD)` block with the screen size, rotation, `USE_LANDSCAPE` if the screen is wider than tall, the input pins, `TOUCH_DEBOUNCE`, and `KBD_USB_HID` (ESP32-S3) or `KBD_BLE` (ESP32).
3. **Add input support if needed:** if the board's input hardware is new, add a branch to `include/input.h` that implements `inputBegin()` and `inputRead()`.
4. **Build it** with `pio run -e my-board`.

The layout in `main.cpp` supports two sizes: portrait 170×320 and landscape 320×240. Other resolutions may need changes to the `PIN_*`, `NP_*` and `WEB_*` layout constants at the top of `main.cpp`.

---

## Hardening: flash encryption & secure boot

These are **not enabled** by this project. Without them, anyone with physical access can dump the flash and brute-force the PIN offline. Enabling them closes that gap, but read the risks first.

### ⚠️ Why this is not on by default

- **The changes are permanent.** Both features burn **eFuses** in the chip. A wrong configuration or a lost key can **brick the board permanently**, and re-flashing will not recover it.
- **The signing key must never be lost.** Secure boot needs an RSA-3072 key file (`.pem`) that you keep. If you lose it, you can never flash that board again. Back it up somewhere safe and never commit it.
- **The board must be connected.** The eFuses are burned on the first boot of the new firmware, or with `espefuse.py`, so this always happens on the physical board.
- **Try it on a spare board first.**

### What would need to change

1. **Framework configuration.** The Arduino framework is precompiled without these options. The pioarduino platform supports `custom_sdkconfig`, which rebuilds the framework with your own sdkconfig. The first build is much slower.
2. **sdkconfig options:**
   - `CONFIG_SECURE_FLASH_ENC_ENABLED=y`: flash encryption
   - `CONFIG_NVS_ENCRYPTION=y`: needed because the credentials live in NVS, and NVS is *not* covered by flash encryption alone
   - `CONFIG_SECURE_BOOT_V2_ENABLED=y` + `CONFIG_SECURE_BOOT_SIGNING_KEY="secure_boot_signing_key.pem"`: only signed firmware can boot
3. **Partition table.** A custom table that adds an `nvs_keys` partition, marked `encrypted`, next to `nvs`.
4. **Mode.**
   - **Development:** you can still re-flash over USB/serial, a limited number of times. Use this for testing.
   - **Release:** everything is locked and UART download is disabled. On the T-Display S3 this also affects flashing over USB. Use this only for a final, tested build.

Example of a separate environment (sketch, untested):

```ini
[env:tdisplay-s3-touch-secure]
extends = env:tdisplay-s3-touch
board_build.partitions = partitions_secure.csv   ; includes nvs_keys
custom_sdkconfig =
    CONFIG_SECURE_FLASH_ENC_ENABLED=y
    CONFIG_SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT=y
    CONFIG_NVS_ENCRYPTION=y
```

### Suggested rollout

1. Create a separate `-secure` environment and keep the normal environments unchanged.
2. Enable **flash encryption + NVS encryption in development mode** only, and flash a spare board.
3. Confirm that it boots, the PIN works, credentials persist and USB/BLE typing still works.
4. Only then add **secure boot** and, when you are fully satisfied, switch to **release** mode.

See Espressif's documentation for the exact steps for your chip (ESP32 and ESP32-S3 differ): *Flash Encryption*, *Secure Boot v2* and *NVS Encryption* in the ESP-IDF Programming Guide.

---

## Build notes

- **Arduino-ESP32 core:** this project targets core **3.x**, built with core 3.3.12.
- **Library finder:** `lib_ldf_mode = deep` is required. With the default mode, the core's own libraries fail to find their internal dependencies, for example `USB → FS.h` and `WiFi → Network.h`. Because `deep` ignores `#if`, the BLE environments set `lib_ignore = USB`.
- **Library warnings:** `DISABLE_ALL_LIBRARY_WARNINGS` silences TFT_eSPI's informational warnings, and `-Wno-attributes` silences a warning inside the CST816S library. Every environment builds with no warnings.

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| `FS.h` / `Network.h: No such file or directory` | Make sure `lib_ldf_mode = deep` is set |
| Upload fails on T-Display S3 | Enter download mode: hold BOOT, press RST, release BOOT |
| Nothing typed on USB boards | Use the USB-C data port and make sure the host recognised a keyboard |
| Nothing typed on BLE boards | Check that `BLE` is green in the menu and pair again if needed. On macOS, accept the keyboard-identification prompt |
| Wrong characters typed | Set the host keyboard layout to **US**. HID sends US key codes |
| Touch offset or mirrored (CYD / generic) | Recalibrate (see the board sections above) |
| White or garbled CYD screen | Try `ILI9341_2_DRIVER` and/or `TFT_INVERSION_ON` |
| Forgot the PIN | Enter 10 wrong PINs, or erase the flash with `pio run -e <env> -t erase` and upload again. **All credentials are lost** |
| Unlock takes too long | Lower `KDF_ITERS` (this makes offline guessing faster) |
