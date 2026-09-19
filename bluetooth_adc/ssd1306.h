#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64
#define SSD1306_PAGES 8

#define SSD1306_I2C_ADDR 0x3C
#define SSD1306_I2C_ADDR_ALT 0x3D

typedef struct {
    i2c_inst_t *i2c;
    uint8_t address;
    uint8_t width;
    uint8_t height;
    uint8_t buffer[SSD1306_WIDTH * SSD1306_PAGES];
    bool inverted;
} ssd1306_t;

bool ssd1306_init(ssd1306_t *disp, i2c_inst_t *i2c, uint8_t address, uint8_t width, uint8_t height);
void ssd1306_deinit(ssd1306_t *disp);
void ssd1306_clear(ssd1306_t *disp);
void ssd1306_show(ssd1306_t *disp);
void ssd1306_set_pixel(ssd1306_t *disp, int x, int y, bool on);
void ssd1306_draw_char(ssd1306_t *disp, int x, int y, char c, bool inverted);
void ssd1306_draw_string(ssd1306_t *disp, int x, int y, const char *str, bool inverted);
void ssd1306_draw_line(ssd1306_t *disp, int x1, int y1, int x2, int y2, bool color);
void ssd1306_draw_rect(ssd1306_t *disp, int x, int y, int w, int h, bool filled, bool color);
void ssd1306_invert_display(ssd1306_t *disp, bool invert);
void ssd1306_set_contrast(ssd1306_t *disp, uint8_t contrast);
void ssd1306_draw_graph(ssd1306_t *disp, int x, int y, int w, int h, const float *values, int count, float min_val, float max_val, const char *title);

#endif