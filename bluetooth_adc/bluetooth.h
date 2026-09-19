#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stdbool.h>
#include <stdint.h>

#define BT_MAX_PAYLOAD 256
#define BT_DEVICE_NAME "Pico_ADC_Monitor"

typedef enum {
    BT_TYPE_NONE = 0,
    BT_TYPE_BLE,      // Pico W built-in BLE
    BT_TYPE_UART      // External module (HC-05/06)
} bt_type_t;

typedef struct {
    bt_type_t type;
    bool connected;
    bool initialized;
    // BLE specific
    void *ble_context;
    // UART specific
    int uart_id;
    uint32_t tx_pin;
    uint32_t rx_pin;
    uint32_t baudrate;
    // Common
    uint8_t rx_buffer[BT_MAX_PAYLOAD];
    int rx_len;
    uint32_t last_tx;
    char device_name[32];
} bluetooth_t;

void bluetooth_init(bluetooth_t *bt, bt_type_t type);
void bluetooth_deinit(bluetooth_t *bt);
bool bluetooth_start(bluetooth_t *bt);
void bluetooth_stop(bluetooth_t *bt);
bool bluetooth_is_connected(bluetooth_t *bt);
int bluetooth_send(bluetooth_t *bt, const uint8_t *data, int len);
int bluetooth_send_str(bluetooth_t *bt, const char *str);
int bluetooth_receive(bluetooth_t *bt, uint8_t *buffer, int max_len);
void bluetooth_process(bluetooth_t *bt);
void bluetooth_send_adc_data(bluetooth_t *bt, float *values, int count, uint32_t timestamp);
void bluetooth_send_status(bluetooth_t *bt, const char *status);

#endif