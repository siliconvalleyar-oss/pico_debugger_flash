cmake_minimum_required(VERSION 3.13)

# =============================================================================
# floppydisk - Lectora/grabadora de disquetes 3.5" basada en Raspberry Pi Pico 2 W
#
#   Funciones:
#     - LEER  un disquete real (interfaz de 34 pines) y guardarlo como imagen
#             .ima en la microSD (FAT32, via SPI)
#     - ESCRIBIR una imagen .ima de la microSD en un disquete real
#
#   Hardware objetivo: Raspberry Pi Pico 2 W (RP2350, 150 MHz, 520 KB SRAM)
#   Disquetera:        interfaz Shugart de 34 pines conectada a GPIO
#   Almacenamiento:    microSD FAT32 por SPI
#   Display:           OLED SSD1306 128x64 por I2C
#   Lenguaje:          C++17
# =============================================================================

if(NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
if(NOT EXISTS "${PICO_SDK_PATH}/pico_sdk_init.cmake")
    # Env var may point at a stale/missing SDK; fall back to a sibling checkout
    # (repo layout: pico/floppydisk, pico/pico-sdk).
    set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/../../pico-sdk")
endif()

if(NOT PICO_BOARD)
    set(PICO_BOARD pico2_w)
endif()

include(pico_sdk_import.cmake)

project(floppydisk C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

add_executable(floppydisk
    main.cpp
    app.cpp
    ssd1306.cpp
    sd_spi.cpp
    fat.cpp
    cfg.cpp
    hfe.cpp
    mfm.cpp
    floppy.cpp
    shell.c
)

pico_generate_pio_header(floppydisk ${CMAKE_CURRENT_LIST_DIR}/floppy.pio)

target_link_libraries(floppydisk
    pico_stdlib
    hardware_i2c
    hardware_spi
    hardware_pio
    hardware_dma
)

pico_enable_stdio_uart(floppydisk 1)
pico_enable_stdio_usb(floppydisk 1)

pico_add_extra_outputs(floppydisk)