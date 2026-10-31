"""Exercise the production CST816 polling gesture state machine on the host."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
checks = r'''
#include <cassert>
#include <iostream>
#include "touch_gesture.h"
int main() {
  TouchGesture t;
  int x=-1,y=-1,g=-1;
  assert(!t.sample(false,0,0,x,y,g));
  for(int i=0;i<200;i++) assert(!t.sample(true,30,40,x,y,g));
  assert(t.sample(false,0,0,x,y,g) && x==30 && y==40 && g==0);
  assert(!t.sample(false,0,0,x,y,g));
  assert(!t.sample(true,20,150,x,y,g));
  assert(!t.sample(true,21,70,x,y,g));
  assert(t.sample(false,0,0,x,y,g) && g==2 && x==-1 && y==-1);
  assert(!t.sample(true,20,50,x,y,g));
  assert(!t.sample(true,25,110,x,y,g));
  assert(t.sample(false,0,0,x,y,g) && g==1);
  // Horizontal or diagonal drags do not activate a button.
  assert(!t.sample(true,20,50,x,y,g));
  assert(!t.sample(true,90,60,x,y,g));
  assert(!t.sample(false,0,0,x,y,g));
  assert(!t.sample(true,20,50,x,y,g));
  assert(!t.sample(true,80,110,x,y,g));
  assert(!t.sample(false,0,0,x,y,g));
  // Small movement remains one tap at the original target.
  assert(!t.sample(true,20,50,x,y,g));
  assert(!t.sample(true,23,54,x,y,g));
  assert(t.sample(false,0,0,x,y,g) && x==20 && y==50 && g==0);
  std::cout << "PASS: tap, hold, release, vertical swipes and drag suppression\n";
}
'''
with tempfile.TemporaryDirectory(prefix='passvault-touch-') as temp:
    cpp, binary = Path(temp) / 'test.cpp', Path(temp) / 'test'
    cpp.write_text(checks)
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        raise SystemExit('A C++17 compiler is required')
    subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-I', str(root / 'include'),
                    str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
