#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"

#define LED_PIN 25
#define BLINK_MS 250

int main() {
    stdio_init_all();
    sleep_ms(1000);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    printf("\r\n=== RP2040 SELF-TEST ===\r\n");
    printf("CPU clock: %lu MHz\r\n", clock_get_hz(clk_sys) / 1000000);
    printf("SDK version: %s\r\n",
#ifdef PICO_SDK_VERSION_STRING
           PICO_SDK_VERSION_STRING
#else
           "unknown"
#endif
    );
    printf("LED blink test started...\r\n");

    uint16_t blinks = 0;
    while (true) {
        gpio_put(LED_PIN, 1);
        sleep_ms(BLINK_MS);
        gpio_put(LED_PIN, 0);
        sleep_ms(BLINK_MS);
        blinks++;
        printf("blink #%u\r\n", blinks);
    }
}