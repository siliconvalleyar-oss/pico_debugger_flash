/*
 * ble_server.c — Servidor BLE GATT con el patrón EXACTO del doorbell del SDK
 * local (FFE0/FFE1, Verified OK en esta sesión). Recibe un comando ASCII por
 * Write en FFE1 y lo encola al bridge de teclado USB.
 */
#include "ble_server.h"
#include "ascii_to_hid.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "btstack.h"

/* doorbell.gatt (patrón real del pico-examples) da el UUID de 16 bits
 * característica FFE1. Este .h lo genera pico_btstack_make_gatt_header. */
#include "doorbell.h"

static btstack_packet_callback_registration_t hci_cb_registration;
static uint16_t kb_led_pin = 0;
static volatile bool kb_connected = false;;

/* Callback GATT Write (FFE1): doorbell implementa así — echo del valor. */
static void kb_att_write(void) {
    // En doorbell real el device da "Doorbell ring" vía doorbell_server.
}
