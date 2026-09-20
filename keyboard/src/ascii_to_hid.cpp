#include "ascii_to_hid.h"

// ASCII -> HID boot-keyboard (USB HID Usage Tables 1.4). Solo imprimibles.
static const HidKey LOW = {0x00, 0};
static bool map_alpha(char c, HidKey *out) {
    if (c >= 'a' && c <= 'z') { out->usage = (uint8_t)(0x04 + (c - 'a')); out->shift = 0; return true; }
    if (c >= 'A' && c <= 'Z') { out->usage = (uint8_t)(0x04 + (c - 'A')); out->shift = 1; return true; }
    return false;
}
bool ascii_to_hid(char c, HidKey *out) {
    if (c < 0x20 || c > 0x7E) return false;
    if (map_alpha(c, out)) return true;
    if (c >= '0' && c <= '9') { out->usage = (uint8_t)(0x1E + (c - '0')); out->shift = 0; return true; }
    switch (c) {
        case ' ': { out->usage = 0x2C; out->shift = 0; return true; }
        case '!': { out->usage = 0x1E; out->shift = 1; return true; } // 1+shift
        case '"': { out->usage = 0x34; out->shift = 1; return true; } // '"+shift
        case '#': { out->usage = 0x20; out->shift = 1; return true; }
        case '$': { out->usage = 0x21; out->shift = 1; return true; }
        case '%': { out->usage = 0x22; out->shift = 1; return true; }
        case '&': { out->usage = 0x24; out->shift = 1; return true; }
        case '(': { out->usage = 0x26; out->shift = 1; return true; }
        case ')': { out->usage = 0x27; out->shift = 1; return true; }
        case '*': { out->usage = 0x25; out->shift = 1; return true; } // 8+shift
        case '+': { out->usage = 0x2E; out->shift = 1; return true; }
        case ',': { out->usage = 0x36; out->shift = 0; return true; }
        case '-': { out->usage = 0x2D; out->shift = 0; return true; }
        case '.': { out->usage = 0x37; out->shift = 0; return true; }
        case '/': { out->usage = 0x38; out->shift = 0; return true; }
        case ':': { out->usage = 0x33; out->shift = 1; return true; }
        case ';': { out->usage = 0x33; out->shift = 0; return true; }
        case '<': { out->usage = 0x36; out->shift = 1; return true; }
        case '=': { out->usage = 0x2E; out->shift = 0; return true; }
        case '>': { out->usage = 0x37; out->shift = 1; return true; }
        case '?': { out->usage = 0x38; out->shift = 1; return true; }
        case '@': { out->usage = 0x1F; out->shift = 1; return true; }
        case '[': { out->usage = 0x2F; out->shift = 0; return true; }
        case '\\': { out->usage = 0x31; out->shift = 0; return true; }
        case ']': { out->usage = 0x30; out->shift = 0; return true; }
        case '^': { out->usage = 0x23; out->shift = 1; return true; }
        case '_': { out->usage = 0x2D; out->shift = 1; return true; }
        case '`': { out->usage = 0x35; out->shift = 0; return true; }
        case '{': { out->usage = 0x2F; out->shift = 1; return true; }
        case '|': { out->usage = 0x31; out->shift = 1; return true; }
        case '}': { out->usage = 0x30; out->shift = 1; return true; }
        case '~': { out->usage = 0x35; out->shift = 1; return true; }
        default: (void)LOW; return false;
    }
}
