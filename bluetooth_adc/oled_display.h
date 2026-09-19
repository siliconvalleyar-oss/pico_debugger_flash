#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "ssd1306.h"
#include "adc_monitor.h"
#include <stdbool.h>

#define DISPLAY_UPDATE_MS 500

typedef struct {
    ssd1306_t *display;
    adc_monitor_t *monitor;
    uint32_t last_update;
    int current_page;
    int num_pages;
    bool show_graph;
    bool inverted;
} oled_display_t;

void oled_display_init(oled_display_t *disp, ssd1306_t *ssd1306, adc_monitor_t *monitor);
void oled_display_update(oled_display_t *disp);
void oled_display_draw_page0(oled_display_t *disp);
void oled_display_draw_page1(oled_display_t *disp);
void oled_display_draw_page2(oled_display_t *disp);
void oled_display_draw_page3(oled_display_t *disp);
void oled_display_next_page(oled_display_t *disp);
void oled_display_prev_page(oled_display_t *disp);
void oled_display_toggle_graph(oled_display_t *disp);

#endif