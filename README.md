# PassVault

A hardware password manager built on the ESP32-S3. PassVault stores your credentials encrypted on the device. It unlocks with a PIN and **types the username and/or password into any computer** by acting as a keyboard: native USB HID on ESP32-S3, without external keyboard modules or Bluetooth pairing. You add and edit credentials from a phone or laptop through a temporary Wi-Fi access point with a built-in web UI.

It is a PlatformIO project with one firmware and a build environment for each supported board.

---

## Features

- **PIN lock:** a 6-digit PIN entered on an on-screen numpad. The PIN is created on first boot.
- **Attempt limit:** the wrong-PIN counter is stored in flash, so rebooting does not reset it. After 5 wrong PINs the device locks for 30 s, and the wait doubles with each further error. On the 10th wrong PIN, **the whole vault is erased**.
- **Auto-lock:** after a configurable inactivity timeout (60 s by default), the device locks and the decrypted credentials are wiped from RAM. Choose 30 s, 60 s, 2 min, 5 min or 10 min in **Config. → Auto-lock**; the choice survives a restart.
- **Display brightness:** open **Config. → Brilho** and use **−/+** to adjust from 10% to 100% in 10% steps (default: 100%). Changes apply immediately and are saved in NVS, including across restarts. Touch and physical-button navigation are supported.
- **Screen transitions:** a non-blocking 180 ms fade eases in each new screen and returns to your selected brightness. Redrawing the same screen does not restart the animation. Auto-lock and Wi-Fi processing remain active; taps during the fade are discarded. SPI/parallel profiles draw directly; JD9853 and QSPI profiles use a PSRAM framebuffer.
- **Languages:** device and web UI support Portuguese (default), English, Spanish, Mandarin (Simplified Chinese), Italian, Japanese, German and Russian. Change **Config. → Idioma** on the device or **Idioma / Language** at the top of the web UI. The choice is shared, takes effect immediately and survives restarts and PIN changes.
- **Encrypted storage:** each credential is AES-256-CBC encrypted with a random IV and stored in NVS flash.
- **Auto-typing:** each credential offers *All* (user + TAB + password), *Login* (user only), *Password* (password only) or **Entrar** (user + TAB + password + ENTER, submitting the form).
- **Wi-Fi management:** an on-demand Wi-Fi access point with a single-page web app to list, add, edit and delete credentials. Each time it is turned on it gets a **new random password**, accepts only one client, and shuts down after 5 minutes without use.
- **Search by name:** filter the menu list with an on-screen A–Z/0–9 keyboard. Matching ignores case and finds the text anywhere in the name.
- **Password generator:** the web UI creates random passwords with a chosen length and character types.
- **Favorites:** mark frequently used credentials with **Favoritar** on the device or web detail screen. Favorites appear first (within each category when grouped by type), including in search results, and survive locking and rebooting. Use **Desmarcar** to remove the mark.
- **Credential sorting:** select **Config. → Ordenar** on the device or **Ordenar por** in the web UI. Choose **Alfabética** (default, ignores letter case), **Mais recente** (newest registrations first; edits keep their original position) or **Mais usada** (highest keyboard-send count first, with alphabetical ties). Favorites stay first in these modes, including search results. **Por tipo** groups credentials by category, with favorites first within each group and **Sem tipo** last. The same preference applies to the device and web list and survives restarts. Each completed *All/Login/Password/Entrar* send while the keyboard remains connected counts once; viewing a credential does not count. Existing credentials start with zero uses. Usage counters remain on the device and are reset for imported credentials; backups do not include the sorting preference or usage history.
- **Credential types:** choose **Redes Sociais** (purple), **Email** (cyan), **Banco** (green), **Trabalho** (yellow), or **Sem tipo** (gray) when adding or editing a credential in the web UI. Colored markers and text labels identify types on the device and web list. Select **Por tipo** under **Config. → Ordenar** or web **Ordenar por** to group them. The preference and categories survive restarts and PIN changes. Existing credentials default to **Sem tipo**. Category metadata is stored in NVS alongside favorites and usage counts; in backups it is encrypted.
- **Encrypted backup:** export all credentials to a JSON file protected by a backup password, and import them again on the same or another device.
- **Up to 30 credentials:** name ≤ 32 chars, user ≤ 64, password ≤ 64.
- **Native USB boards:** T-Display S3 / S3 Touch, T-Display S3 AMOLED Touch (1.91″), and compact Waveshare ESP32-S3 touch profiles (1.47″, 1.54″, 1.64″, 1.69″ and 1.83″). Every profile uses USB HID without an external keyboard module; all newly added profiles still require hardware validation.

