#pragma once

#include "unicode_font.h"

// Bounded UTF-8 decoding. Invalid input consumes at least one byte and uses '?'.
inline uint32_t nextUiCodepoint(const char*& text) {
  uint8_t first = static_cast<uint8_t>(*text++);
  if (first < 0x80) return first;
  int count = first >= 0xC2 && first <= 0xDF ? 1 :
              first >= 0xE0 && first <= 0xEF ? 2 :
              first >= 0xF0 && first <= 0xF4 ? 3 : 0;
  if (!count) return '?';
  uint32_t code = first & (0x7F >> (count + 1));
  for (int i = 0; i < count; i++) {
    uint8_t byte = static_cast<uint8_t>(*text);
    if ((byte & 0xC0) != 0x80) return '?';
    text++;
    code = (code << 6) | (byte & 0x3F);
  }
  if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) ||
      (count == 3 && code < 0x10000) || code > 0x10FFFF ||
      (code >= 0xD800 && code <= 0xDFFF)) return '?';
  return code;
}

inline const UiGlyph& uiGlyph(uint32_t code) {
  unsigned lo = 0, hi = UI_GLYPH_COUNT;
  while (lo < hi) {
    unsigned mid = lo + (hi - lo) / 2;
    if (UI_GLYPHS[mid].codepoint < code) lo = mid + 1;
    else hi = mid;
  }
  if (lo < UI_GLYPH_COUNT && UI_GLYPHS[lo].codepoint == code) return UI_GLYPHS[lo];
  return uiGlyph('?');
}

inline bool uiHasUnicode(const char* text) {
  while (*text) if (static_cast<uint8_t>(*text++) >= 0x80) return true;
  return false;
}

inline int uiFontHeight(uint8_t font) { return font == 4 ? 24 : font == 1 ? 8 : 16; }

inline int uiTextWidth(const char* text, uint8_t font) {
  int width = 0;
  while (*text) width += uiGlyph(nextUiCodepoint(text)).width * uiFontHeight(font) / 16;
  return width;
}

// Both the direct TFT and the PSRAM sprite expose the same pixel API and datum.
// Only translated Unicode/overlong text uses this renderer; native ASCII fonts remain.
template <typename Surface>
int drawUiBitmap(Surface& surface, const char* text, int x, int y, uint8_t font, int maxWidth) {
  int natural = uiTextWidth(text, font), height = uiFontHeight(font);
  if (!natural || maxWidth <= 0) return 0;
  int width = natural < maxWidth ? natural : maxWidth;
  int col = surface.textdatum % 3, row = surface.textdatum / 3;
  x -= col == 1 ? width / 2 : col == 2 ? width : 0;
  y -= row == 1 ? height / 2 : row == 2 ? height : 0;
  if (surface.textbgcolor != surface.textcolor)
    surface.fillRect(x, y, width, height, surface.textbgcolor);
  int advance = 0;
  while (*text) {
    const UiGlyph& glyph = uiGlyph(nextUiCodepoint(text));
    int glyphWidth = glyph.width * height / 16;
    int start = advance * width / natural;
    int end = (advance + glyphWidth) * width / natural;
    for (int dy = 0; dy < height; dy++) {
      int sy = dy * 16 / height;
      for (int dx = start; dx < end; dx++) {
        int sx = (dx - start) * glyph.width / (end - start);
        int offset = sy * (glyph.width / 8) + sx / 8;
        if (glyph.bits[offset] & (0x80 >> (sx % 8)))
          surface.drawPixel(x + dx, y + dy, surface.textcolor);
      }
    }
    advance += glyphWidth;
  }
  return width;
}

template <typename Surface>
int drawUiString(Surface& surface, const String& text, int x, int y, uint8_t font, int maxWidth) {
  if (uiHasUnicode(text.c_str()) || surface.textWidth(text, font) > maxWidth)
    return drawUiBitmap(surface, text.c_str(), x, y, font, maxWidth);
  return surface.drawString(text, x, y, font);
}

template <typename Surface>
int uiAvailableWidth(const Surface& surface, int x) {
  int col = surface.textdatum % 3;
  return col == 1 ? 2 * (x < SCR_W - x ? x : SCR_W - x) - 8 :
         col == 2 ? x - 4 : SCR_W - x - 4;
}
