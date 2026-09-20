/*
 * usb_kbd.c — TinyUSB HID Boot Keyboard (boot protocol, 6KRO).
 * Envía un reporte HID tras cada caracter recibido por BLE (patrón de la
 * tabla ascii_to_hid ya verificada: usage+shift).
 */
#include "usb_kbd.h"
#include "ascii_to_hid.h"
#include "tusb.h"
#include <string.h>

#define HID_REPORT_RETRY_MS 5

/* Reporte "todas teclas sueltas" de 8 bytes (boot keyboard). */
void usb_kbd_release_all(void) {
    uint8_t empty[8] = {0};
    tud_hid_keyboard_report(0, 0x00, empty + 2);
}

/* Teclea UN carácter ASCII convirtiéndolo con la tabla (usage + shift).
 * Devuelve true si se aceptó (siempre lo hace con el boot report). */
bool usb_kbd_type_char(char c) {
    HidKey k;
    if (!ascii_to_hid(c, &k)) return false;

    uint8_t report[8] = {0};
    report[0] = k.shift ? 0x02 : 0x00;   /* modifier: KEY_LEFT_SHIFT */
    report[2] = k.usage;                  /* keycode en report[2] (6KRO) */
    tud_hid_keyboard_report(0, report[0], report + 2);;
    tud_task();                           /* Fuerza el envío inmediato. */
    return true;
}

/* Teclea una cadena completa pasando por la cola TinyUSB. */
void usb_kbd_type_string(const char *s) {
    while (s && *s) {
        usb_kbd_type_char(*s);
        s++;
    }
    usb_kbd_release_all();
}
