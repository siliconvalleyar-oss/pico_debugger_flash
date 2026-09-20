#ifndef PICO_OLED_SSD1306_H
#define PICO_OLED_SSD1306_H

/* SSD1306 128x64 OLED driver — I2C0 del RP2040 (Pico W)
 *   SDA = GP4 (I2C0_SDA, mux Verified del chip)
 *   SCL = GP5 (I2C0_SCL, mux Verified del chip)
 * Dirección I2C del display: 0x3C (SA0=0, SSD1306 nativo)
 *
 * Patrón 1:1 del driver "Verified" del SDK en disco:
 *   /mnt/disk/src/rpico/pico_src/tmp/oled_ssd1306/ssd1306.[ch] (Verified
 *   [100%] en sesión previa) — buffer 1024 B (128*64/8), secuencia de init
 *   del datasheet SSD1306, comando a 0x00 / datos a 0x40 (protocolo I2C).
 */

#include "hardware/i2c.h"

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_PAGES 8

struct oled_dev {
    i2c_inst_t *i2c;
    uint8_t addr;
    uint8_t buf[OLED_WIDTH * OLED_HEIGHT / 8];
};

void oled_init(struct oled_dev *dev);
void oled_clear(struct oled_dev *dev);
void oled_pixel(struct oled_dev *dev, uint8_t x, uint8_t y, bool on);
void oled_char(struct oled_dev *dev, uint8_t x, uint8_t y, char c);
void oled_text(struct oled_dev *dev, uint8_t col, uint8_t row, const char *s);
void oled_blit(struct oled_dev *dev);

#endif
