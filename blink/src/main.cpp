#include <stdio.h>
#include "pico/stdlib.h"

#ifndef LED_PIN
#define LED_PIN 25
#endif

int main() {
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    printf("Blink LED on GPIO %d every 500ms\n", LED_PIN);

    while (true) {
        gpio_put(LED_PIN, 1);
        sleep_ms(500);
        gpio_put(LED_PIN, 0);
        sleep_ms(500);
    }

    return 0;
}