#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "ssd1306.h"

#ifndef LED_PIN
#define LED_PIN 25
#endif

#ifndef EXPECTED_BOARD_ID
#error "EXPECTED_BOARD_ID must be defined at compile time"
#endif

void board_id_to_hex(const pico_unique_board_id_t *id, char *hex_str) {
    const uint8_t *bytes = id->id;
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        sprintf(hex_str + i * 2, "%02x", bytes[i]);
    }
    hex_str[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2] = '\0';
}

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

void show_id_on_oled(ssd1306_t *display, const char *current_hex, const char *expected_hex, bool match) {
    ssd1306_clear(display);
    
    ssd1306_draw_string(display, 0, 0, "Blink Locked OLED", true);
    ssd1306_draw_line(display, 0, 9, 127, 9, true);
    
    char line[22];
    snprintf(line, sizeof(line), "ID: %.16s", current_hex);
    ssd1306_draw_string(display, 0, 11, line, true);
    
    snprintf(line, sizeof(line), "Exp: %.16s", expected_hex);
    ssd1306_draw_string(display, 0, 20, line, true);
    
    ssd1306_draw_line(display, 0, 29, 127, 29, true);
    
    if (match) {
        ssd1306_draw_string(display, 0, 31, "STATUS: UNLOCKED", true);
        ssd1306_draw_string(display, 0, 40, "Blinking 500ms", true);
    } else {
        ssd1306_draw_string(display, 0, 31, "STATUS: LOCKED", true);
        ssd1306_draw_string(display, 0, 40, "Wrong Pico!", true);
    }
    
    ssd1306_draw_line(display, 0, 50, 127, 50, true);
    ssd1306_draw_string(display, 0, 52, "GP25=LED GP4/5=I2C", true);
    
    ssd1306_update(display);
}

int main() {
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    ssd1306_t display;
    bool oled_ok = ssd1306_init(&display);
    if (!oled_ok) {
        printf("OLED init failed\n");
    }

    pico_unique_board_id_t current_id;
    pico_get_unique_board_id(&current_id);

    char current_hex[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    const uint8_t *bytes = current_id.id;
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        sprintf(current_hex + i * 2, "%02x", bytes[i]);
    }
    current_hex[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2] = '\0';

    uint8_t expected_bytes[PICO_UNIQUE_BOARD_ID_SIZE_BYTES];
    if (!parse_expected_id(EXPECTED_BOARD_ID, expected_bytes)) {
        printf("ERROR: Invalid EXPECTED_BOARD_ID format\n");
        if (oled_ok) {
            ssd1306_clear(&display);
            ssd1306_draw_string(&display, 0, 0, "CONFIG ERROR", true);
            ssd1306_draw_string(&display, 0, 10, "Bad EXPECTED_ID", true);
            ssd1306_update(&display);
        }
        while (true) {
            gpio_put(LED_PIN, 1);
            sleep_ms(100);
            gpio_put(LED_PIN, 0);
            sleep_ms(100);
        }
    }

    bool match = (memcmp(current_id.id, expected_bytes, PICO_UNIQUE_BOARD_ID_SIZE_BYTES) == 0);

    printf("Board ID: %s\n", current_hex);
    printf("Expected: %s\n", EXPECTED_BOARD_ID);
    printf("Match: %s\n", match ? "YES" : "NO");

    if (oled_ok) {
        show_id_on_oled(&display, current_hex, EXPECTED_BOARD_ID, match);
    }

    if (!match) {
        printf("LOCKED: This firmware only runs on the original Pico\n");
        while (true) {
            gpio_put(LED_PIN, 1);
            sleep_ms(100);
            gpio_put(LED_PIN, 0);
            sleep_ms(100);
            if (oled_ok) {
                ssd1306_invert(&display, true);
                sleep_ms(100);
                ssd1306_invert(&display, false);
            }
        }
    }

    printf("UNLOCKED: Blinking on GPIO %d every 500ms\n", LED_PIN);

    while (true) {
        gpio_put(LED_PIN, 1);
        sleep_ms(500);
        gpio_put(LED_PIN, 0);
        sleep_ms(500);
    }

    return 0;
}