---

## Supported boards

| Environment | Board | MCU | Display | Input | Keyboard |
|---|---|---|---|---|---|
| `tdisplay-s3-touch` | LilyGO T-Display S3 Touch | ESP32-S3 | ST7789 170×320, 8-bit parallel, portrait | CST816S capacitive (I²C) with gestures | USB HID |
| `tdisplay-s3` | LilyGO T-Display S3 (no touch) | ESP32-S3 | ST7789 170×320, 8-bit parallel, portrait | 2 physical buttons (GPIO0 / GPIO14) | USB HID |
| `waveshare-s3-touch-1_69` | Waveshare ESP32-S3-Touch-LCD-1.69 | ESP32-S3 | 1.69″ ST7789V2, 240×280 SPI, portrait | CST816T capacitive | USB HID |
| `waveshare-s3-touch-1_83` | Waveshare ESP32-S3-Touch-LCD-1.83 | ESP32-S3 | 1.83″ ST7789P, 240×284 SPI, portrait | CST816D capacitive | USB HID |
| `waveshare-s3-touch-1_54` | Waveshare ESP32-S3-Touch-LCD-1.54 | ESP32-S3 | 1.54″ ST7789, 240×240 SPI | CST816T capacitive | USB HID |
| `waveshare-s3-touch-1_47` | Waveshare ESP32-S3-Touch-LCD-1.47 | ESP32-S3 | 1.47″ JD9853, 172×320 SPI, portrait | AXS5106L capacitive | USB HID |
| `waveshare-s3-amoled-1_64` | Waveshare ESP32-S3-Touch-AMOLED-1.64 **V1.1** | ESP32-S3 | 1.64″ CO5300, 280×456 QSPI, portrait | FT3168 capacitive | USB HID |
| `tdisplay-s3-amoled-touch` | LilyGO T-Display-S3 AMOLED **Touch** | ESP32-S3 | 1.91″ RM67162, 240×536 QSPI, portrait | CST816T capacitive | USB HID |

---

## Installation

### 1. What you need

