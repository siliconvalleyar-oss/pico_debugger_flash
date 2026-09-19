#include "at_commands.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>

/* ================================================================
 * Funciones auxiliares para enviar respuestas por Bluetooth
 * ================================================================ */

static void at_send_ok(at_command_parser_t *parser) {
    bluetooth_send_str(parser->bt, "OK\r\n");
}

static void at_send_error(at_command_parser_t *parser) {
    bluetooth_send_str(parser->bt, "ERROR\r\n");
}

static void at_send_response(at_command_parser_t *parser, const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    bluetooth_send_str(parser->bt, buf);
}

/* ================================================================
 * Comandos AT implementados
 * ================================================================ */

/* AT -> responde OK */
static void cmd_at(at_command_parser_t *parser) {
    at_send_ok(parser);
}

/* ATE0 / ATE1 -> eco off/on */
static void cmd_ate(at_command_parser_t *parser, const char *args) {
    if (strstr(args, "0")) parser->echo = false;
    else if (strstr(args, "1")) parser->echo = true;
    at_send_ok(parser);
}

/* AT+VERSION? -> versión del firmware */
static void cmd_version(at_command_parser_t *parser) {
    at_send_response(parser, "+VERSION:%s (%s %s)\r\n", FW_VERSION, FW_BUILD_DATE, FW_BUILD_TIME);
    at_send_ok(parser);
}

/* AT+ADC? -> valor ADC actual de todos los canales habilitados */
static void cmd_adc_query(at_command_parser_t *parser) {
    adc_monitor_read_all(parser->monitor);
    for (int i = 0; i < parser->monitor->num_channels; i++) {
        adc_channel_t *ch = &parser->monitor->channels[i];
        if (!ch->enabled) continue;
        at_send_response(parser, "ADC%d=%d\r\n", i, ch->raw_value);
        at_send_response(parser, "+ADC%d:%.3fV,raw=%d\r\n", i, ch->voltage, ch->raw_value);
    }
    at_send_ok(parser);
}

/* AT+ADC=<canal> -> selecciona canal y lee su valor */
static void cmd_adc_select(at_command_parser_t *parser, const char *args) {
    int ch_num = atoi(args);
    if (ch_num < 0 || ch_num >= parser->monitor->num_channels) {
        at_send_error(parser);
        return;
    }
    parser->selected_channel = ch_num;
    adc_channel_t *ch = &parser->monitor->channels[ch_num];
    if (!ch->enabled) {
        at_send_error(parser);
        return;
    }
    adc_monitor_read_all(parser->monitor);
    at_send_response(parser, "ADC%d=%d\r\n", ch_num, ch->raw_value);
    at_send_response(parser, "+ADC%d:%.3fV,raw=%d\r\n", ch_num, ch->voltage, ch->raw_value);
    at_send_ok(parser);
}

/* AT+ADCR -> leer todos los canales (alias compatibilidad) */
static void cmd_adc_read_all(at_command_parser_t *parser) {
    cmd_adc_query(parser);
}

/* AT+ADCC -> listar canales configurados */
static void cmd_adc_channels(at_command_parser_t *parser) {
    at_send_response(parser, "+CHANNELS:%d\r\n", parser->monitor->num_channels);
    for (int i = 0; i < parser->monitor->num_channels; i++) {
        adc_channel_t *ch = &parser->monitor->channels[i];
        if (!ch->enabled) continue;
        at_send_response(parser, "+CH%d:GPIO%d,thresh=%d,low=%.2f,high=%.2f\r\n",
            i, ch->channel + 26, ch->change_threshold, ch->threshold_low, ch->threshold_high);
    }
    at_send_ok(parser);
}

