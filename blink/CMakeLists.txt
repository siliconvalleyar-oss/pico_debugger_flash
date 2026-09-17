cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(blink C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(blink
    blink.c
)

pico_enable_stdio_usb(blink 1)
pico_enable_stdio_uart(blink 0)

target_link_libraries(blink pico_stdlib)

if (PICO_CYW43_SUPPORTED)
    message(STATUS "Board: Pico W - habilitando driver CYW43 LED")
    target_link_libraries(blink pico_cyw43_arch_none)
endif()

pico_add_extra_outputs(blink)