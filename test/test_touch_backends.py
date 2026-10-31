"""Exercise production I2C touch decoding and gesture generation with recorded-format packets."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'include/input.h').read_text()
backend = source[source.index('#include <Wire.h>'):source.index('#elif defined(BOARD_TDISPLAY_S3)')]
backend = backend.replace('#include <Wire.h>', '').rsplit('// ---------------------------------------------------------------------', 1)[0]
support = r'''
#include <cassert>
#include <cstdint>
#include <vector>
#include <algorithm>
#define OUTPUT 1
#define HIGH 1
#define LOW 0
#define TOUCH_SDA 3
#define TOUCH_SCL 2
#define TOUCH_RST TEST_RESET
unsigned long now = 0;
unsigned long millis() { return now; }
void delay(int) {}
void pinMode(int p, int) { assert(p >= 0); }
void digitalWrite(int p, int) { assert(p >= 0); }
struct MockWire {
  std::vector<uint8_t> packet, writes;
  int address = -1, offset = 0, error = 0, requested = 0;
  void begin(int, int) {}
  void setTimeOut(int) {}
  void beginTransmission(int a) { address=a; writes.clear(); }
  void write(int b) { writes.push_back(b); }
  int endTransmission(bool = true) { return error; }
  int requestFrom(uint8_t a, uint8_t count) {
    assert(a==address); requested=count; offset=0; return packet.size();
  }
  int read() { assert(offset < int(packet.size())); return packet[offset++]; }
} Wire;
'''
checks = r'''
bool sample(bool down, int sx, int sy, int &x, int &y, int &gesture) {
  int px=sx, py=sy;
#ifdef BOARD_TDISPLAY_S3_AMOLED_TOUCH
  px=sy; py=SCR_W-1-sx;
#endif
#ifdef INPUT_AXS5106_POLL
  px=SCR_W-1-sx;
  Wire.packet={0,uint8_t(down),uint8_t(px>>8),uint8_t(px),uint8_t(py>>8),uint8_t(py)};
#else
  Wire.packet={uint8_t(down),uint8_t(px>>8),uint8_t(px),uint8_t(py>>8),uint8_t(py)};
#endif
  now+=10;
  bool result=inputRead(x,y,gesture);
#ifdef INPUT_AXS5106_POLL
  assert(Wire.address==0x63 && Wire.writes[0]==1 && Wire.requested==6);
#elif defined(INPUT_FT3168_POLL)
  assert(Wire.address==0x38 && Wire.writes[0]==2 && Wire.requested==5);
#else
  assert(Wire.address==0x15 && Wire.writes[0]==2 && Wire.requested==5);
#endif
  return result;
}
int main() {
  inputBegin();
  int x=-1,y=-1,g=-1;
  // Corners and middle map into the same portrait coordinate system as drawing.
  for (auto p : std::vector<std::pair<int,int>>{{0,0},{SCR_W-1,SCR_H-1},{70,110}}) {
    assert(!sample(true,p.first,p.second,x,y,g));
    for(int i=0;i<10;i++) assert(!sample(true,p.first,p.second,x,y,g));
    assert(sample(false,p.first,p.second,x,y,g));
    assert(x==p.first && y==p.second && g==0);
    assert(!sample(false,p.first,p.second,x,y,g));
  }
  assert(!sample(true,60,150,x,y,g));
  assert(!sample(true,60,70,x,y,g));
  assert(sample(false,60,70,x,y,g) && g==2); // Swipe, no tap.
  // Failed and truncated reads must not manufacture a release/tap.
  assert(!sample(true,60,100,x,y,g));
  Wire.error=1;
  assert(!sample(false,60,100,x,y,g));
  Wire.error=0;
  Wire.packet={0};now+=10;assert(!inputRead(x,y,g));
  assert(sample(false,60,100,x,y,g) && x==60 && y==100 && g==0);
}
'''
compiler = shutil.which('clang++') or shutil.which('g++')
if not compiler:
    raise SystemExit('A C++17 compiler is required')
variants = [
    ['INPUT_CST816_POLL','SCR_W=240','SCR_H=240','TEST_RESET=47'],
    ['INPUT_AXS5106_POLL','SCR_W=172','SCR_H=320','TEST_RESET=47'],
    ['INPUT_FT3168_POLL','SCR_W=280','SCR_H=456','TEST_RESET=-1'],
    ['INPUT_CST816_POLL','BOARD_TDISPLAY_S3_AMOLED_TOUCH','SCR_W=240','SCR_H=536','TEST_RESET=-1'],
]
with tempfile.TemporaryDirectory(prefix='passvault-touch-') as tmp:
    cpp=Path(tmp)/'test.cpp'
    binary=Path(tmp)/'test'
    cpp.write_text(support+backend+checks)
    for flags in variants:
        subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror',*['-D'+f for f in flags],'-I',str(root/'include'),str(cpp),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
print('PASS: CST816, AXS5106L, FT3168, portrait transforms, held touches and failed I2C reads')
