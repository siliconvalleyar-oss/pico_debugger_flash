#pragma once

#include <stdint.h>

/* SSD1306 128x64 I2C driver (5x7 font), 1 KiB framebuffer held in RAM. */

void ssd1306_init(void);
void ssd1306_clear(void);
void ssd1306_flush(void);

void ssd1306_putc(uint8_t x, uint8_t y, char c);        /* pixel column, page */
void ssd1306_puts(uint8_t x, uint8_t y, const char *s);
void ssd1306_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);

/* helpers: 1 char high at arbitrary (0..127, 0..7) */
void ssd1306_bar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t pct);