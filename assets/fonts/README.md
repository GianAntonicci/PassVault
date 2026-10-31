# PassVault UI font

`passvault.hex` and the generated `include/unicode_font.h` contain a subset
of **GNU Unifont 18.0.01**, maintained by Paul Hardy, originally created by
Roman Czyborra, with glyphs by the contributors credited in
[UPSTREAM-README.txt](UPSTREAM-README.txt). Copyright remains with those
contributors; this font subset is distributed under the **SIL Open Font
License 1.1**, independently of PassVault's MIT software license.
See [OFL-1.1.txt](OFL-1.1.txt) and the original [COPYING.unifont](COPYING.unifont).
The subset is named **PassVault UI**; the glyph designs are unmodified.

Upstream: https://unifoundry.com/unifont/index.html

Source glyphs: https://unifoundry.com/pub/unifont/unifont-18.0.01/font-builds/unifont-18.0.01.hex.gz

Only ASCII, native language names and characters present in the translations
are included. Unsupported user-entered Unicode characters display as `?` on
the device; credential data itself is preserved. This does not extend the
ASCII-only USB HID keyboard or the A–Z/0–9 search keyboard.

Run `python3 scripts/generate_localization.py` to regenerate catalogs and the
font from the bundled subset. To add characters, download the full upstream
`.hex.gz` and pass its local path using `--font-source`. The generator checks
all translations for missing glyphs and consistent formatting placeholders.
No downloads or font installation are needed to build the firmware.