/* AT+ADCCFG=<ch>,<low>,<high> -> configurar umbrales de voltaje */
static void cmd_adc_config(at_command_parser_t *parser, const char *args) {
    int ch_num = 0;
    float low = 0, high = 3.3;
    if (sscanf(args, "%d,%f,%f", &ch_num, &low, &high) >= 1) {
        if (ch_num >= 0 && ch_num < parser->monitor->num_channels) {
            adc_channel_t *ch = &parser->monitor->channels[ch_num];
            if (ch->enabled) {
                ch->threshold_low = low;
                ch->threshold_high = high;
                at_send_response(parser, "+ADCCFG:%d,%.2f,%.2f\r\n", ch_num, low, high);
                at_send_ok(parser);
                return;
            }
        }
    }
    at_send_error(parser);
}

/* AT+THRESH=<n> -> umbral de cambio en LSB (default: 16) */
static void cmd_threshold(at_command_parser_t *parser, const char *args) {
    int thresh = atoi(args);
    if (thresh < 0 || thresh > 4095) {
        at_send_error(parser);
        return;
    }
    // Aplicar al canal seleccionado o a todos
    if (parser->selected_channel >= 0 && parser->selected_channel < parser->monitor->num_channels) {
        parser->monitor->channels[parser->selected_channel].change_threshold = (uint16_t)thresh;
        at_send_response(parser, "+THRESH:%d,%d\r\n", parser->selected_channel, thresh);
    } else {
        for (int i = 0; i < parser->monitor->num_channels; i++) {
            if (parser->monitor->channels[i].enabled) {
                parser->monitor->channels[i].change_threshold = (uint16_t)thresh;
            }
        }
        at_send_response(parser, "+THRESH:ALL,%d\r\n", thresh);
    }
    at_send_ok(parser);
}

/* AT+RATE=<ms> -> periodo de muestreo en milisegundos */
static void cmd_rate(at_command_parser_t *parser, const char *args) {
    int rate = atoi(args);
    if (rate < 10 || rate > 60000) {
        at_send_error(parser);
        return;
    }
    parser->sample_rate_ms = (uint32_t)rate;
    at_send_response(parser, "+RATE:%d\r\n", rate);
    at_send_ok(parser);
}

/* AT+OLED=ON / AT+OLED=OFF -> habilita/deshabilita display */
static void cmd_oled(at_command_parser_t *parser, const char *args) {
    if (strncmp(args, "ON", 2) == 0 || strncmp(args, "1", 1) == 0) {
        parser->oled_enabled = true;
        at_send_response(parser, "+OLED:ON\r\n");
        at_send_ok(parser);
    } else if (strncmp(args, "OFF", 3) == 0 || strncmp(args, "0", 1) == 0) {
        parser->oled_enabled = false;
        // Limpiar pantalla al deshabilitar
        if (parser->display && parser->display->display) {
            ssd1306_clear(parser->display->display);
            ssd1306_show(parser->display->display);
        }
        at_send_response(parser, "+OLED:OFF\r\n");
        at_send_ok(parser);
    } else {
        at_send_error(parser);
    }
}

/* AT+DISP=<page> -> cambiar página del OLED */
static void cmd_display_page(at_command_parser_t *parser, const char *args) {
    int page = atoi(args);
    if (page >= 0 && page < parser->display->num_pages) {
        parser->display->current_page = page;
        at_send_response(parser, "+DISP:%d\r\n", page);
        at_send_ok(parser);
        return;
    }
    at_send_error(parser);
}

/* AT+GRAPH=0/1 -> toggle gráfico */
static void cmd_display_graph(at_command_parser_t *parser, const char *args) {
    if (strstr(args, "1") || strstr(args, "ON")) {
        parser->display->show_graph = true;
    } else {
        parser->display->show_graph = false;
    }
    at_send_response(parser, "+GRAPH:%d\r\n", parser->display->show_graph);
    at_send_ok(parser);
}

