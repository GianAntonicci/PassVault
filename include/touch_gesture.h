#ifndef TOUCH_GESTURE_H
#define TOUCH_GESTURE_H

// Emit one event on release, regardless of the controller's interrupt rate.
// Gesture values match the CST816 interface used by input.h.
struct TouchGesture {
  bool down = false;
  int startX = 0, startY = 0, lastX = 0, lastY = 0;

  bool sample(bool pressed, int px, int py, int &x, int &y, int &gesture) {
    if (pressed) {
      if (!down) { startX = px; startY = py; }
      down = true;
      lastX = px; lastY = py;
      return false;
    }
    if (!down) return false;
    down = false;
    int dx = lastX - startX, dy = lastY - startY;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    x = startX; y = startY; gesture = 0;
    if (ay >= 40 && ay > ax) {
      x = y = -1;
      gesture = dy > 0 ? 1 : 2;
    } else if (ax >= 40 || ay >= 40) {
      return false;  // A drag must not activate the button at its starting point.
    }
    return true;
  }
};

#endif
