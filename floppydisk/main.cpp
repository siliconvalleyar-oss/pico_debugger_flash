#include "app.h"
#include "pico/stdlib.h"
#include "ssd1306.h"

int main(void) {
    stdio_init_all();
    ssd1306_init();
    app_run();
    return 0;
}