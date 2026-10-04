#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/unique_id.h"

int main() {
    stdio_init_all();

    // Wait for USB CDC to connect (up to 5 seconds)
    uint32_t start = time_us_32();
    while (!stdio_usb_connected() && (time_us_32() - start) < 5000000) {
        sleep_ms(100);
    }

    printf("\n========================================\n");
    printf("   Pico Unique Board ID Reader\n");
    printf("========================================\n\n");

    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);

    printf("Board ID (hex): ");
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        printf("%02x", id.id[i]);
    }
    printf("\n\n");

    printf("Board ID (decimal bytes): ");
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        printf("%d ", id.id[i]);
    }
    printf("\n\n");

    printf("Size: %d bytes\n", PICO_UNIQUE_BOARD_ID_SIZE_BYTES);
    printf("========================================\n\n");

    // Keep printing every 5 seconds for easy reading
    while (true) {
        sleep_ms(5000);
        printf("Board ID: ");
        for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
            printf("%02x", id.id[i]);
        }
        printf("\n");
    }

    return 0;
}