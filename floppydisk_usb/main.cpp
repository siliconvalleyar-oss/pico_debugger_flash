#include "app.h"
#include "shell.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "ssd1306.h"

int main(void) {
    /* Initialize CYW43 (WiFi/BLE chip) first - required for USB on Pico W */
    if (cyw43_arch_init()) {
        // CYW43 init failed, but continue anyway
    }
    
    stdio_init_all();
    
    /* Quick LED blink to show we're alive (only on regular Pico, not Pico W) */
#if defined(PICO_DEFAULT_LED_PIN)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    for (int i = 0; i < 3; i++) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        busy_wait_ms(100);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        busy_wait_ms(100);
    }
#endif
    
    ssd1306_init();
    shell_init();

    while (true) {
        shell_task();
        app_run();
    }
    return 0;
}