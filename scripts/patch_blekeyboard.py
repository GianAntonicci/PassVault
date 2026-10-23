# Fix da lib t-vk/ESP32 BLE Keyboard para o core Arduino-ESP32 3.x:
# BLEDevice::init() e setValue() passaram a receber String em vez de std::string.
# Roda antes do build (extra_scripts = pre:) e e idempotente.
import os

Import("env")  # noqa: F821

PATCHES = [
    ("BLEDevice::init(deviceName);",
     "BLEDevice::init(String(deviceName.c_str()));"),
    ("hid->manufacturer()->setValue(deviceManufacturer);",
     "hid->manufacturer()->setValue(String(deviceManufacturer.c_str()));"),
    # driver ADC legado: nao e usado e gera aviso de deprecated no core 3.x
    ("#include <driver/adc.h>\n", ""),
]

libdeps = env.subst("$PROJECT_LIBDEPS_DIR/$PIOENV")  # noqa: F821
for root, _, files in os.walk(libdeps):
    if "BleKeyboard.cpp" not in files:
        continue
    path = os.path.join(root, "BleKeyboard.cpp")
    with open(path, encoding="utf-8") as f:
        src = f.read()
    new = src
    for old, fixed in PATCHES:
        new = new.replace(old, fixed)
    if new != src:
        with open(path, "w", encoding="utf-8") as f:
            f.write(new)
        print("PassVault: BleKeyboard.cpp corrigido para core 3.x")
