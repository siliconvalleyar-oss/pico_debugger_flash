#include "ascii_to_hid.h"

namespace {

/// Tabla de letras minúsculas 'a'-'z' -> Usage ID HID (0x04-0x1D).
constexpr uint8_t kLetterBase = 0x04;

/// Tabla de dígitos '1'-'9','0' -> Usage ID HID (0x1E-0x27).
/// Nota: en HID, '1' es 0x1E y '0' es 0x27 (el orden no es correlativo
/// al carácter, así que se resuelve por switch más abajo).

} // namespace

HidKeyMapping ascii_to_hid(char c) {
    HidKeyMapping m{HidMod::kNone, 0};

    // Letras minúsculas
    if (c >= 'a' && c <= 'z') {
        m.keycode = kLetterBase + (c - 'a');
        return m;
    }

    // Letras mayúsculas: mismo keycode que la minúscula + Shift
    if (c >= 'A' && c <= 'Z') {
        m.modifier = HidMod::kLeftShift;
        m.keycode = kLetterBase + (c - 'A');
        return m;
    }

    // Dígitos y símbolos de la fila superior (requieren mapeo manual porque
    // el orden de Usage IDs de HID no es correlativo al ASCII)
    switch (c) {
        case '1': m.keycode = 0x1E; return m;
        case '2': m.keycode = 0x1F; return m;
        case '3': m.keycode = 0x20; return m;
        case '4': m.keycode = 0x21; return m;
        case '5': m.keycode = 0x22; return m;
        case '6': m.keycode = 0x23; return m;
        case '7': m.keycode = 0x24; return m;
        case '8': m.keycode = 0x25; return m;
        case '9': m.keycode = 0x26; return m;
        case '0': m.keycode = 0x27; return m;

        case '!': m.modifier = HidMod::kLeftShift; m.keycode = 0x1E; return m;
        case '@': m.modifier = HidMod::kLeftShift; m.keycode = 0x1F; return m;
        case '#': m.modifier = HidMod::kLeftShift; m.keycode = 0x20; return m;
        case '$': m.modifier = HidMod::kLeftShift; m.keycode = 0x21; return m;
        case '%': m.modifier = HidMod::kLeftShift; m.keycode = 0x22; return m;
        case '^': m.modifier = HidMod::kLeftShift; m.keycode = 0x23; return m;
        case '&': m.modifier = HidMod::kLeftShift; m.keycode = 0x24; return m;
        case '*': m.modifier = HidMod::kLeftShift; m.keycode = 0x25; return m;
        case '(': m.modifier = HidMod::kLeftShift; m.keycode = 0x26; return m;
        case ')': m.modifier = HidMod::kLeftShift; m.keycode = 0x27; return m;

        case ' ': m.keycode = 0x2C; return m;              // Space
        case '-': m.keycode = 0x2D; return m;              // Minus
        case '_': m.modifier = HidMod::kLeftShift; m.keycode = 0x2D; return m;
        case '=': m.keycode = 0x2E; return m;              // Equal
        case '+': m.modifier = HidMod::kLeftShift; m.keycode = 0x2E; return m;
        case '[': m.keycode = 0x2F; return m;
        case '{': m.modifier = HidMod::kLeftShift; m.keycode = 0x2F; return m;
        case ']': m.keycode = 0x30; return m;
        case '}': m.modifier = HidMod::kLeftShift; m.keycode = 0x30; return m;
        case '\\': m.keycode = 0x31; return m;
        case '|': m.modifier = HidMod::kLeftShift; m.keycode = 0x31; return m;
        case ';': m.keycode = 0x33; return m;
        case ':': m.modifier = HidMod::kLeftShift; m.keycode = 0x33; return m;
        case '\'': m.keycode = 0x34; return m;
        case '"': m.modifier = HidMod::kLeftShift; m.keycode = 0x34; return m;
        case '`': m.keycode = 0x35; return m;
        case '~': m.modifier = HidMod::kLeftShift; m.keycode = 0x35; return m;
        case ',': m.keycode = 0x36; return m;
        case '<': m.modifier = HidMod::kLeftShift; m.keycode = 0x36; return m;
        case '.': m.keycode = 0x37; return m;
        case '>': m.modifier = HidMod::kLeftShift; m.keycode = 0x37; return m;
        case '/': m.keycode = 0x38; return m;
        case '?': m.modifier = HidMod::kLeftShift; m.keycode = 0x38; return m;

        case '\n': m.keycode = HidKey::kEnter; return m;
        case '\t': m.keycode = HidKey::kTab; return m;
        case 0x08: m.keycode = HidKey::kBackspace; return m;
        case 0x1B: m.keycode = HidKey::kEscape; return m;

        default:
            // Caracter no representable en layout US (tildes, ñ, UTF-8, etc.)
            // keycode queda en 0: el llamador debe ignorarlo o loguearlo.
            return m;
    }
}
