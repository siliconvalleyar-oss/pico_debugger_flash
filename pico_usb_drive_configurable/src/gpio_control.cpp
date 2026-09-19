/*
 * gpio_control.cpp - Non-blocking status LED patterns
 *
 * SPDX-License-Identifier: MIT
 *
 * The LED communicates boot / config / USB state with three blink patterns:
 *   - SOLID      : mounted and working correctly
 *   - FAST BLINK : init / formatting / error
 *   - SLOW BLINK : config.txt contains a syntax error (watchdog-style nag)
 *   - OFF        : LED_ON_CONNECT=0 and no activity
 *
 * The pattern is stored as an enum and applied in gpio_led_task(), which must
 * be called regularly (e.g. every loop iteration). No busy waits.
 */

#include <stdint.h>
#include <stdbool.h>
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "gpio_control.h"
#include "config.h"

typedef enum {
    LED_MODE_OFF,
    LED_MODE_SOLID,
    LED_MODE_FAST,      // toggle ~10 Hz
    LED_MODE_SLOW,      // toggle ~1 Hz
} led_mode_t;

static volatile led_mode_t s_mode = LED_MODE_SOLID;
static volatile bool       s_force_off = false;

/* current relative phase of the current pattern */
static uint32_t s_tick = 0;     // ms counter

void gpio_led_init(void) {
    gpio_init(GPIO_LED_ESTADO);
    gpio_set_dir(GPIO_LED_ESTADO, GPIO_OUT);
    gpio_put(GPIO_LED_ESTADO, 0);
    s_mode = LED_MODE_SOLID;
}

void gpio_led_set_solid(void)       { s_mode = LED_MODE_SOLID; s_force_off = false; }
void gpio_led_set_fast_blink(void)  { s_mode = LED_MODE_FAST;  s_force_off = false; }
void gpio_led_set_slow_blink(void)  { s_mode = LED_MODE_SLOW;  s_force_off = false; }
void gpio_led_set_off(void)         { s_force_off = true; }

void gpio_led_task(void) {
    /* drive a stable timebase from the 1 MHz counter -> ms */
    static uint64_t last_us = 0;
    uint64_t now = time_us_64();
    uint32_t delta = (uint32_t)((now - last_us) / 1000ull);
    if (delta == 0) return;
    last_us = now;
    s_tick += delta;

    bool on;
    switch (s_mode) {
        default:
        case LED_MODE_SOLID: on = true; break;
        case LED_MODE_FAST:  on = ((s_tick / 100) % 2); break;   // 500 ms period
        case LED_MODE_SLOW:  on = ((s_tick / 1000) % 2); break; // 2 s period
        case LED_MODE_OFF:   on = false; break;
    }
    if (s_force_off) on = false;
    gpio_put(GPIO_LED_ESTADO, on);
}
