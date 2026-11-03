"""Compile production sync UI for all panel sizes; check focus targets and input."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
main = (root / 'src/main.cpp').read_text()
ui = main[main.index('// ---- Device-to-device sync'):main.index('// ---- Web Screen ----')]
settings = main[main.index('void drawSettings() {'):main.index('#define BRIGHT_BTN_Y')]
protocol = (root / 'include/vault_sync.h').read_text()
enums = protocol[protocol.index('enum SyncPhase'):protocol.index('SyncPhase syncPhase')]
support = r'''
#include <cassert>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
using std::min;
constexpr int C_TEXT=0,C_BG=1,C_ACCENT=2,C_BTN=3,C_DIM=4,C_WARN=5,TC_DATUM=0,ST_SYNC=2,ST_SETTINGS=3;
unsigned long autoLockMs=60000;
enum {TXT_SETTINGS,TXT_AUTOLOCK_SECONDS,TXT_AUTOLOCK_MINUTES,TXT_TAP_CHANGE,TXT_THEMES,TXT_CHANGE_PIN,TXT_BRIGHTNESS,TXT_SORT,TXT_LANGUAGE,TXT_BACK};
const char* tr(int) {return "";}
#include "ui_layout.h"
struct Rect {int x,y,w,h;};
std::vector<Rect> targets;
struct Display {
  void fillScreen(int) {}
  void setTextColor(int,int) {}
  void setTextColor(int) {}
  void drawString(const char*,int,int,int) {}
  void setTextDatum(int) {}
  void drawFitString(const char*,int,int y,int,int) {assert(y>=0 && y+16<=SCR_H);}
} tft;
void beginScreen() {}
void focusBegin(int) {targets.clear();}
void focusEnd() {}
void btn(int x,int y,int w,int h,const char*,int) {
  assert(x>=0 && y>=0 && w>0 && h>0 && x+w<=SCR_W && y+h<=SCR_H);
  for(auto a:targets)assert(x+w<=a.x || a.x+a.w<=x || y+h<=a.y || a.y+a.h<=y);
  targets.push_back({x,y,w,h});
}
bool inBox(int tx,int ty,int x,int y,int w,int h) {return tx>=x && tx<x+w && ty>=y && ty<y+h;}
char syncCode[13]="",syncSsid[33]="PassVault-Sync-ABC123",syncNetworks[6][33]={};
uint8_t syncChallenge[16]={};
size_t syncOffset=0;
int syncNetworkIndex=0,syncNetworkCount=2,syncAdded=0,syncSkipped=0;
const char* syncMessage="Recebido";
const char* npLabels[12]={"1","2","3","4","5","6","7","8","9","C","0","<"};
int stopCalls=0,connectCalls=0,confirmCalls=0;
void handleBack() {stopCalls++;}
'''
mocks = r'''
SyncPhase syncPhase=SYNC_CHOOSE;
void syncStop() {syncPhase=SYNC_OFF;}
void syncStartSender() {syncPhase=SYNC_AP_WAIT;}
void syncStartScan() {syncPhase=SYNC_SCANNING;}
void syncConnect() {if(strlen(syncCode)==12){connectCalls++;syncPhase=SYNC_CONNECTING;}}
void syncConfirmSend() {if(syncPhase==SYNC_WAIT_CONFIRM && syncOffset==16){confirmCalls++;syncPhase=SYNC_SENDING;}}
'''
checks = r'''
int main() {
  drawSettings();assert(targets.size()==9);
  for(int phase=SYNC_CHOOSE;phase<=SYNC_FAILED;phase++) {
    syncPhase=SyncPhase(phase);syncOffset=16;drawSync();assert(!targets.empty());
  }
  syncPhase=SYNC_CHOOSE;drawSync();handleSyncTouch(11,SYNC_SECOND_Y+1);
  assert(syncPhase==SYNC_SCANNING);
  syncPhase=SYNC_SELECT;drawSync();handleSyncTouch(11,SYNC_ACTION_Y+1);
  assert(syncPhase==SYNC_CODE && targets.size()==14);
  // Twelve digits alone do not start receiving. Explicit confirmation is required.
  for(int i=0;i<12;i++)handleSyncTouch(11,SYNC_KEY_Y+1);
  assert(strlen(syncCode)==12 && connectCalls==0);
  handleSyncTouch(11,SYNC_KEY_Y+1);assert(strlen(syncCode)==12);
  handleSyncTouch(14+(SCR_W-24)/2+1,SYNC_BACK_Y+1);
  assert(connectCalls==1 && syncPhase==SYNC_CONNECTING);
  syncPhase=SYNC_WAIT_CONFIRM;syncOffset=16;drawSync();
  handleSyncTouch(11,SYNC_ACTION_Y+1);assert(confirmCalls==1);
  syncPhase=SYNC_CODE;drawSync();handleSyncTouch(11,SYNC_BACK_Y+1);assert(stopCalls==1);
  syncPhase=SYNC_FINISHED;handleSyncTouch(11,SYNC_SECOND_Y+1);
  assert(syncPhase==SYNC_CHOOSE && syncAdded==0 && syncSkipped==0);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / 'ui.cpp'
    src.write_text(support + enums + mocks + settings + ui + checks)
    for w, h in [(170,320),(240,280),(240,284),(240,240),(172,320),(280,456),(240,536)]:
        exe = Path(tmp) / f'ui-{w}-{h}'
        subprocess.run(['c++','-std=c++17','-I',str(root / 'include'),f'-DSCR_W={w}',f'-DSCR_H={h}',str(src),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
        print(f'PASS: sync controls, bounds, overlap, code entry and confirmation {w}x{h}')