| Item | Notes |
|---|---|
| A supported board | See [Supported boards](#supported-boards) |
| USB cable **with data lines** | Many cheap cables only carry power. If the board is not detected, try another cable first |
| [VS Code](https://code.visualstudio.com/) + [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) | **or** the [PlatformIO Core CLI](https://docs.platformio.org/en/latest/core/installation/index.html), which needs Python 3 |
| Git (optional) | To clone the repository |

### 2. Get the code

```bash
git clone <repository-url> PassVault
cd PassVault
```

You can also download and unzip the folder. The PlatformIO project root is the folder that contains `platformio.ini`.

### 3. Build and flash

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
| Waveshare S3 Touch 1.69″ | `waveshare-s3-touch-1_69` |
| Waveshare S3 Touch 1.83″ | `waveshare-s3-touch-1_83` |
| Waveshare S3 Touch 1.54″ | `waveshare-s3-touch-1_54` |
| Waveshare S3 Touch 1.47″ | `waveshare-s3-touch-1_47` |
| Waveshare S3 AMOLED 1.64″ V1.1 | `waveshare-s3-amoled-1_64` |
| T-Display S3 AMOLED Touch 1.91″ | `tdisplay-s3-amoled-touch` |

**Notes**

- **Upload to a single board:** always pass `-e <env>`, for example `pio run -e tdisplay-s3-touch --target upload` (`-t` is short for `--target`). Without `-e`, PlatformIO builds and tries to upload **every** environment in `platformio.ini`.
- **Multiple serial ports:** if several ports are connected, choose one with `--upload-port /dev/cu.usbmodem1101` (on Windows, `COM5`), or set `upload_port` in the env.
- **T-Display S3 upload fails:** because the board runs as a USB keyboard, its serial port may not respond. Put it in download mode by holding **BOOT**, pressing and releasing **RST**, then releasing **BOOT**, and upload again. Press **RST** after flashing to start the firmware.
- **Build every board:** `pio run` with no `-e`. All required libraries download automatically.

### 4. Connect the keyboard

- **All supported boards:** plug the board into the computer you want to type into. It shows up as a USB keyboard; no pairing is needed.

### 5. First use

1. **Create a PIN:** on first boot, enter a 6-digit PIN twice. Unlocking takes a moment ("Verificando...") because of the key derivation.
2. **Turn on Wi-Fi:** tap **Config. → Ativar WiFi**. The screen shows the network `PassVault`, a **random password** and the URL (normally `http://192.168.4.1`).
3. **Add credentials:** connect your phone or laptop to that network, open the URL and add your credentials.
4. **Turn off Wi-Fi:** tap **Desligar WiFi**. The AP also turns itself off after 5 minutes without use.
5. **Type a credential:** put the cursor in a login field on the computer, select a credential on the device and tap **Tudo** (user + TAB + password), **Login**, **Senha** or **Entrar** (user + TAB + password + ENTER).

> The default UI language is Portuguese; select another language in **Config. → Idioma** or the web language selector. In Portuguese: *Tudo* = All, *Login* = Login, *Senha* = Password, *Voltar* = Back, *Bloquear* = Lock, *Desligar WiFi* = Turn off Wi-Fi.

> The host keyboard layout should be **US**, because HID sends US key codes. With another layout, symbols in passwords may come out wrong.

### 6. Updating the firmware

Flash again with the same `-t upload` command. **Your credentials and PIN are kept.** They live in the NVS partition, which a normal upload does not overwrite. Vaults from older firmware are converted to the new format on the first unlock (see [Upgrading from the old format](#security-model)).

Credentials can be lost in these cases:

- The partition table (`board_build.partitions`) changes.
- You run `-t erase`.
- You flash a different board environment that uses another layout.

Before such changes, **export a backup** from the web UI (see [Backup](#backup-export--import)) and import it again afterwards.

### 7. Resetting the vault

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

| Action | Capacitive touch (T-Display S3 Touch / Waveshare) | Buttons (T-Display S3) |
|---|---|---|
| Press a button | Tap | Move the focus with short presses, then **hold GPIO0** |
| Scroll the menu | Swipe up / down | Move the focus past the first or last visible item |
| Back | *Voltar* button | **Hold GPIO14** |

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

1. On the computer, **click the field** where the text should go. For *Tudo* or *Entrar*, click the username field.
2. On the device, select the credential in the menu to open its detail screen. It shows the name and username, with the password masked.
3. Choose what to type:

| Button | Types | Typical use |
|---|---|---|
| **Tudo** | username, **TAB**, password | Login forms where TAB moves to the password field |
| **Entrar** | username, **TAB**, password, **ENTER** | Fill and submit a login form in one action |
| **Login** | username only | Two-step logins (username first, then the next page) |
| **Senha** | password only | Password-only prompts, or the second step of a two-step login |
| **Voltar** | — | Back to the menu |

Use **Revelar** on the detail screen to display the password for **3 seconds**. It is then masked automatically; pressing the button again restarts the timer. Leaving the screen also hides the password. Long passwords wrap to fit the display.

**Entrar** appends Enter to submit the form. **Tudo**, **Login** and **Senha** do not append Enter; submit manually after using those options.


### Managing credentials (web UI)

1. In the menu, tap **Config. → Ativar WiFi**.
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
{"app":"PassVault","v":3,"iter":20000,"salt":"<hex>","ver":"<hex>","mac":"<hex>","creds":["<hex>", "..."]}
```

- The backup key is derived with **PBKDF2-HMAC-SHA256** (`iter` rounds) from the backup password and a new random 16-byte `salt` on each export.
- Each v3 `creds` entry is `category|favorite|name|user|pass` (`category`: `0` Sem tipo, `1` Redes Sociais, `2` Email, `3` Banco, `4` Trabalho; `favorite`: `0` or `1`), encrypted with **AES-256-CBC** and a random IV (IV + ciphertext, hex).
- Types and favorites are preserved in v3 backups. Legacy v1 (`name|user|pass`) and v2 (`favorite|name|user|pass`) backups remain importable and default to **Sem tipo**; v1 credentials also start without favorites. Firmware supporting v3 is required to restore new backups.
- `ver` checks the password, and `mac` (**HMAC-SHA256** over `3,`, the salt and all entries in v3; `2,` plus salt and entries in v2; salt and entries in v1) detects a damaged or modified file.

> Anyone who has the file can try to guess the backup password offline. Use a long, unique password and keep the file somewhere safe.

When you are done, tap **Desligar WiFi** on the device. If you forget, Wi-Fi turns off and the device locks after **5 minutes** without use.

> Only one device can be connected to the Wi-Fi at a time. If the connection is refused, disconnect the other phone or laptop first.

### Locking

| How | When |
|---|---|
| **Bloquear** button in the menu | Immediately |
| Auto-lock | After **60 s** by default without input in the menu, detail, search or settings screen; adjustable in **Config. → Auto-lock** |
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
| Config. / Bloquear | Settings / Lock |
| Configuracoes / Ativar WiFi | Settings / Turn on Wi-Fi |
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
          30 s lockout                         select item│      │Config.
                                                         ▼      ▼
                                                   ┌────────┐ ┌──────────┐
                                                   │ Detail │ │ Settings │
                                                   └────────┘ └──────────┘

Menu ──► Search (tap "Buscar...") ──► OK ──► Menu (filtered)
Settings ──► Auto-lock (cycle timeout) / Ativar WiFi ──► Wi-Fi AP
```

- **PIN screens:** a numpad with digits, `C` (clear) and `<` (backspace).
- **Menu:** a scrollable credential list (swipe on the touch board, or button navigation) plus *Config.* (Settings) and *Bloquear* (Lock) buttons. The top-right corner shows `USB`.
- **Settings:** press *Auto-lock* to cycle through 30 s, 60 s, 2 min, 5 min and 10 min. Each change takes effect immediately and is saved in NVS. *Ativar WiFi* opens the Wi-Fi screen; *Voltar* returns to the menu. Physical-button boards use the same focus navigation as other screens. The Wi-Fi screen keeps its separate 5-minute idle timeout.
- **Language:** open **Config. → Idioma**. Four native language names appear per page; use **< / >** to reach the other four. Tap a language to save it; **Voltar** returns to settings. Physical-button focus and auto-lock remain active. The language selector shares the last settings row with sorting. Changing the language in the web UI also refreshes the active Wi-Fi screen, without restarting the access point. Browser form values and stored credentials remain unchanged. This device preference is excluded from credential backups.
- **Detail:** shows the name and username with the password masked, plus the *Tudo / Login / Senha / Entrar / Voltar* buttons.
- **Typing feedback:** *Tudo*, *Login*, *Senha* and *Entrar* show **Digitando...** during USB transmission, followed by **Envio finalizado** for 0.9 s before returning to the detail screen. Credential contents stay hidden during this feedback.
- **Themes:** open **Config. → Temas** on the device and select **Cyber Green**, **Azul**, **Roxo** or **Ambar**. The palette changes immediately across the display, including the PIN screen. An asterisk marks the active theme, and the choice survives a restart. Touch and physical-button navigation are supported; auto-lock remains active in this tab.
- **Change unlock PIN:** open **Config. → Alterar PIN**, enter the current PIN, then enter and confirm a new 6-digit PIN. Legacy 4-digit PINs are accepted as the current PIN. On success the device locks; unlock with the new PIN. Credentials, favorites and settings are preserved. *Cancelar* (or holding Back on button boards) cancels without changing the PIN, and auto-lock remains active throughout the flow. An incorrect current PIN ends the unlocked session and counts toward the existing lockout/wipe limits.

PIN changes first re-encrypt the stored credentials with a fresh salt into the temporary NVS namespace `pvnext`. Only after every staging write succeeds is the `pinchange` marker committed. Startup replays committed changes before accepting PIN input; interrupted staging leaves the old PIN active. If NVS lacks room for the temporary copy, the change fails and the old PIN remains valid. If committed recovery cannot complete, the display asks for a restart and keeps the vault inaccessible until recovery succeeds.

Run `python3 test/test_touch_gesture.py` to exercise tap/hold behavior, release handling, vertical swipes and drag suppression for the Waveshare touch backend.

Run `python3 test/test_credential_typing.py` to check the keyboard sequences, disconnected-host behavior, usage counting and button bounds for all supported display layouts.

Run `python3 test/test_credential_sort.py` to check sorting, favorites, usage persistence, legacy records, deletion, stable IDs, types, grouping, write failures and backup v1/v2/v3 compatibility with simulated storage and mocked crypto.

Run `node test/test_web_categories.js` to check all eight web languages, live labels/placeholders, preservation of in-progress forms, language-save failure rollback, grouping, type editing, escaping and backup compatibility with simulated DOM and API responses.

Run `python3 test/test_localization.py` to check language persistence and API validation, all translated glyphs, malformed UTF-8 handling and language control bounds on every supported display size. The test also renders font samples to `/tmp/passvault-languages-<width>x<height>.pgm` for inspection. Run `python3 scripts/generate_localization.py --check` to verify the generated catalogs and font subset. Edit `locales/translations.json` and regenerate with `python3 scripts/generate_localization.py` when changing translations.

The device embeds a small Unicode bitmap font containing the characters used by the translations, including Simplified Chinese, Japanese and Cyrillic. It requires no filesystem upload, external font or internet connection. Unsupported characters in user-entered credentials render as `?` on the device without changing stored data. USB HID remains ASCII-only and the search keyboard remains A–Z/0–9. Font provenance and its separate SIL OFL 1.1 license are documented in [assets/fonts/README.md](assets/fonts/README.md). Physical display/touch/button validation is still required.

Run `python3 test/test_pin_change.py` to exercise the production transaction code on the host with simulated NVS write failures and power cuts. The test covers empty/full vaults, legacy PIN lengths, favorites/categories/language/settings preservation and repeated recovery interruptions. Cryptography is mocked in this host test; hardware power-loss and USB/touch behavior still require device validation.
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
- **Unlock speed:** `KDF_ITERS` sets how long an unlock takes. More iterations make offline guessing slower but also make unlocking slower. Tune it on your board.

### Code layout

```
platformio.ini              one [env:...] per board + shared base sections
include/
  config.h                  per-board settings (selected by BOARD_xxx) + shared constants
  input.h                   input abstraction: CST816 touch / buttons
  touch_gesture.h           tap/hold/swipe processing for polled touch
  ui_layout.h               shared drawing and hit-test geometry
  keyboard.h                native USB HID keyboard
  vault_core.h              crypto + NVS storage (hardware-independent)
  vault_web.h               Wi-Fi AP + web server + HTML single-page app (hardware-independent)
src/
  main.cpp                  UI, portrait layout and state machine
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
| GET | `/api/list` | List credentials with numeric `category` in the selected order (storage IDs preserved) |
| GET | `/api/language` | Get the selected language (`language`, `code`), native language names and the selected translation catalog |
| POST | `/api/language` | Save `language`: `0` Portuguese, `1` English, `2` Spanish, `3` Simplified Chinese, `4` Italian, `5` Japanese, `6` German, `7` Russian; return the selected catalog |
| GET | `/api/sort` | Get sorting preference (`mode`) |
| POST | `/api/sort` | Save sorting preference (`mode`: `0` alphabetical, `1` newest, `2` most used, `3` grouped by type) |
| GET | `/api/get` | Get one credential, including numeric `category` |
| POST | `/api/save` | Add or update a credential (`category`: `0` none, `1` social, `2` email, `3` bank, `4` work; omitted preserves the type on edit or defaults to none on add) |
| POST | `/api/del` | Delete a credential |
| POST | `/api/favorite` | Set favorite status (`id`, `favorite` = `0` or `1`) |
| POST | `/api/export` | Download an encrypted backup (`pw` = backup password) |
| POST | `/api/import` | Import a backup (`pw`, `v` = backup version (defaults to 1), `iter`, `salt`, `ver`, `mac`, `n`, `c0`…`cN`) |

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

### Waveshare ESP32-S3 touch boards under 2 inches

The new profiles are based on the manufacturer's examples and have been build-checked, but have not been validated on physical boards. All use native USB on GPIO19/20 and 16 MB flash / 8 MB OPI PSRAM. The 1.69″ and 1.83″ profiles use ST7789 over SPI and CST816-family touch over I2C. Buy the complete **ESP32-S3-Touch-LCD** board, not a standalone LCD module or a non-touch variant.

| Signal | 1.69″ | 1.83″ |
|---|---|---|
| LCD DC / CS / SCK / MOSI | 4 / 5 / 6 / 7 | 4 / 5 / 6 / 7 |
| LCD reset / backlight | 8 / 15 | 38 / 40 |
| Touch SDA / SCL | 11 / 10 | 15 / 14 |
| Touch reset / interrupt | 13 / 14 | 39 / 13 |
| Display size / row offset | 240×280 / 20 | 240×284 / 0 |
| Power | GPIO41 holds SYS_EN high | USB-powered bring-up; no additional AXP2101 battery-management UI |

Tap actions are emitted once when the finger is released. A vertical drag of at least 40 pixels scrolls; a horizontal drag does not activate the initial button. The compact layout keeps PIN entry, PIN cancellation, search, credential actions and all settings within the available height. USB19/20 are reserved for HID and are not assigned to the display or touch.

```bash
pio run -e waveshare-s3-touch-1_69
pio run -e waveshare-s3-touch-1_83
# Add --target upload to flash the selected board.
```

Use a data-capable USB cable. If the port is unavailable, hold BOOT while resetting, release BOOT, then upload again. Before considering any new profile hardware-validated, check colors/orientation, all screen corners, one action per held touch, scrolling, PIN setup/unlock/change, brightness, web management, credential persistence and USB typing (including Tab and Enter). Battery operation also requires a physical check.

Hardware references:

- [Waveshare 1.69″ documentation](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.69) and [manufacturer pin configuration](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.69/blob/50c85c2616d4e42c56f11ce154e6ce6f3d2dceb0/examples/arduino/libraries/board_config/pin_config.h).
- [Waveshare 1.83″ documentation](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.83) and [manufacturer pin configuration](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.83/blob/a790c727912d8f2f5f7cf56fd8cca8c892a36c76/examples/arduino/libraries/Mylibrary/pin_config.h). The display offsets follow each manufacturer's HelloWorld example.

### Additional compact touch boards

| Signal | Waveshare 1.54″ | Waveshare 1.47″ | Waveshare AMOLED 1.64″ V1.1 | LilyGO AMOLED Touch 1.91″ |
|---|---|---|---|---|
| Display controller | ST7789 | JD9853 (manufacturer register sequence) | CO5300 | RM67162 |
| Display CS / SCK | 21 / 38 | 21 / 38 | 46 / 10 | 6 / 47 |
| SPI DC / MOSI | 45 / 39 | 45 / 39 | — | — |
| QSPI D0 / D1 / D2 / D3 | — | — | 11 / 12 / 13 / 14 | 18 / 7 / 48 / 5 |
| LCD reset / backlight | 40 / 46 | 40 / 46 | 21 / command 0x51 | 17 / command 0x51 |
| Touch SDA / SCL | 42 / 41 | 42 / 41 | 47 / 48 | 3 / 2 |
| Touch reset / interrupt | 47 / 48 | 47 / 48 | board reset / 18 | board reset / 21 |
| Display offset | 0 / 0 | column 34 | column 20 | 0 / 0 |

- Select the **touch version** of the Waveshare 1.54″ and LilyGO 1.91″. The LilyGO profile targets the QSPI model, not AMOLED Plus (SPI); GPIO38 enables power. Its touch coordinates are transformed from landscape to native portrait.
- The Waveshare 1.64″ profile targets **V1.1 only**, with display CS on GPIO46. Do not flash it onto V1, whose CS is GPIO9.
- JD9853 uses the manufacturer's full register sequence, including its pixel format and inversion settings. It is not initialized as an ordinary ST7789 panel.
- AMOLED brightness and screen fades use the display brightness command, without a backlight GPIO. TFT_eSPI renders the existing fonts into PSRAM; Arduino_GFX sends completed frames over SPI/QSPI. Allocation/initialization failure stops startup.
- The 240×240 layout uses three columns for credential actions and nine columns for the search keyboard. Drawing and touch hit tests share the same geometry.
- No hardware validation is claimed for these profiles. Validate display colors/orientation, touch corners/swipes, brightness, PIN changes, web management and USB typing on each physical board. Battery charging/status, audio, SD, RTC and IMU features are not implemented.

Manufacturer sources:

- [Waveshare 1.54″ resources](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.54/Resources-And-Documents) and [Arduino touch example](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.54/blob/5157db7c888e476fd57f8a95020800377447478f/examples/ESP32-S3-Touch-LCD-1.54-demo/Arduino-3.2.0/examples/08_lvgl_arduino_v8/08_lvgl_arduino_v8.ino).
- [Waveshare 1.47″ resources and original Arduino demo](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.47/Resources-And-Documents).
- [Waveshare AMOLED 1.64″ V1.1 example](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.64-v2/tree/21b952243d306692774f3b5bfeb3eb5cff10f851/Arduino/examples/06_LVGL_Test).
- [LilyGO AMOLED pin definitions](https://github.com/Xinyuan-LilyGO/LilyGo-AMOLED-Series/blob/e17360cecd7ed9085463b981581172e53dfa1740/src/LilyGo_AMOLED.h) and [touch orientation](https://github.com/Xinyuan-LilyGO/LilyGo-AMOLED-Series/blob/e17360cecd7ed9085463b981581172e53dfa1740/src/LilyGo_AMOLED.cpp).

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
| `AUTOLOCK_MS` | 60000 | Default inactivity timeout; the device settings override it with a value from `AUTOLOCK_OPTIONS_MS` |
| `C_*` | — | UI colours (RGB565) |

Settings for each board (inside the `#if defined(BOARD_xxx)` blocks): `SCR_W`, `SCR_H`, `TFT_ROTATION`, `PIN_POWER_ON`, the touch or button pins, `TOUCH_DEBOUNCE`, `KBD_USB_HID`, and `MENU_SCROLL_BTNS`, which shows ▲/▼ menu buttons on boards without swipe gestures.

Direct-rendering display driver and pin settings for TFT_eSPI are passed as `build_flags` in `platformio.ini` (`USER_SETUP_LOADED=1`), so the TFT_eSPI library's own `User_Setup.h` is never edited.

---

## Adding a new board

1. **Add an environment:** in `platformio.ini`, add an `[env:my-board]` that `extends` the closest base section (`tdisplay_s3_base`). Set the TFT_eSPI driver and pin flags and add `-DBOARD_MY_BOARD=1`. Include only the libraries this board needs in `lib_deps`.
2. **Add the board settings:** in `include/config.h`, add an `#elif defined(BOARD_MY_BOARD)` block with the screen size, rotation, the input pins, `TOUCH_DEBOUNCE`, and `KBD_USB_HID`. Only boards with native USB HID are supported.
3. **Add input support if needed:** if the board's input hardware is new, add a branch to `include/input.h` that implements `inputBegin()` and `inputRead()`.
4. **Add display support if needed:** add the transport/controller in `include/display.h` for a new SPI/QSPI controller. JD9853 and AMOLED profiles use `DISPLAY_GFX` and require working OPI PSRAM.
5. **Build it** with `pio run -e my-board`.

The geometry in `ui_layout.h` supports 170×320, 172×320, 240×240, 240×280, 240×284, 280×456 and 240×536. Other resolutions may need changes to the `PIN_*`, `NP_*` and `WEB_*` layout constants in `ui_layout.h`.

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
3. Confirm that it boots, the PIN works, credentials persist and USB typing still works.
4. Only then add **secure boot** and, when you are fully satisfied, switch to **release** mode.

See Espressif's documentation for the exact steps for the ESP32-S3: *Flash Encryption*, *Secure Boot v2* and *NVS Encryption* in the ESP-IDF Programming Guide.

---

## Build notes

- **Arduino-ESP32 core:** this project targets core **3.x**, built with core 3.3.12.
- **Library finder:** `lib_ldf_mode = deep` is required. With the default mode, the core's own libraries fail to find their internal dependencies, for example `USB → FS.h` and `WiFi → Network.h`.
- **Library warnings:** `DISABLE_ALL_LIBRARY_WARNINGS` silences TFT_eSPI's informational warnings, and `-Wno-attributes` silences a warning inside the CST816S library. Every environment builds with no warnings.

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| `FS.h` / `Network.h: No such file or directory` | Make sure `lib_ldf_mode = deep` is set |
| Upload fails on T-Display S3 | Enter download mode: hold BOOT, press RST, release BOOT |
| Nothing typed on USB boards | Use the USB-C data port and make sure the host recognised a keyboard |
| Wrong characters typed | Set the host keyboard layout to **US**. HID sends US key codes |
| Forgot the PIN | Enter 10 wrong PINs, or erase the flash with `pio run -e <env> -t erase` and upload again. **All credentials are lost** |
| Unlock takes too long | Lower `KDF_ITERS` (this makes offline guessing faster) |
