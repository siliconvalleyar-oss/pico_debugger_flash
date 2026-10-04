#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/unique_id.h"

#ifndef LED_PIN
#define LED_PIN 25
#endif

#ifndef EXPECTED_BOARD_ID
#error "EXPECTED_BOARD_ID must be defined at compile time"
#endif

// Convert 8-byte board ID to hex string
void board_id_to_hex(const pico_unique_board_id_t *id, char *hex_str) {
    const uint8_t *bytes = id->id;
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        sprintf(hex_str + i * 2, "%02x", bytes[i]);
    }
    hex_str[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2] = '\0';
}

// Parse compile-time expected ID (16 hex chars = 8 bytes)
bool parse_expected_id(const char *hex_str, uint8_t *out_bytes) {
    if (strlen(hex_str) != PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2) {
        return false;
    }
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        char byte_str[3] = { hex_str[i * 2], hex_str[i * 2 + 1], '\0' };
        char *endptr;
        unsigned long val = strtoul(byte_str, &endptr, 16);
        if (*endptr != '\0' || val > 255) {
            return false;
        }
        out_bytes[i] = (uint8_t)val;
    }
    return true;
}

int main() {
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Get this Pico's unique board ID
    pico_unique_board_id_t current_id;
    pico_get_unique_board_id(&current_id);

    // Convert to hex for display/comparison
    char current_hex[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    board_id_to_hex(&current_id, current_hex);

    // Parse expected ID from compile-time definition
    uint8_t expected_bytes[PICO_UNIQUE_BOARD_ID_SIZE_BYTES];
    if (!parse_expected_id(EXPECTED_BOARD_ID, expected_bytes)) {
        printf("ERROR: Invalid EXPECTED_BOARD_ID format\n");
        // Fast blink = config error
        while (true) {
            gpio_put(LED_PIN, 1);
            sleep_ms(100);
            gpio_put(LED_PIN, 0);
            sleep_ms(100);
        }
    }

    // Compare current ID with expected
    bool match = (memcmp(current_id.id, expected_bytes, PICO_UNIQUE_BOARD_ID_SIZE_BYTES) == 0);

    printf("Board ID: %s\n", current_hex);
    printf("Expected: %s\n", EXPECTED_BOARD_ID);
    printf("Match: %s\n", match ? "YES" : "NO");

    if (!match) {
        printf("LOCKED: This firmware only runs on the original Pico\n");
        // Fast blink = locked
        while (true) {
            gpio_put(LED_PIN, 1);
            sleep_ms(100);
            gpio_put(LED_PIN, 0);
            sleep_ms(100);
        }
    }

    printf("UNLOCKED: Blinking on GPIO %d every 500ms\n", LED_PIN);

    // Normal blink = unlocked
    while (true) {
        gpio_put(LED_PIN, 1);
        sleep_ms(500);
        gpio_put(LED_PIN, 0);
        sleep_ms(500);
    }

    return 0;
}