/* AT+STATUS -> estado completo del sistema */
static void cmd_status(at_command_parser_t *parser) {
    adc_monitor_read_all(parser->monitor);
    at_send_response(parser, "+STATUS:Vref=%.2f,Channels=%d,Rate=%lums,OLED=%s,Uptime=%lus\r\n",
        parser->monitor->vref, parser->monitor->num_channels,
        parser->sample_rate_ms,
        parser->oled_enabled ? "ON" : "OFF",
        to_ms_since_boot(get_absolute_time()) / 1000);
    for (int i = 0; i < parser->monitor->num_channels; i++) {
        adc_channel_t *ch = &parser->monitor->channels[i];
        if (!ch->enabled) continue;
        at_send_response(parser, "+CH%d:V=%.3f,R=%d,Thresh=%d,Min=%.3f,Max=%.3f,Avg=%.3f,%s%s%s\r\n",
            i, ch->voltage, ch->raw_value, ch->change_threshold,
            ch->min_voltage, ch->max_voltage, ch->avg_voltage,
            ch->alert_high ? "HIGH " : "",
            ch->alert_low ? "LOW " : "",
            ch->changed ? "CHANGED " : "");
    }
    at_send_ok(parser);
}

/* AT+RESET -> reinicia configuración a valores por defecto */
static void cmd_reset(at_command_parser_t *parser) {
    // Restaurar valores por defecto
    parser->sample_rate_ms = 100;
    parser->oled_enabled = true;
    parser->selected_channel = -1;
    parser->echo = true;
    for (int i = 0; i < parser->monitor->num_channels; i++) {
        adc_channel_t *ch = &parser->monitor->channels[i];
        if (ch->enabled) {
            ch->change_threshold = 16;
            ch->threshold_low = 0.5f;
            ch->threshold_high = 2.5f;
        }
    }
    at_send_ok(parser);
    sleep_ms(100);
    watchdog_reboot(0, 0, 0);
}

/* AT+HELP -> lista de comandos disponibles */
static void cmd_help(at_command_parser_t *parser) {
    at_send_response(parser, "+HELP:AT Commands v%s\r\n", FW_VERSION);
    at_send_response(parser, "AT                  - Test connection\r\n");
    at_send_response(parser, "AT+VERSION?         - Firmware version\r\n");
    at_send_response(parser, "ATE0/1              - Echo off/on\r\n");
    at_send_response(parser, "AT+ADC?             - Read all ADC channels\r\n");
    at_send_response(parser, "AT+ADC=<ch>         - Read specific channel\r\n");
    at_send_response(parser, "AT+ADCR             - Read all ADC (alias)\r\n");
    at_send_response(parser, "AT+ADCC             - List configured channels\r\n");
    at_send_response(parser, "AT+ADCCFG=<ch>,<lo>,<hi> - Set voltage thresholds\r\n");
    at_send_response(parser, "AT+THRESH=<n>       - Set change threshold (LSB)\r\n");
    at_send_response(parser, "AT+RATE=<ms>        - Set sample period (10-60000)\r\n");
    at_send_response(parser, "AT+OLED=ON|OFF      - Enable/disable OLED display\r\n");
    at_send_response(parser, "AT+DISP=<page>      - Set display page (0-3)\r\n");
    at_send_response(parser, "AT+GRAPH=0|1        - Toggle graph view\r\n");
    at_send_response(parser, "AT+STATUS           - Full system status\r\n");
    at_send_response(parser, "AT+RST              - Reset to defaults\r\n");
    at_send_response(parser, "AT+HELP             - This help\r\n");
    at_send_ok(parser);
}

/* ================================================================
 * Parser principal
 * ================================================================ */

void at_parser_init(at_command_parser_t *parser, bluetooth_t *bt, adc_monitor_t *monitor, oled_display_t *display) {
    memset(parser, 0, sizeof(at_command_parser_t));
    parser->bt = bt;
    parser->monitor = monitor;
    parser->display = display;
    parser->echo = true;
    parser->verbose = true;
    parser->oled_enabled = true;
    parser->sample_rate_ms = 100;
    parser->selected_channel = -1;
}

