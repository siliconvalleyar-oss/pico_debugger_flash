#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>

#define SSD1306_I2C_ADDR                0x3C
#define SSD1306_WIDTH                   128
#define SSD1306_HEIGHT                  64
#define SSD1306_PAGES                   8

#define SSD1306_I2C_SDA_PIN             4
#define SSD1306_I2C_SCL_PIN             5
#define SSD1306_I2C_INSTANCE            i2c0
#define SSD1306_I2C_BAUDRATE            400000

typedef struct {
    uint8_t buffer[SSD1306_WIDTH * SSD1306_PAGES];
    bool dirty[SSD1306_PAGES];
} ssd1306_t;

bool ssd1306_init(ssd1306_t *display);
void ssd1306_clear(ssd1306_t *display);
void ssd1306_draw_pixel(ssd1306_t *display, int x, int y, bool on);
void ssd1306_draw_char(ssd1306_t *display, int x, int y, char c, bool on);
void ssd1306_draw_string(ssd1306_t *display, int x, int y, const char *str, bool on);
void ssd1306_draw_line(ssd1306_t *display, int x1, int y1, int x2, int y2, bool on);
void ssd1306_draw_rect(ssd1306_t *display, int x, int y, int w, int h, bool filled, bool on);
void ssd1306_update(ssd1306_t *display);
void ssd1306_set_contrast(ssd1306_t *display, uint8_t contrast);
void ssd1306_invert(ssd1306_t *display, bool invert);
void ssd1306_sleep(ssd1306_t *display, bool sleep);

void ssd1306_show_status(ssd1306_t *display, bool usb_connected, bool wifi_ap_active,
                         int wifi_clients, const char *ip_str);
void ssd1306_show_error(ssd1306_t *display, const char *msg);

#endif