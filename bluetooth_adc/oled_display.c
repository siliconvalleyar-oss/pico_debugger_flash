#include "oled_display.h"
#include <stdio.h>
#include <string.h>

void oled_display_init(oled_display_t *disp, ssd1306_t *ssd1306, adc_monitor_t *monitor) {
    disp->display = ssd1306;
    disp->monitor = monitor;
    disp->last_update = 0;
    disp->current_page = 0;
    disp->num_pages = 4;
    disp->show_graph = true;
    disp->inverted = false;
}

void oled_display_update(oled_display_t *disp) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - disp->last_update < DISPLAY_UPDATE_MS) return;
    disp->last_update = now;

    ssd1306_clear(disp->display);

    switch (disp->current_page) {
        case 0: oled_display_draw_page0(disp); break;
        case 1: oled_display_draw_page1(disp); break;
        case 2: oled_display_draw_page2(disp); break;
        case 3: oled_display_draw_page3(disp); break;
    }

    ssd1306_show(disp->display);
}

void oled_display_draw_page0(oled_display_t *disp) {
    adc_monitor_t *mon = disp->monitor;
    ssd1306_t *d = disp->display;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "ADC Monitor v1.0");
    ssd1306_draw_string(d, 0, 0, buf, false);
    
    snprintf(buf, sizeof(buf), "Vref: %.2fV Ch:%d", mon->vref, mon->num_channels);
    ssd1306_draw_string(d, 0, 10, buf, false);
    
    for (int i = 0; i < mon->num_channels; i++) {
        adc_channel_t *ch = &mon->channels[i];
        if (!ch->enabled) continue;
        int y = 22 + i * 12;
        snprintf(buf, sizeof(buf), "CH%d: %.3fV %s%s", i, ch->voltage,
            ch->alert_high ? "HIGH " : "",
            ch->alert_low ? "LOW " : "");
        ssd1306_draw_string(d, 0, y, buf, ch->alert_high || ch->alert_low);
    }
    
    // Page indicator
    snprintf(buf, sizeof(buf), "Page 1/%d", disp->num_pages);
    ssd1306_draw_string(d, 0, 56, buf, false);
}

void oled_display_draw_page1(oled_display_t *disp) {
    adc_monitor_t *mon = disp->monitor;
    ssd1306_t *d = disp->display;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Voltage Details");
    ssd1306_draw_string(d, 0, 0, buf, false);
    
    for (int i = 0; i < mon->num_channels; i++) {
        adc_channel_t *ch = &mon->channels[i];
        if (!ch->enabled) continue;
        int y = 12 + i * 12;
        snprintf(buf, sizeof(buf), "CH%d: R=%d V=%.3f", i, ch->raw_value, ch->voltage);
        ssd1306_draw_string(d, 0, y, buf, false);
        snprintf(buf, sizeof(buf), "  Min:%.3f Max:%.3f Avg:%.3f", ch->min_voltage, ch->max_voltage, ch->avg_voltage);
        ssd1306_draw_string(d, 0, y + 8, buf, false);
    }
    
    snprintf(buf, sizeof(buf), "Page 2/%d", disp->num_pages);
    ssd1306_draw_string(d, 0, 56, buf, false);
}

void oled_display_draw_page2(oled_display_t *disp) {
    adc_monitor_t *mon = disp->monitor;
    ssd1306_t *d = disp->display;
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Thresholds");
    ssd1306_draw_string(d, 0, 0, buf, false);
    
    for (int i = 0; i < mon->num_channels; i++) {
        adc_channel_t *ch = &mon->channels[i];
        if (!ch->enabled) continue;
        int y = 12 + i * 12;
        snprintf(buf, sizeof(buf), "CH%d: Low=%.2f High=%.2f", i, ch->threshold_low, ch->threshold_high);
        ssd1306_draw_string(d, 0, y, buf, false);
        snprintf(buf, sizeof(buf), "  Status: %s%s", ch->alert_high ? "HIGH " : "", ch->alert_low ? "LOW " : "");
        ssd1306_draw_string(d, 0, y + 8, buf, ch->alert_high || ch->alert_low);
    }
    
    snprintf(buf, sizeof(buf), "Page 3/%d", disp->num_pages);
    ssd1306_draw_string(d, 0, 56, buf, false);
}

void oled_display_draw_page3(oled_display_t *disp) {
    adc_monitor_t *mon = disp->monitor;
    ssd1306_t *d = disp->display;
    char buf[32];
    
    if (!disp->show_graph || mon->num_channels == 0) {
        ssd1306_draw_string(d, 0, 0, "Graph View (disabled)", false);
        snprintf(buf, sizeof(buf), "Page 4/%d", disp->num_pages);
        ssd1306_draw_string(d, 0, 56, buf, false);
        return;
    }
    
    // Draw graph for first channel
    adc_channel_t *ch = &mon->channels[0];
    if (!ch->enabled) {
        ssd1306_draw_string(d, 0, 0, "No active channel", false);
        return;
    }
    
    ssd1306_draw_graph(d, 0, 0, 128, 48, ch->history, ch->history_count, ch->min_voltage, ch->max_voltage, "CH0 Voltage");
    
    snprintf(buf, sizeof(buf), "CH0: %.3fV (avg:%.3f)", ch->voltage, ch->avg_voltage);
    ssd1306_draw_string(d, 0, 50, buf, false);
    
    snprintf(buf, sizeof(buf), "Page 4/%d", disp->num_pages);
    ssd1306_draw_string(d, 0, 56, buf, false);
}

void oled_display_next_page(oled_display_t *disp) {
    disp->current_page = (disp->current_page + 1) % disp->num_pages;
}

void oled_display_prev_page(oled_display_t *disp) {
    disp->current_page = (disp->current_page - 1 + disp->num_pages) % disp->num_pages;
}

void oled_display_toggle_graph(oled_display_t *disp) {
    disp->show_graph = !disp->show_graph;
}