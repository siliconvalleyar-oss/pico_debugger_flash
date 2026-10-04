cmake_minimum_required(VERSION 3.13)

include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(blink_locked_oled C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico_w CACHE STRING "Board type")

pico_sdk_init()

set(EXPECTED_BOARD_ID "0000000000000000" CACHE STRING "Expected unique board ID (16 hex chars)")

add_executable(blink_locked_oled
    src/main.cpp
    src/ssd1306.cpp
    ${PICO_TINYUSB_PATH}/src/common/tusb_fifo.c
)

target_compile_definitions(blink_locked_oled PRIVATE
    EXPECTED_BOARD_ID="${EXPECTED_BOARD_ID}"
)

target_link_libraries(blink_locked_oled PRIVATE
    pico_stdlib
    pico_unique_id
    hardware_i2c
    tinyusb_device
)

pico_enable_stdio_usb(blink_locked_oled 1)
pico_enable_stdio_uart(blink_locked_oled 0)

pico_add_extra_outputs(blink_locked_oled)