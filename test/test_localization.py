"""Host checks for actual language persistence/API, Unicode drawing and paged controls."""
from pathlib import Path
import importlib.util
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('localization_generator', root / 'scripts/generate_localization.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
catalog = generator.generate(check=True)
core = (root / 'include/vault_core.h').read_text()
web = (root / 'include/vault_web.h').read_text()
main = (root / 'src/main.cpp').read_text()
setter = core[core.index('bool setLanguage('):core.index('// ---- Crypto helpers ----')]
loader = core[core.index('void loadLanguagePreference('):core.index('bool storageBegin(')]
api = web[web.index('void handleLanguageGet('):web.index('void handleSortGet(')]
controls = main[main.index('constexpr int LANGUAGE_PAGE_SIZE'):main.index('void handleSettingsTouch(')]
support = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <iostream>
using String = std::string;
#include "localization.h"
#include "unicode_text.h"
#include "ui_layout.h"
struct Prefs {
  std::map<std::string,uint8_t> disk;
  bool fail=false;
  size_t putUChar(const char* k, uint8_t v) { if(fail)return 0;disk[k]=v;return 1; }
  uint8_t getUChar(const char* k, uint8_t d=0) { return disk.count(k)?disk[k]:d; }
} prefs;
struct Server {
  std::map<std::string,String> args;
  int status=0;
  String body;
  String arg(const char* key) { return args[key]; }
  void send(int code,const char*,const String& value) { status=code;body=value; }
} server;
void webTouch() {}
constexpr int C_BG=0,C_ACCENT=1,C_WARN=2,C_BTN=3,TC_DATUM=1;
constexpr int ST_LANGUAGES=1,ST_SETTINGS=2;
int state=ST_LANGUAGES;
void beginScreen() {}
struct Rect { int x,y,w,h;String label; };
std::vector<Rect> buttons;
void focusBegin(int) { buttons.clear(); }
void focusEnd() {}
void drawSettings() { buttons.clear(); }
void btn(int x,int y,int w,int h,const char* label,int) { buttons.push_back({x,y,w,h,label}); }
bool inBox(int tx,int ty,int x,int y,int w,int h) { return tx>=x && tx<x+w && ty>=y && ty<y+h; }
struct Surface {
  uint32_t textcolor=1,textbgcolor=0;
  uint8_t textdatum=0;
  std::vector<uint8_t> pixels=std::vector<uint8_t>(SCR_W*SCR_H,0);
  void fillScreen(int) { std::fill(pixels.begin(),pixels.end(),0); }
  void setTextColor(int color) { textcolor=color; }
  void setTextDatum(int datum) { textdatum=datum; }
  void drawPixel(int x,int y,int color) {
    assert(x>=0 && x<SCR_W && y>=0 && y<SCR_H);
    pixels[y*SCR_W+x]=color?255:0;
  }
  void fillRect(int x,int y,int w,int h,int color) {
    for(int row=y;row<y+h;row++) for(int col=x;col<x+w;col++) drawPixel(col,row,color);
  }
  int textWidth(const String& text,int font) { return uiTextWidth(text.c_str(),font); }
  int drawString(const String& text,int x,int y,int font) {
    return drawUiBitmap(*this,text.c_str(),x,y,font,SCR_W-8);
  }
} tft;
'''
checks = r'''
int main(int argc,char** argv) {
  loadLanguagePreference();assert(activeLanguage==0);
  assert(!setLanguage(8) && activeLanguage==0);
  for(int language=0;language<LANGUAGE_COUNT;language++) {
    assert(setLanguage(language));activeLanguage=99;loadLanguagePreference();
    assert(activeLanguage==language);
    server.args["language"]=std::to_string(language);handleLanguageSave();
    assert(server.status==200 && server.body==LANGUAGE_WEB_JSON[language]);
    handleLanguageGet();assert(server.body==LANGUAGE_WEB_JSON[language]);
    languagePage=language/LANGUAGE_PAGE_SIZE;state=ST_LANGUAGES;
    drawLanguages();assert(buttons.size()==7);
    for(size_t i=0;i<buttons.size();i++) {
      const auto& b=buttons[i];
      assert(b.x>=0 && b.y>=0 && b.x+b.w<=SCR_W && b.y+b.h<=SCR_H);
      for(size_t j=0;j<i;j++) {
        const auto& a=buttons[j];
        assert(b.x+b.w<=a.x || a.x+a.w<=b.x || b.y+b.h<=a.y || a.y+a.h<=b.y);
      }
    }
    handleLanguagesTouch(SETTINGS_X+1,LANGUAGE_Y+(language%4)*LANGUAGE_STEP+1);
    assert(activeLanguage==language);
    int page=languagePage;
    handleLanguagesTouch(SETTINGS_LANGUAGE_X+1,LANGUAGE_NAV_Y+1);
    assert(languagePage==1-page);
    for(int id=0;id<TXT_COUNT;id++) {
      const char* text=tr(static_cast<TextId>(id));
      while(*text) { uint32_t code=nextUiCodepoint(text);assert(uiGlyph(code).codepoint==code); }
      tft.textdatum=0;
      drawUiBitmap(tft,tr(static_cast<TextId>(id)),4,4,2,SCR_W-8);
    }
  }
  assert(setLanguage(3));prefs.fail=true;
  assert(!setLanguage(7) && activeLanguage==3);
  server.args["language"]="7";handleLanguageSave();assert(server.status==500 && activeLanguage==3);
  prefs.fail=false;
  for(const char* invalid:{"", "-1", "8", "255", "1x", " 1"}) {
    server.args["language"]=invalid;handleLanguageSave();
    assert(server.status==400 && activeLanguage==3);
  }
  prefs.disk["language"]=255;loadLanguagePreference();assert(activeLanguage==0);
  prefs.disk.erase("language");loadLanguagePreference();assert(activeLanguage==0);
  // Invalid UTF-8 must make progress, never read beyond the terminator.
  for(const char* malformed:{"\xE4", "\xF0\x80", "\xFF", "\xC0\x80", "\xED\xA0\x80"}) {
    const char* p=malformed;
    while(*p) { const char* before=p;nextUiCodepoint(p);assert(p>before); }
  }
  assert(uiGlyph(0x10FFFF).codepoint=='?');
  // Render an atlas with the exact firmware bitmap renderer for visual QA.
  if(argc>1) {
    tft.fillScreen(0);tft.textcolor=1;tft.textdatum=0;
    const TextId samples[]={TXT_LANGUAGE,TXT_ENTER_PIN,TXT_PASSWORD,TXT_SETTINGS};
    for(int language=0;language<LANGUAGE_COUNT;language++) {
      activeLanguage=language;
      String sample=String(LANGUAGE_NAMES[language])+": "+tr(samples[language%4]);
      drawUiBitmap(tft,sample.c_str(),4,4+language*27,2,SCR_W-8);
    }
    std::ofstream out(argv[1],std::ios::binary);
    out<<"P5\n"<<SCR_W<<" "<<SCR_H<<"\n255\n";
    out.write(reinterpret_cast<const char*>(tft.pixels.data()),tft.pixels.size());
  }
  std::cout<<"PASS: eight locales, persistence, invalid settings, API failures, glyph coverage, UTF-8 and paged controls "<<SCR_W<<"x"<<SCR_H<<"\n";
}
'''
with tempfile.TemporaryDirectory(prefix='passvault-languages-') as temp:
    compiler = shutil.which('clang++') or shutil.which('g++')
    assert compiler, 'C++17 compiler required'
    for width, height in [(170,320),(240,280),(240,284),(240,240),(172,320),(280,456),(240,536)]:
        cpp, binary = Path(temp)/'test.cpp', Path(temp)/'test'
        cpp.write_text(support + setter + loader + api + controls + checks)
        subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        f'-DSCR_W={width}', f'-DSCR_H={height}', '-I', str(root/'include'),
                        str(cpp), '-o', str(binary)], check=True)
        subprocess.run([str(binary), f'/tmp/passvault-languages-{width}x{height}.pgm'], check=True)
