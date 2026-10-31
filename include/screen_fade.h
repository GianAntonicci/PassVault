#ifndef SCREEN_FADE_H
#define SCREEN_FADE_H

#include <stdint.h>

// Apenas o nivel de luz: nao guarda pixels nem dados das credenciais.
class ScreenFade {
public:
  static constexpr uint32_t DURATION_MS = 180;
  static constexpr uint32_t FRAME_MS = 12;
  uint8_t level = 255;
  bool active = false;

  bool begin(int screen, uint32_t now) {
    if (screen == currentScreen) return false;
    currentScreen = screen;
    startedAt = lastFrame = now;
    level = 0;
    active = true;
    return true;
  }

  bool tick(uint32_t now) {
    if (!active) return false;
    uint32_t elapsed = now - startedAt;  // seguro no rollover de millis()
    if (elapsed >= DURATION_MS) {
      active = false;
      level = 255;
      return true;
    }
    if (now - lastFrame < FRAME_MS) return false;
    lastFrame = now;
    uint32_t t = elapsed * 255 / DURATION_MS;
    // Smoothstep: acelera e desacelera sem degraus nas extremidades.
    level = (t * t * (765 - 2 * t) + 32512) / 65025;
    return true;
  }

  void cancel() {
    active = false;
    level = 255;
    currentScreen = -1;
  }

private:
  int currentScreen = -1;
  uint32_t startedAt = 0;
  uint32_t lastFrame = 0;
};

#endif
