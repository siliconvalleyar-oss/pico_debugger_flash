/**
 * Copyright (c) 2026
 * Author: optimus
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Pico W + SSD1306 128x64 OLED (I2C1: SDA=GP2, SCL=GP3).
 * Muestra info del sistema en el OLED y parpadea el LED onboard.
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/clocks.h"

#include "ssd1306.h"

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#define OLED_SDA_PIN 2
#define OLED_SCL_PIN 3

static void led_set(bool on) {
#ifdef CYW43_WL_GPIO_LED_PIN
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
#elif defined(PICO_DEFAULT_LED_PIN)
    gpio_put(PICO_DEFAULT_LED_PIN, on);
#else
    (void)on;
#endif
}

int main() {
    stdio_init_all();
    sleep_ms(1000);

#ifdef CYW43_WL_GPIO_LED_PIN
    if (cyw43_arch_init() != 0) {
        printf("Error: cyw43_arch_init fallo\r\n");
        while (true) { tight_loop_contents(); }
    }
#endif

    static ssd1306_t oled;
    ssd1306_init(&oled, i2c1, OLED_SDA_PIN, OLED_SCL_PIN);

    printf("\r\n=== PICO W + SSD1306 OLED ===\r\n");
    printf("I2C1 en SDA=GP%d SCL=GP%d\r\n", OLED_SDA_PIN, OLED_SCL_PIN);
    printf("CPU clock: %lu MHz\r\n", clock_get_hz(clk_sys) / 1000000);

    uint16_t ticks = 0;
    while (true) {
        // LED parpadea cada tick (500 ms)
        led_set(true);
        ssd1306_clear(&oled);

        ssd1306_draw_string(&oled, 0, 0, "PICO W + SSD1306");
        ssd1306_draw_line(&oled, 0, 10, 127, 10, true);

        ssd1306_draw_string(&oled, 0, 16, "I2C1 SDA=GP2");
        ssd1306_draw_string(&oled, 0, 24, "SCL=GP3");
        ssd1306_draw_string(&oled, 64, 16, "128x64");
        ssd1306_draw_string(&oled, 0, 40, "CPU 125 MHZ");
        ssd1306_draw_string(&oled, 0, 48, "SDK 2.1.0");

        char line[24];
        snprintf(line, sizeof(line), "TICK #%u", ticks);
        ssd1306_draw_string(&oled, 0, 56, line);

        ssd1306_update(&oled);
        sleep_ms(500);

        led_set(false);
        ticks++;
    }
}