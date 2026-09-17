#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#define BLINK_MS 250

static void led_set(bool on) {
#ifdef CYW43_WL_GPIO_LED_PIN
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
#elif defined(PICO_DEFAULT_LED_PIN)
    gpio_put(PICO_DEFAULT_LED_PIN, on);
#else
    (void)on;
#endif
}

int main() {
    stdio_init_all();
    sleep_ms(1000);

#ifdef CYW43_WL_GPIO_LED_PIN
    if (cyw43_arch_init() != 0) {
        printf("Error: cyw43_arch_init fallo\r\n");
        while (true) {
            tight_loop_contents();
        }
    }
    printf("LED: CYW43_WL_GPIO_LED_PIN (%d)\r\n", CYW43_WL_GPIO_LED_PIN);
#elif defined(PICO_DEFAULT_LED_PIN)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    printf("LED: PICO_DEFAULT_LED_PIN (GPIO%d)\r\n", PICO_DEFAULT_LED_PIN);
#else
    printf("LED: no definido en esta placa\r\n");
#endif

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
        led_set(true);
        sleep_ms(BLINK_MS);
        led_set(false);
        sleep_ms(BLINK_MS);
        blinks++;
        printf("blink #%u\r\n", blinks);
    }
}