#pragma once
#include <stdint.h>
#include <stdbool.h>

// Servidor GATT "doorbell": servicio 0xFFE0, char 0xFFE1 Write. Copia el doorbell.gatt
// del SDK (EOF doorbell) -> sin inventar UUIDs. stdout va por USB CDC si conectado.
bool ble_server_init(void);
bool ble_server_connected(void);
// Maneja la escritura de texto desde el movil: encola en kb_bridge.
void ble_server_on_write(const char *buf, uint16_t lenpw);
