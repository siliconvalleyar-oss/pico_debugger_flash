#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/watchdog.h"
#include "tusb.h"

#include "tusb_config.h"
#include "ssd1306.h"
#include "adc_monitor.h"
#include "oled_display.h"
#include "bluetooth.h"
#include "at_commands.h"

/* ================================================================
 * Configuración de pines (ajustar según hardware)
 * ================================================================ */
#define I2C_PORT    i2c0
#define I2C_SDA     4       // GPIO4 = I2C0 SDA
#define I2C_SCL     5       // GPIO5 = I2C0 SCL
#define OLED_ADDR   0x3C

#define ADC_VREF    3.3f

#define BT_UART_TX  0       // GPIO0 = TX hacia HC-05/06
#define BT_UART_RX  1       // GPIO1 = RX desde HC-05/06

#define STATUS_SEND_INTERVAL_MS 5000   // Envío periódico de status por BT

/* ================================================================
 * Función auxiliar: enviar notificación de cambio por BT
 * Formato: "CHANGED:ADC<n>=<raw>,<voltage>V,delta=<lsb>\r\n"
 * Pensado para app Android genérica de terminal BT SPP.
 * ================================================================ */
static void send_change_notification(bluetooth_t *bt, int ch_num, adc_channel_t *ch) {
    char buf[BT_MAX_PAYLOAD];
    snprintf(buf, sizeof(buf), "CHANGED:ADC%d=%d,%.3fV,delta=%d\r\n",
        ch_num, ch->raw_value, ch->voltage, ch->change_delta);
    bluetooth_send_str(bt, buf);
}

/* ================================================================
 * MAIN
 * ================================================================ */
int main() {
    stdio_init_all();
    sleep_ms(2000);

    printf("\n=== Pico ADC Monitor with OLED & Bluetooth ===\n");
    printf("Firmware v%s (Build: %s %s)\n", FW_VERSION, FW_BUILD_DATE, FW_BUILD_TIME);

    char serial[32];
    pico_get_unique_board_id_string(serial, sizeof(serial));
    printf("Device ID: %s\n", serial);

    /* --- Inicializar I2C para OLED --- */
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    /* --- Inicializar OLED --- */
    ssd1306_t oled;
    if (!ssd1306_init(&oled, I2C_PORT, OLED_ADDR, SSD1306_WIDTH, SSD1306_HEIGHT)) {
        printf("OLED init failed!\n");
    } else {
        printf("OLED initialized\n");
        ssd1306_draw_string(&oled, 0, 0, "Pico ADC Monitor", false);
        ssd1306_draw_string(&oled, 0, 10, "FW v" FW_VERSION, false);
        ssd1306_draw_string(&oled, 0, 20, "Starting...", false);
        ssd1306_show(&oled);
    }

    /* --- Inicializar Monitor ADC --- */
    adc_monitor_t adc_mon;
    adc_monitor_init(&adc_mon, ADC_VREF);
    // 4 canales: ADC0 (GPIO26), ADC1 (GPIO27), ADC2 (GPIO28), ADC3 (temp)
    adc_monitor_add_channel(&adc_mon, 0, 0.5f, 2.5f);
    adc_monitor_add_channel(&adc_mon, 1, 0.5f, 2.5f);
    adc_monitor_add_channel(&adc_mon, 2, 0.5f, 2.5f);
    adc_monitor_add_channel(&adc_mon, 3, 0.5f, 2.5f);

    /* --- Inicializar Display OLED --- */
    oled_display_t oled_disp;
    oled_display_init(&oled_disp, &oled, &adc_mon);

    /* --- Inicializar Bluetooth (UART para HC-05/06) --- */
    bluetooth_t bt;
    bluetooth_init(&bt, BT_TYPE_UART);
    bt.tx_pin = BT_UART_TX;
    bt.rx_pin = BT_UART_RX;
    bt.baudrate = 9600;
    bluetooth_start(&bt);

    /* --- Inicializar Parser AT --- */
    at_command_parser_t at_parser;
    at_parser_init(&at_parser, &bt, &adc_mon, &oled_disp);

    /* --- TinyUSB (USB CDC) --- */
    tusb_init();

    printf("System ready!\n");
    printf("AT commands via USB serial or Bluetooth (9600 baud)\n");
    bluetooth_send_status(&bt, "System Ready");

    /* --- Variables de timing --- */
    uint32_t last_sample = 0;
    uint32_t last_status_send = 0;
    uint32_t last_display_update = 0;
    uint32_t last_page_cycle = 0;

    /* ================================================================
     * Bucle principal
     * ================================================================ */
    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // TinyUSB task
        tud_task();

        // Procesar comandos AT (USB serial + Bluetooth)
        at_parser_process(&at_parser);

        // --- Muestreo ADC con periodo configurable ---
        if (now - last_sample >= at_parser.sample_rate_ms) {
            last_sample = now;
            adc_monitor_read_all(&adc_mon);

            // Verificar cambios en cada canal y notificar por BT
            for (int i = 0; i < adc_mon.num_channels; i++) {
                adc_channel_t *ch = &adc_mon.channels[i];
                if (!ch->enabled) continue;
                if (adc_channel_has_changed(ch)) {
                    send_change_notification(&bt, i, ch);
                    // También enviar formato simple para app Android:
                    // "ADC<n>=<raw>\r\n"
                    char simple[64];
                    snprintf(simple, sizeof(simple), "ADC%d=%d\r\n", i, ch->raw_value);
                    bluetooth_send_str(&bt, simple);
                }
            }
        }

        // --- Actualizar OLED (respeta flag de habilitación) ---
        if (at_parser.oled_enabled && (now - last_display_update >= 500)) {
            last_display_update = now;
            oled_display_update(&oled_disp);
        }

        // --- Envío periódico de status por BT ---
        if (now - last_status_send >= STATUS_SEND_INTERVAL_MS) {
            last_status_send = now;
            float values[4];
            int count = 0;
            for (int i = 0; i < adc_mon.num_channels; i++) {
                if (adc_mon.channels[i].enabled) {
                    values[count++] = adc_mon.channels[i].voltage;
                }
            }
            bluetooth_send_adc_data(&bt, values, count, now / 1000);
        }

        // --- Rotación automática de páginas OLED cada 10s ---
        if (now - last_page_cycle >= 10000) {
            last_page_cycle = now;
            if (at_parser.oled_enabled) {
                oled_display_next_page(&oled_disp);
            }
        }

        sleep_ms(10);
    }

    return 0;
}
