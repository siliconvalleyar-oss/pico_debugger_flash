#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t usage;    // HID keyboard usage code (0x04=A .. 0x65)
    uint8_t shift;    // 1 si requiere LEFT_SHIFT
} HidKey;
// ASCII imprimible (0x20..0x7E) -> HID usage+shift. Devuelve false si no hay tecla.
bool ascii_to_hid(char c, HidKey *out);
