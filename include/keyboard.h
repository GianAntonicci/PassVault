#ifndef KEYBOARD_H
#define KEYBOARD_H

// =====================================================================
//  Abstracao de teclado: USB HID (ESP32-S3) ou BLE (ESP32 classico)
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

#elif defined(KBD_BLE)

#include <BleKeyboard.h>

BleKeyboard kbd(BLE_KBD_NAME, "PassVault", 100);

void kbdBegin() { kbd.begin(); }

bool kbdConnected() { return kbd.isConnected(); }
const char* kbdLabel() { return "BLE"; }

// Sem host pareado nao ha para onde digitar
void kbdPrint(const char* s) { if (kbd.isConnected()) kbd.print(s); }
void kbdWrite(uint8_t k)     { if (kbd.isConnected()) kbd.write(k); }

#else
  #error "Nenhum teclado definido (KBD_USB_HID / KBD_BLE)"
#endif

#endif
