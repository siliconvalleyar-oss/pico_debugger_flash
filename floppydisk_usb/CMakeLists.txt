cmake_minimum_required(VERSION 3.13)

# =============================================================================
# floppydisk_usb - Lectora/grabadora de disquetes 3.5" con USB/UART shell
#                  basada en Raspberry Pi Pico 2 W
#
#   Funciones:
#     - LEER  un disquete real (interfaz de 34 pines) y guardarlo como imagen
#             .ima en la microSD (FAT32, via SPI)
#     - ESCRIBIR una imagen .ima de la microSD en un disquete real
#     - SHELL  USB/UART interactivo para debug y comandos
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
    set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/../../pico-sdk")
endif()

if(NOT PICO_BOARD)
    set(PICO_BOARD pico2_w)
endif()

include(pico_sdk_import.cmake)

project(floppydisk_usb C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

# ---- Version from git tag / build number ----
set(VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/version.h")
set(VERSION_H_IN "${CMAKE_CURRENT_LIST_DIR}/version.h.in")

# Get git tag
execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Get commit count as build number
execute_process(
    COMMAND git rev-list --count HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    OUTPUT_VARIABLE BUILD_NUMBER
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Get short commit hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_VERSION)
    set(GIT_VERSION "v0.0.0")
endif()
if(NOT BUILD_NUMBER)
    set(BUILD_NUMBER "0")
endif()
if(NOT GIT_COMMIT)
    set(GIT_COMMIT "unknown")
endif()

# Generate version.h from template
configure_file(${VERSION_H_IN} ${VERSION_FILE} @ONLY)

add_executable(floppydisk_usb
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

pico_generate_pio_header(floppydisk_usb ${CMAKE_CURRENT_LIST_DIR}/floppy.pio)

# Make version.h available
target_include_directories(floppydisk_usb PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Embed version into binary
target_compile_definitions(floppydisk_usb PRIVATE
    FLOPPYDISK_VERSION="${GIT_VERSION}"
    FLOPPYDISK_BUILD=${BUILD_NUMBER}
    FLOPPYDISK_COMMIT="${GIT_COMMIT}"
)

target_link_libraries(floppydisk_usb
    pico_stdlib
    hardware_i2c
    hardware_spi
    hardware_pio
    hardware_dma
)

pico_enable_stdio_uart(floppydisk_usb 1)
pico_enable_stdio_usb(floppydisk_usb 1)

pico_add_extra_outputs(floppydisk_usb)