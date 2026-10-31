"""Check production keyboard sequences and detail button bounds on every supported portrait layout."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/main.cpp').read_text()
typing = source[source.index('void typeCredential(int mode) {'):source.index('// Botoes do detalhe:')]
layout = source[source.index('void detailBtnPos'):source.index('const TextId detLabels')]
support = r'''
#include <cassert>
#include <algorithm>
using std::min;
#include <string>
#include <vector>
#include <iostream>
struct Credential { const char* user; const char* pass; } creds[]={{"login","password"}};
int selCred=0, uses=0;
unsigned long typingFeedbackShownAt=0,lastAction=0;
bool typingFeedbackVisible=false, connected=true;
constexpr int C_WARN=1,C_ACCENT=2,C_OK=3,KEY_TAB=9,KEY_RETURN=13;
std::vector<std::string> events;
bool kbdConnected() { return connected; }
void kbdPrint(const char* s) { events.push_back(s); }
void kbdWrite(int k) { events.push_back(k==KEY_TAB?"TAB":"ENTER"); }
void drawTypingFeedback(const char*, int) {}
void recordCredentialUse(int) { ++uses; }
unsigned long millis() { return 42; }
'''
checks = r'''
int main() {
  const std::vector<std::vector<std::string>> expected={
    {"login","TAB","password"},{"login"},{"password"},{"login","TAB","password","ENTER"}
  };
  for(int mode=0;mode<4;mode++) {
    events.clear();uses=0;connected=true;typingFeedbackVisible=false;
    typeCredential(mode);
    assert(events==expected[mode] && uses==1 && typingFeedbackVisible);
    events.clear();uses=0;connected=false;
    typeCredential(mode);assert(events.empty() && uses==0);
  }
  // Actual production geometry: controls must fit and not overlap.
  assert(PIN_BOX_Y+PIN_BOX_H<=NP_Y0);
  assert(NP_Y0+NP_ROWS*NP_BH+(NP_ROWS-1)*NP_GAP<=PIN_CANCEL_Y);
  assert(PIN_CANCEL_Y+PIN_CANCEL_H<=SCR_H-2);
  assert(NP_COLS*NP_BW+(NP_COLS-1)*NP_GAP<=SCR_W-4);
  assert(KB_Y0+((KB_KEYS+KB_COLS-1)/KB_COLS)*KB_KH+((KB_KEYS+KB_COLS-1)/KB_COLS-1)*KB_GAP<KB_ACT_Y);
  assert(KB_ACT_Y+KB_ACT_H<=SCR_H-2);
  assert(SETTINGS_LOCK_Y+SETTINGS_H+6+16<=SETTINGS_WIFI_Y);
  int settings[]={SETTINGS_WIFI_Y, SETTINGS_THEME_Y, SETTINGS_PIN_Y,
                  SETTINGS_BRIGHT_Y, SETTINGS_SORT_Y, SETTINGS_BACK_Y};
  for(int i=0;i<6;i++) {
    assert(settings[i]+SETTINGS_H<=SCR_H-2);
    if(i) assert(settings[i-1]+SETTINGS_H<settings[i]);
  }
  assert(WEB_BTN_Y+42<=SCR_H-2);
  int xs[DET_BTN_COUNT],ys[DET_BTN_COUNT];
  for(int i=0;i<DET_BTN_COUNT;i++) {
    detailBtnPos(i,xs[i],ys[i]);
    assert(xs[i]>=2 && ys[i]>=DET_PASS_Y+24);
    assert(xs[i]+DET_BTN_W+2<=SCR_W && ys[i]+DET_BTN_H+2<=SCR_H);
    for(int j=0;j<i;j++)
      assert(xs[i]+DET_BTN_W<=xs[j] || xs[j]+DET_BTN_W<=xs[i] ||
             ys[i]+DET_BTN_H<=ys[j] || ys[j]+DET_BTN_H<=ys[i]);
  }
  std::cout << "PASS: keyboard sequences, disconnected keyboard, one use per send and detail layout "
            << SCR_W << "x" << SCR_H << "\n";
}
'''
with tempfile.TemporaryDirectory(prefix='passvault-typing-') as temp:
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('A C++17 compiler is required')
    for width, height in [(170, 320), (240, 280), (240, 284), (240, 240), (172, 320), (280, 456), (240, 536)]:
        cpp = Path(temp) / 'test.cpp'
        binary = Path(temp) / 'test'
        defines = f'#include "localization.h"\n#define SCR_W {width}\n#define SCR_H {height}\n#include "ui_layout.h"\n'
        cpp.write_text(support + defines + typing + layout + checks)
        subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-I', str(root / 'include'), str(cpp), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
