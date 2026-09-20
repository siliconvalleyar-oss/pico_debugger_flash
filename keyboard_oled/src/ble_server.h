#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Initialize the BLE server (GATT DB, advertising, HCI handlers) */
void ble_server_init(void);

/* Returns true if a BLE client is connected */
bool ble_server_connected(void);

/* Pop a character from the BLE receive queue. Returns false if queue is empty. */
bool ble_server_get_char(char *c);
