cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(bluetooth_adc C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Define board (pico_w for Bluetooth, pico for UART-only)
set(PICO_BOARD "${PICO_BOARD}" CACHE STRING "Board type: pico or pico_w")
if(NOT PICO_BOARD)
    set(PICO_BOARD "pico_w")
endif()

pico_sdk_init()

add_executable(bluetooth_adc
    main.c
    adc_monitor.c
    oled_display.c
    bluetooth.c
    at_commands.c
)

target_include_directories(bluetooth_adc PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

# Core libraries
target_link_libraries(bluetooth_adc PUBLIC
    pico_stdlib
    pico_unique_id
    hardware_adc
    hardware_i2c
    hardware_gpio
)

# TinyUSB for potential USB CDC
target_link_libraries(bluetooth_adc PUBLIC
    tinyusb_device
    tinyusb_board
)

# Bluetooth/CYW43 for Pico W
if(PICO_BOARD STREQUAL "pico_w" OR PICO_BOARD STREQUAL "pico2_w")
    target_link_libraries(bluetooth_adc PUBLIC
        pico_cyw43_arch_lwip_threadsafe_background
        btstack
    )
    target_compile_definitions(bluetooth_adc PUBLIC PICO_CYW43_SUPPORTED=1)
endif()

# SSD1306 library (I2C)
target_link_libraries(bluetooth_adc PUBLIC
    pico_ssd1306
)

pico_add_extra_outputs(bluetooth_adc)