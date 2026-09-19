#include "bluetooth.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <string.h>
#include <stdio.h>

void bluetooth_init(bluetooth_t *bt, bt_type_t type) {
    memset(bt, 0, sizeof(bluetooth_t));
    bt->type = type;
    bt->connected = false;
    bt->initialized = false;
    bt->rx_len = 0;
    strncpy(bt->device_name, BT_DEVICE_NAME, sizeof(bt->device_name) - 1);

    if (type == BT_TYPE_UART) {
        bt->uart_id = 0;
        bt->tx_pin = 0;
        bt->rx_pin = 1;
        bt->baudrate = 9600;
    }
}

void bluetooth_deinit(bluetooth_t *bt) {
    if (bt->type == BT_TYPE_UART && bt->initialized) {
        uart_deinit(uart0);
    }
    bt->initialized = false;
    bt->connected = false;
}

bool bluetooth_start(bluetooth_t *bt) {
    if (bt->type == BT_TYPE_UART) {
        uart_init(uart0, bt->baudrate);
        gpio_set_function(bt->tx_pin, GPIO_FUNC_UART);
        gpio_set_function(bt->rx_pin, GPIO_FUNC_UART);
        uart_set_hw_flow(uart0, false, false);
        uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
        bt->initialized = true;
        bt->connected = true;
        return true;
    } else if (bt->type == BT_TYPE_BLE) {
        // BLE initialization would go here for Pico W
        // Requires cyw43_arch_init() and btstack setup
        bt->initialized = true;
        return true;
    }
    return false;
}

void bluetooth_stop(bluetooth_t *bt) {
    bluetooth_deinit(bt);
}

bool bluetooth_is_connected(bluetooth_t *bt) {
    return bt->connected;
}

int bluetooth_send(bluetooth_t *bt, const uint8_t *data, int len) {
    if (!bt->connected || !bt->initialized) return -1;
    
    if (bt->type == BT_TYPE_UART) {
        uart_write_blocking(uart0, data, len);
        return len;
    }
    return 0;
}

int bluetooth_send_str(bluetooth_t *bt, const char *str) {
    return bluetooth_send(bt, (const uint8_t *)str, strlen(str));
}

int bluetooth_receive(bluetooth_t *bt, uint8_t *buffer, int max_len) {
    if (!bt->connected || !bt->initialized) return 0;
    
    if (bt->type == BT_TYPE_UART) {
        int count = 0;
        while (uart_is_readable(uart0) && count < max_len) {
            buffer[count++] = uart_getc(uart0);
        }
        return count;
    }
    return 0;
}

void bluetooth_process(bluetooth_t *bt) {
    if (!bt->initialized) return;
    
    if (bt->type == BT_TYPE_UART) {
        while (uart_is_readable(uart0)) {
            uint8_t c = uart_getc(uart0);
            if (bt->rx_len < BT_MAX_PAYLOAD - 1) {
                bt->rx_buffer[bt->rx_len++] = c;
            }
        }
    }
}

void bluetooth_send_adc_data(bluetooth_t *bt, float *values, int count, uint32_t timestamp) {
    char buf[BT_MAX_PAYLOAD];
    int offset = 0;
    offset += snprintf(buf + offset, sizeof(buf) - offset, "ADC,%lu", timestamp);
    for (int i = 0; i < count; i++) {
        offset += snprintf(buf + offset, sizeof(buf) - offset, ",%.3f", values[i]);
    }
    offset += snprintf(buf + offset, sizeof(buf) - offset, "\n");
    bluetooth_send_str(bt, buf);
}

void bluetooth_send_status(bluetooth_t *bt, const char *status) {
    char buf[BT_MAX_PAYLOAD];
    snprintf(buf, sizeof(buf), "STATUS,%s\n", status);
    bluetooth_send_str(bt, buf);
}