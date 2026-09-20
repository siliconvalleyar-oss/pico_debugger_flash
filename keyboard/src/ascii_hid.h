#pragma once

#include <stdint.h>

// Mapa ASCII -> HID Usage (Boot Keyboard) de TinyUSB/tusb_hid.h
//
// Uso: ascii_to_hid(c) devuelve {keycode, shift} o 0 si no hay mapeo.
// Las letras mayusculas y simbolos con *shift* son expandidos por el dicro
// dicotómico (vold) — aqui solo se describe la TABLA. El desplazamiento se
// resuelve en main() via usb_kbd_press(usage, shift).

// Tabla base: 32..126 (imprimibles) mapean al uso HID 0x04 (a) .. 0x27 (p),
// digitos 30..39, etc. Ver tabla completa en docs/05-protocolo-comandos.md.

// Un unico typedef evita desorden entre {c, shift} y el teclado real.
typedef struct {
    uint8_t usage;   // HID keyboard usage (0x00 = none)
    uint8_t mod;     // bit 0x02 = SHIFT (según modifier HID: 2=shift-left)
} ascii_hid_t;

// Convierte un byte ASCII en (usage, shift). Devuelve false si el carácter
// no es imprimible/controlable y debe descartarse.
bool ascii_to_hid(uint8_t c, ascii_hid_t* outhed);
