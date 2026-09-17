cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(oled_ssd1306 C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(oled_ssd1306
    main.c
    ssd1306.c
)

pico_enable_stdio_usb(oled_ssd1306 1)
pico_enable_stdio_uart(oled_ssd1306 0)

target_link_libraries(oled_ssd1306 pico_stdlib hardware_i2c)

if (PICO_CYW43_SUPPORTED)
    message(STATUS "Board: Pico W - habilitando driver CYW43 LED")
    target_link_libraries(oled_ssd1306 pico_cyw43_arch_none)
endif()

pico_add_extra_outputs(oled_ssd1306)