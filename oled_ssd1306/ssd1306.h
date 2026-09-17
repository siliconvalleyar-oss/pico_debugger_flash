/**
 * Copyright (c) 2026
 * Author: optimus
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * SSD1306 128x64 OLED - driver I2C minimalista para RP2040 (Pico W).
 * Usa I2C1 (SDA=GP2, SCL=GP3) a 400 kHz. Dirección 0x3C.
 */

#ifndef SSD1306_H
#define SSD1306_H

#include <stdbool.h>
#include <stdint.h>
#include "hardware/i2c.h"

#define SSD1306_WIDTH    128
#define SSD1306_HEIGHT   64
#define SSD1306_I2C_ADDR 0x3C

#define SSD1306_NUM_PAGES (SSD1306_HEIGHT / 8)
#define SSD1306_BUF_LEN   (SSD1306_NUM_PAGES * SSD1306_WIDTH)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_inst_t *i2c;
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint8_t buf[SSD1306_BUF_LEN];
} ssd1306_t;

void ssd1306_init(ssd1306_t *d, i2c_inst_t *i2c, uint8_t sda_pin, uint8_t scl_pin);
void ssd1306_send_cmd(ssd1306_t *d, uint8_t cmd);
void ssd1306_update(ssd1306_t *d);
void ssd1306_clear(ssd1306_t *d);
void ssd1306_set_pixel(ssd1306_t *d, int x, int y, bool on);
void ssd1306_draw_line(ssd1306_t *d, int x0, int y0, int x1, int y1, bool on);
void ssd1306_draw_char(ssd1306_t *d, int x, int y, char c);
void ssd1306_draw_string(ssd1306_t *d, int x, int y, const char *str);

#ifdef __cplusplus
}
#endif

#endif // SSD1306_H