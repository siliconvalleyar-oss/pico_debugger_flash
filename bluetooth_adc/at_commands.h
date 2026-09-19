#ifndef AT_COMMANDS_H
#define AT_COMMANDS_H

#include "bluetooth.h"
#include "adc_monitor.h"
#include "oled_display.h"
#include <stdbool.h>

#define FW_VERSION "1.0.0"
#define FW_BUILD_DATE __DATE__
#define FW_BUILD_TIME __TIME__

typedef struct {
    bluetooth_t *bt;
    adc_monitor_t *monitor;
    oled_display_t *display;
    char rx_buffer[256];
    int rx_len;
    bool echo;
    bool verbose;
    bool oled_enabled;          // Habilita/deshabilita display OLED
    uint32_t sample_rate_ms;    // Periodo de muestreo en ms
    int selected_channel;       // Canal seleccionado para lectura individual
} at_command_parser_t;

void at_parser_init(at_command_parser_t *parser, bluetooth_t *bt, adc_monitor_t *monitor, oled_display_t *display);
void at_parser_process(at_command_parser_t *parser);
void at_parser_handle_uart(at_command_parser_t *parser);
void at_parser_handle_bluetooth(at_command_parser_t *parser);

#endif