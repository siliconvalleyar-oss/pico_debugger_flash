/**
 * @file main.cpp
 * @brief Punto de entrada: inicializa USB HID + BLE GATT y corre el loop.
 *
 * Flujo:
 *   1. Se inicializa el radio CYW43 (necesario tanto para BLE como para
 *      el LED onboard de la Pico W).
 *   2. Se inicializa TinyUSB en modo teclado HID.
 *   3. Se inicializa BTstack + el servidor GATT ("Pico-KB-Bridge").
 *   4. Loop principal: bombea btstack_run_loop y usb_kbd_task().
 *      Nada bloquea: cada iteración es no-bloqueante y cede el control
 *      rápidamente (cooperative multitasking, típico de TinyUSB+BTstack
 *      sobre RP2040 en un único core).
 */

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"
#include <cstdio>

#include "usb_hid_keyboard.h"
#include "ble_gatt_server.h"

int main() {
    stdio_init_all();
    // Console de debug por UART0 (GP0=TX, GP1=RX, 115200): en el banco se
    // monitorea desde el puerto CDC-ACM del Raspberry Pi Debug Probe.
    printf("\n=== Pico-KB-Bridge boot (uart0 GP0/GP1 @115200) ===\n");

    if (cyw43_arch_init()) {
        // Sin el radio CYW43 no hay BLE ni LED: no tiene sentido continuar.
        while (true) {
            tight_loop_contents();
        }
    }

    usb_kbd_init();
    ble_gatt_server_init();
    printf("[main] usb_kbd + ble_gatt_server inicializados\n");

    // btstack_run_loop_execute() es bloqueante en el modelo "estándar" de
    // BTstack, pero en el puerto de Pico (btstack_run_loop_embedded) puede
    // integrarse con un loop propio llamando a btstack_run_loop_poll_data_sources
    // en cada iteración en vez de ceder el control por completo.
    while (true) {
        usb_kbd_task();               // No bloquea: bombea TinyUSB + cola de teclas
        cyw43_arch_poll();            // No bloquea: procesa eventos pendientes del radio/BTstack
        sleep_us(100);                // Cede CPU brevemente, no es una espera activa dura
    }

    return 0;
}
