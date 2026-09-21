#include "usb_hid_keyboard.h"
#include "ascii_to_hid.h"
#include "key_queue.h"

#include "tusb.h"
#include "pico/time.h"
#include <cstring>

namespace {

KeyQueue g_queue;

// Estado del "drenado" de la cola: cuándo es seguro mandar el próximo
// reporte HID (limitado por tud_hid_ready() y por los delays encolados).
uint64_t g_next_event_time_us = 0;
bool g_key_currently_down = false;

/// Envía un reporte boot-keyboard de 8 bytes: [modifier, reserved, k1..k6]
void send_report(uint8_t modifier, uint8_t keycode) {
    uint8_t keycodes[6] = {0};
    if (keycode != 0) {
        keycodes[0] = keycode;
    }
    tud_hid_keyboard_report(/*report_id=*/0, modifier, keycodes);
}

} // namespace

void usb_kbd_init(void) {
    tusb_init();
}

size_t usb_kbd_type_string(const char *str) {
    size_t enqueued = 0;
    if (str == nullptr) {
        return 0;
    }
    for (size_t i = 0; str[i] != '\0'; ++i) {
        HidKeyMapping map = ascii_to_hid(str[i]);
        if (map.keycode == 0) {
            // Caracter no representable en layout US: se omite (no cuenta
            // como error fatal, pero tampoco se encola nada para él).
            continue;
        }
        KeyEvent ev{map.modifier, map.keycode, /*delay_ms=*/8};
        if (!g_queue.push(ev)) {
            // Buffer lleno: troceamos acá, el resto de la cadena se pierde.
            // El llamador puede reintentar más tarde con el remanente.
            break;
        }
        enqueued++;
    }
    return enqueued;
}

void usb_kbd_press(uint8_t keycode, uint8_t modifier) {
    KeyEvent ev{modifier, keycode, /*delay_ms=*/8};
    g_queue.push(ev);
}

void usb_kbd_release(void) {
    KeyEvent ev{HidMod::kNone, 0, /*delay_ms=*/0};
    g_queue.push(ev);
}

void usb_kbd_delay(uint16_t ms) {
    KeyEvent ev{HidMod::kNone, 0, ms};
    g_queue.push(ev);
}

void usb_kbd_task(void) {
    tud_task(); // Bombea la pila TinyUSB (no bloquea)

    if (!tud_hid_ready()) {
        return; // El host todavía no puede recibir otro reporte
    }

    uint64_t now = time_us_64();
    if (now < g_next_event_time_us) {
        return; // Todavía esperando el delay del evento anterior
    }

    if (g_key_currently_down) {
        // Toca soltar la tecla que quedó presionada del evento anterior
        send_report(HidMod::kNone, 0);
        g_key_currently_down = false;
        g_next_event_time_us = now + 4000; // 4ms de guarda entre press/release
        return;
    }

    KeyEvent ev;
    if (!g_queue.pop(ev)) {
        return; // No hay nada pendiente
    }

    if (ev.keycode == 0 && ev.delay_ms > 0) {
        // Evento de tipo "solo esperar" (\SLEEP)
        g_next_event_time_us = now + (uint64_t)ev.delay_ms * 1000;
        return;
    }

    send_report(ev.modifier, ev.keycode);
    g_key_currently_down = (ev.keycode != 0);
    g_next_event_time_us = now + (uint64_t)ev.delay_ms * 1000;
}
