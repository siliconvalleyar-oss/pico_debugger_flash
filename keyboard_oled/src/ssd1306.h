#ifndef SSD1306_H
#define SSD1306_H

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define OLED_I2C_ADDR 0x3C
#define OLED_WIDTH  128
#define OLED_HEIGHT 64
/* buffer de página (8 páginas x 128 bytes = 1024B, 1 byte/página-column) */
#define OLED_BUFSZ (OLED_WIDTH * OLED_HEIGHT / 8)

struct oled_dev {
    i2c_inst_t *i2c;
    uint8_t addr;
    uint8_t buf[OLED_BUFSZ];
};

void oled_init(struct oled_dev *dev, i2c_inst_t *i2c, uint8_t addr);
void oled_clear(struct oled_dev *dev);
void oled_pixel(struct oled_dev *dev, uint8_t x, uint8_t y, bool on);
/* texto: col=0..20 (6px/char), row=0..7 (8px/row) */
void oled_text(struct oled_dev *dev, uint8_t col, uint8_t row, const char *s);
void oled_show(struct oled_dev *dev);

#endif
