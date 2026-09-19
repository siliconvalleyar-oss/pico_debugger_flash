#ifndef ADC_MONITOR_H
#define ADC_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

#define ADC_MAX_CHANNELS 4
#define ADC_HISTORY_SIZE 128

typedef struct {
    uint32_t channel;
    float voltage;
    float min_voltage;
    float max_voltage;
    float avg_voltage;
    uint16_t raw_value;
    uint16_t previous_raw;      // Valor anterior para detectar cambios
    bool first_reading;         // Primera lectura (no hay anterior)
    bool changed;               // Flag: detectó cambio desde última lectura
    int16_t change_delta;       // Diferencia actual - anterior (LSB)
    float history[ADC_HISTORY_SIZE];
    int history_idx;
    int history_count;
    bool enabled;
    float threshold_high;
    float threshold_low;
    bool alert_high;
    bool alert_low;
    uint16_t change_threshold;  // Umbral de cambio en LSB (ej: 16 sobre 12 bits)
} adc_channel_t;

typedef struct {
    adc_channel_t channels[ADC_MAX_CHANNELS];
    int num_channels;
    float vref;
    bool initialized;
} adc_monitor_t;

void adc_monitor_init(adc_monitor_t *mon, float vref);
void adc_monitor_add_channel(adc_monitor_t *mon, uint32_t channel, float threshold_low, float threshold_high);
void adc_monitor_read_all(adc_monitor_t *mon);
float adc_monitor_raw_to_voltage(uint16_t raw, float vref);
void adc_monitor_update_history(adc_channel_t *ch);
bool adc_monitor_check_thresholds(adc_channel_t *ch);
void adc_monitor_print_status(adc_monitor_t *mon);
bool adc_channel_has_changed(adc_channel_t *ch);

#endif