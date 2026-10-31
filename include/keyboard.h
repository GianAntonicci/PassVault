#ifndef KEYBOARD_H
#define KEYBOARD_H

// =====================================================================
//  Teclado USB HID nativo (ESP32-S3)
// =====================================================================

#include <Arduino.h>
#include "config.h"

#if defined(KBD_USB_HID)

#include <USB.h>
#include <USBHIDKeyboard.h>

USBHIDKeyboard kbd;

void kbdBegin() {
  kbd.begin();
  USB.begin();
}

// USB: sempre "conectado" quando enumerado pelo host
bool kbdConnected() { return true; }
const char* kbdLabel() { return "USB"; }

void kbdPrint(const char* s) { kbd.print(s); }
void kbdWrite(uint8_t k)     { kbd.write(k); }

#else
  #error "PassVault requer uma placa com teclado USB HID nativo (KBD_USB_HID)"
#endif

#endif
