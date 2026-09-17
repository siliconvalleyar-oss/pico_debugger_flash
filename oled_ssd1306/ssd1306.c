/**
 * Copyright (c) 2026
 * Author: optimus
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * SSD1306 128x64 OLED - driver I2C para RP2040 (Pico W).
 * Framebuffer en RAM, modo de direccionamiento horizontal.
 */

#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "ssd1306.h"
#include "ssd1306_font.h"

// Inicializa I2C1 y configura el display (128x64).
void ssd1306_init(ssd1306_t *d, i2c_inst_t *i2c, uint8_t sda_pin, uint8_t scl_pin) {
    d->i2c = i2c;
    d->sda_pin = sda_pin;
    d->scl_pin = scl_pin;

    i2c_init(i2c, 400 * 1000);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);

    const uint8_t init_seq[] = {
        0xAE, // display off
        0x20, 0x00,       // horizontal addressing mode
        0x40,             // display start line 0
        0xA8, 0x3F,       // multiplex ratio 1/64
        0xA0,             // segment remap OFF: col0 -> SEG0 (texto de izq. a der.)
        0xC0,             // COM output scan normal
        0xD3, 0x00,       // display offset 0
        0xD5, 0x80,       // clock div
        0xD9, 0xF1,       // precharge
        0xDA, 0x12,       // COM pins (128x64)
        0x81, 0xCF,       // contrast
        0xA4,             // resume to RAM content
        0xA6,             // normal (no invert)
        0x8D, 0x14,       // charge pump on
        0xAF,             // display on
    };

    for (size_t i = 0; i < sizeof(init_seq); i++) {
        ssd1306_send_cmd(d, init_seq[i]);
    }

    memset(d->buf, 0, sizeof(d->buf));
    ssd1306_update(d);
}

void ssd1306_send_cmd(ssd1306_t *d, uint8_t cmd) {
    uint8_t buf[2] = {0x80, cmd}; // Co=1, D/C=0 -> comando
    i2c_write_blocking(d->i2c, SSD1306_I2C_ADDR, buf, 2, false);
}

// Envía el framebuffer completo al display.
void ssd1306_update(ssd1306_t *d) {
    uint8_t *out = malloc(SSD1306_BUF_LEN + 1);
    if (!out) {
        return;
    }
    out[0] = 0x40; // Co=0, D/C=1 -> datos
    memcpy(out + 1, d->buf, SSD1306_BUF_LEN);
    i2c_write_blocking(d->i2c, SSD1306_I2C_ADDR, out, SSD1306_BUF_LEN + 1, false);
    free(out);
}

void ssd1306_clear(ssd1306_t *d) {
    memset(d->buf, 0, sizeof(d->buf));
}

void ssd1306_set_pixel(ssd1306_t *d, int x, int y, bool on) {
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) {
        return;
    }
    uint32_t idx = (y / 8) * SSD1306_WIDTH + x;
    uint8_t bit = 1u << (y % 8);
    if (on) {
        d->buf[idx] |= bit;
    } else {
        d->buf[idx] &= (uint8_t)~bit;
    }
}

// Bresenham.
void ssd1306_draw_line(ssd1306_t *d, int x0, int y0, int x1, int y1, bool on) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        ssd1306_set_pixel(d, x0, y0, on);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void ssd1306_draw_char(ssd1306_t *d, int x, int y, char c) {
    if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8) {
        return;
    }
    y = y / 8; // página
    uint8_t idx = ssd1306_font_index(c);
    uint32_t fb_idx = y * SSD1306_WIDTH + x;
    for (int i = 0; i < 8; i++) {
        d->buf[fb_idx++] = ssd1306_font[idx * 8 + i];
    }
}

void ssd1306_draw_string(ssd1306_t *d, int x, int y, const char *str) {
    if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8) {
        return;
    }
    while (*str) {
        ssd1306_draw_char(d, x, y, *str++);
        x += 8;
    }
}