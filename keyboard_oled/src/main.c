/*
 * main.c — Pico W BLE-to-USB HID keyboard bridge.
 *
 * Receives characters over BLE (via ble_server.c), converts them to HID
 * keycodes via ascii_to_hid(), and sends them to the host via TinyUSB HID.
 */
#include "tusb.h"
#include <stdio.h>
#include <string.h>
#include "btstack_run_loop.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "ble_server.h"
#include "ascii_to_hid.h"

#define HEARTBEAT_PERIOD_MS 1000

/* Pico W LED is on CYW43 GPIO 0 */
#ifndef CYW43_WL_GPIO_LED_PIN
#define CYW43_WL_GPIO_LED_PIN 0
#endif

/* Heartbeat: toggle LED when BLE has data queued */
static void heartbeat_handler(struct btstack_timer_source *ts) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, ble_server_connected());
    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
}

int main(void) {
    stdio_init_all();

    if (cyw43_arch_init()) {
        printf("CYW43 init failed\n");
        return 1;
    }

    /* Initialize BLE server (only touches btstack, no tusb.h collision) */
    ble_server_init();

    /* Start heartbeat timer */
    static btstack_timer_source_t heartbeat;
    heartbeat.process = heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeat);

    printf("Pico KB Bridge started\n");

    /* Main loop: bridge BLE -> USB HID */
    while (true) {
        char c;
        while (ble_server_get_char(&c)) {
            HidKey k;
            if (ascii_to_hid(c, &k)) {
                tud_hid_keyboard_report(0, k.shift ? 0x02 : 0x00,
                                       (uint8_t[6]){k.usage, 0, 0, 0, 0, 0});
            }
        }
        /* Release all keys */
        tud_hid_keyboard_report(0, 0, (uint8_t[6]){0, 0, 0, 0, 0, 0});

        tud_task();
        btstack_run_loop_execute();
    }
}
