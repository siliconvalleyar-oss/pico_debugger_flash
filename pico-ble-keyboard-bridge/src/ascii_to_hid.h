#pragma once

#include <cstdint>

/// Máscaras de modificador HID estándar (byte 0 del reporte boot keyboard).
namespace HidMod {
constexpr uint8_t kNone     = 0x00;
constexpr uint8_t kLeftCtrl = 0x01;
constexpr uint8_t kLeftShift = 0x02;
constexpr uint8_t kLeftAlt  = 0x04;
constexpr uint8_t kLeftGui  = 0x08;
}

/// Algunos keycodes HID "especiales" usados por los comandos \ENTER, etc.
/// (Usage IDs de la tabla "Keyboard/Keypad Page" del HID Usage Tables spec)
namespace HidKey {
constexpr uint8_t kEnter     = 0x28;
constexpr uint8_t kEscape    = 0x29;
constexpr uint8_t kBackspace = 0x2A;
constexpr uint8_t kTab       = 0x2B;
}

/// Resultado de convertir un caracter ASCII a su representación HID.
struct HidKeyMapping {
    uint8_t modifier; ///< 0 o HidMod::kLeftShift
    uint8_t keycode;  ///< Usage ID HID, 0 si el caracter no es representable
};

/// Convierte un caracter ASCII imprimible (0x20-0x7E) a su keycode HID
/// + modificador correspondiente, asumiendo layout US QWERTY (el más
/// universalmente soportado por hosts USB HID boot protocol).
///
/// @param ascii Caracter de entrada.
/// @return Mapping con keycode == 0 si el caracter no tiene representación
///         directa en el layout US (ej. tildes, ñ, caracteres UTF-8 multibyte).
HidKeyMapping ascii_to_hid(char ascii);
