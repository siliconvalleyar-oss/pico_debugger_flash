#include "adc_monitor.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <stdio.h>
#include <math.h>

void adc_monitor_init(adc_monitor_t *mon, float vref) {
    adc_init();
    adc_set_temp_sensor_enabled(true);
    mon->vref = vref;
    mon->num_channels = 0;
    mon->initialized = true;
    for (int i = 0; i < ADC_MAX_CHANNELS; i++) {
        mon->channels[i].enabled = false;
        mon->channels[i].history_idx = 0;
        mon->channels[i].history_count = 0;
        mon->channels[i].min_voltage = 3.3f;
        mon->channels[i].max_voltage = 0.0f;
        mon->channels[i].alert_high = false;
        mon->channels[i].alert_low = false;
    }
}

void adc_monitor_add_channel(adc_monitor_t *mon, uint channel, float threshold_low, float threshold_high) {
    if (mon->num_channels >= ADC_MAX_CHANNELS || channel > 3) return;
    adc_channel_t *ch = &mon->channels[mon->num_channels];
    ch->channel = channel;
    ch->threshold_low = threshold_low;
    ch->threshold_high = threshold_high;
    ch->enabled = true;
    ch->min_voltage = 3.3f;
    ch->max_voltage = 0.0f;
    ch->history_idx = 0;
    ch->history_count = 0;
    ch->previous_raw = 0;
    ch->first_reading = true;
    ch->changed = false;
    ch->change_delta = 0;
    ch->change_threshold = 16;  // Umbral por defecto: 16 LSB sobre 12 bits
    mon->num_channels++;
}

float adc_monitor_raw_to_voltage(uint16_t raw, float vref) {
    return (raw * vref) / 4095.0f;
}

void adc_monitor_read_all(adc_monitor_t *mon) {
    for (int i = 0; i < mon->num_channels; i++) {
        adc_channel_t *ch = &mon->channels[i];
        if (!ch->enabled) continue;
        
        adc_select_input(ch->channel);
        uint16_t raw = adc_read();
        ch->raw_value = raw;
        ch->voltage = adc_monitor_raw_to_voltage(raw, mon->vref);
        
        if (ch->voltage < ch->min_voltage) ch->min_voltage = ch->voltage;
        if (ch->voltage > ch->max_voltage) ch->max_voltage = ch->voltage;
        
        // Detección de cambio: comparar con lectura anterior
        ch->changed = false;
        if (ch->first_reading) {
            ch->previous_raw = raw;
            ch->first_reading = false;
            ch->change_delta = 0;
        } else {
            ch->change_delta = (int16_t)raw - (int16_t)ch->previous_raw;
            if (ch->change_delta < 0) ch->change_delta = -ch->change_delta;
            if ((uint16_t)ch->change_delta >= ch->change_threshold) {
                ch->changed = true;
            }
            ch->previous_raw = raw;
        }
        
        adc_monitor_update_history(ch);
        adc_monitor_check_thresholds(ch);
    }
}

void adc_monitor_update_history(adc_channel_t *ch) {
    ch->history[ch->history_idx] = ch->voltage;
    ch->history_idx = (ch->history_idx + 1) % ADC_HISTORY_SIZE;
    if (ch->history_count < ADC_HISTORY_SIZE) ch->history_count++;
    
    float sum = 0;
    for (int i = 0; i < ch->history_count; i++) sum += ch->history[i];
    ch->avg_voltage = sum / ch->history_count;
}

bool adc_monitor_check_thresholds(adc_channel_t *ch) {
    bool alert = false;
    if (ch->voltage > ch->threshold_high) {
        ch->alert_high = true;
        alert = true;
    } else {
        ch->alert_high = false;
    }
    if (ch->voltage < ch->threshold_low) {
        ch->alert_low = true;
        alert = true;
    } else {
        ch->alert_low = false;
    }
    return alert;
}

bool adc_channel_has_changed(adc_channel_t *ch) {
    return ch->changed;
}

void adc_monitor_print_status(adc_monitor_t *mon) {
    printf("\n=== ADC Monitor Status ===\n");
    for (int i = 0; i < mon->num_channels; i++) {
        adc_channel_t *ch = &mon->channels[i];
        if (!ch->enabled) continue;
        printf("CH%d (GPIO%d): %.3fV (raw=%d) | Min:%.3f Max:%.3f Avg:%.3f | Thresh=%d LSB | %s%s%s\n",
            i, ch->channel + 26, ch->voltage, ch->raw_value,
            ch->min_voltage, ch->max_voltage, ch->avg_voltage,
            ch->change_threshold,
            ch->alert_high ? "[HIGH] " : "",
            ch->alert_low ? "[LOW] " : "",
            ch->changed ? "[CHANGED] " : "");
    }
}