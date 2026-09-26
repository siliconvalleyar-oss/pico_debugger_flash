#include "app.h"
#include "shell.h"
#include "pico/stdlib.h"
#include "ssd1306.h"
#include "pico/cyw43_arch.h"

int main(void) {
    stdio_init_all();
    
    /* Quick LED blink to show we're alive */
#if defined(CYW43_WL_GPIO_LED_PIN)
    /* Pico W - LED is on wireless chip */
    cyw43_arch_init();
    for (int i = 0; i < 3; i++) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        busy_wait_ms(100);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        busy_wait_ms(100);
    }
#elif defined(PICO_DEFAULT_LED_PIN)
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