/* Procesa una línea completa recibida */
static void process_line(at_command_parser_t *parser, char *line) {
    // Eliminar whitespace trailing
    int len = strlen(line);
    while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) {
        line[--len] = '\0';
    }
    if (len == 0) return;

    // Eco si está habilitado
    if (parser->echo) {
        at_send_response(parser, "%s\r\n", line);
    }

    // Copiar a uppercase para matching
    char upper[256];
    strncpy(upper, line, sizeof(upper) - 1);
    upper[sizeof(upper) - 1] = '\0';
    for (int i = 0; upper[i]; i++) upper[i] = toupper(upper[i]);

    // --- Matching de comandos ---
    if (strcmp(upper, "AT") == 0) {
        cmd_at(parser);
    } else if (strncmp(upper, "ATE", 3) == 0) {
        cmd_ate(parser, upper + 3);
    } else if (strcmp(upper, "AT+VERSION?") == 0) {
        cmd_version(parser);
    } else if (strcmp(upper, "AT+ADC?") == 0) {
        cmd_adc_query(parser);
    } else if (strncmp(upper, "AT+ADC=", 7) == 0) {
        cmd_adc_select(parser, upper + 7);
    } else if (strcmp(upper, "AT+ADCR") == 0) {
        cmd_adc_read_all(parser);
    } else if (strcmp(upper, "AT+ADCC") == 0) {
        cmd_adc_channels(parser);
    } else if (strncmp(upper, "AT+ADCCFG", 9) == 0) {
        cmd_adc_config(parser, upper + 10);
    } else if (strncmp(upper, "AT+THRESH=", 10) == 0) {
        cmd_threshold(parser, upper + 10);
    } else if (strncmp(upper, "AT+RATE=", 8) == 0) {
        cmd_rate(parser, upper + 8);
    } else if (strncmp(upper, "AT+OLED=", 8) == 0) {
        cmd_oled(parser, upper + 8);
    } else if (strncmp(upper, "AT+DISP", 7) == 0) {
        cmd_display_page(parser, upper + 8);
    } else if (strncmp(upper, "AT+GRAPH", 8) == 0) {
        cmd_display_graph(parser, upper + 9);
    } else if (strcmp(upper, "AT+STATUS") == 0) {
        cmd_status(parser);
    } else if (strcmp(upper, "AT+RST") == 0) {
        cmd_reset(parser);
    } else if (strcmp(upper, "AT+HELP") == 0) {
        cmd_help(parser);
    } else {
        at_send_error(parser);
    }
}

void at_parser_process(at_command_parser_t *parser) {
    at_parser_handle_uart(parser);
    at_parser_handle_bluetooth(parser);
}

/* Lee caracteres del USB CDC (stdio) */
void at_parser_handle_uart(at_command_parser_t *parser) {
    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (c == '\r' || c == '\n') {
            if (parser->rx_len > 0) {
                parser->rx_buffer[parser->rx_len] = '\0';
                process_line(parser, parser->rx_buffer);
                parser->rx_len = 0;
            }
        } else if (parser->rx_len < 255) {
            parser->rx_buffer[parser->rx_len++] = c;
        }
    }
}

/* Lee caracteres del Bluetooth UART */
void at_parser_handle_bluetooth(at_command_parser_t *parser) {
    bluetooth_process(parser->bt);

    if (parser->bt->rx_len > 0) {
        for (int i = 0; i < parser->bt->rx_len; i++) {
            uint8_t c = parser->bt->rx_buffer[i];
            if (c == '\r' || c == '\n') {
                if (parser->rx_len > 0) {
                    parser->rx_buffer[parser->rx_len] = '\0';
                    process_line(parser, parser->rx_buffer);
                    parser->rx_len = 0;
                }
            } else if (parser->rx_len < 255) {
                parser->rx_buffer[parser->rx_len++] = c;
            }
        }
        parser->bt->rx_len = 0;
    }
}
