#include "app.h"
#include "shell.h"
#include "pico/stdlib.h"
#include "ssd1306.h"

int main(void) {
    stdio_init_all();
    ssd1306_init();
    shell_init();

    while (true) {
        shell_task();
        app_run();
    }
    return 0;
}