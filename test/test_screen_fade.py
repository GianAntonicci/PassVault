"""Host tests for screen transitions. Run: python3 test/test_screen_fade.py."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/main.cpp").read_text()
brightness = source[source.index("void applyBrightness() {"):source.index("void beginScreen() {")]
tests = r'''
#include <cassert>
#include "screen_fade.h"
ScreenFade screenFade;
int displayBrightness = 100;
int writtenDuty = -1;
#define HIGH 1
#define TFT_BL 38
#define TFT_BACKLIGHT_ON BACKLIGHT_POLARITY
void analogWrite(int pin, int duty) { assert(pin == TFT_BL); writtenDuty = duty; }
''' + brightness + r'''
int main() {
  assert(!screenFade.tick(0));
  assert(screenFade.begin(1, 100));
  assert(screenFade.level == 0 && screenFade.active);
  assert(!screenFade.tick(111));
  int previous = 0;
  for (uint32_t now = 112; now <= 280; now += 12) {
    assert(screenFade.tick(now));
    assert(screenFade.level >= previous);
    previous = screenFade.level;
  }
  assert(!screenFade.active && screenFade.level == 255);
  assert(!screenFade.begin(1, 300));  // redesenho nao pisca
  assert(!screenFade.tick(300));

  assert(screenFade.begin(2, 400));
  assert(screenFade.tick(490));
  assert(screenFade.level >= 125 && screenFade.level <= 129);
  assert(!screenFade.begin(2, 491));  // nao reinicia no meio do fade
  assert(screenFade.begin(3, 492));   // troca de tela interrompe o fade
  assert(screenFade.level == 0);
  screenFade.cancel();               // avisos de seguranca ficam visiveis
  assert(!screenFade.active && screenFade.level == 255);
  assert(!screenFade.tick(600));
  assert(screenFade.begin(3, 600));
  assert(screenFade.tick(1600));      // loop atrasado termina a animacao
  assert(screenFade.level == 255 && !screenFade.active);

  screenFade.begin(4, UINT32_MAX - 50);
  screenFade.tick(39);               // 90 ms depois, atraves do rollover
  assert(screenFade.active && screenFade.level >= 125 && screenFade.level <= 129);
  screenFade.tick(129);
  assert(!screenFade.active && screenFade.level == 255);

  // O brilho final e preservado, inclusive com backlight ativo em LOW.
  for (int brightness = 10; brightness <= 100; brightness += 10) {
    displayBrightness = brightness;
    screenFade.level = 255;
    applyBrightness();
    int expected = (brightness * 255 + 50) / 100;
    assert(writtenDuty == (BACKLIGHT_POLARITY ? expected : 255 - expected));
    screenFade.level = 0;
    applyBrightness();
    assert(writtenDuty == (BACKLIGHT_POLARITY ? 0 : 255));
    assert(displayBrightness == brightness);
  }
}
'''

compiler = shutil.which("c++") or shutil.which("g++")
if not compiler:
    raise SystemExit("A C++ compiler is required")
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / "screen_fade_test.cpp"
    cpp.write_text(tests)
    for polarity in (0, 1):
        binary = Path(tmp) / f"screen_fade_{polarity}"
        subprocess.run([compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror",
                        f"-DBACKLIGHT_POLARITY={polarity}", "-I", str(root / "include"),
                        str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
print("Screen fade tests passed (timing, interruption, rollover, brightness and polarity)")
