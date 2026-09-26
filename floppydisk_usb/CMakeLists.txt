cmake_minimum_required(VERSION 3.13)

# =============================================================================
# floppydisk_usb - Lectora/grabadora de disquetes 3.5" con USB/UART shell
#                  basada en Raspberry Pi Pico (RP2040)
#
#   Funciones:
#     - LEER  un disquete real (interfaz de 34 pines) y guardarlo como imagen
#             .ima en la microSD (FAT32, via SPI)
#     - ESCRIBIR una imagen .ima de la microSD en un disquete real
#     - SHELL  USB/UART interactivo para debug y comandos
#
#   Hardware objetivo: Raspberry Pi Pico (RP2040, 133 MHz, 264 KB SRAM)
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
    set(PICO_BOARD pico)
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

# Set variables for configure_file template
set(FLOPPYDISK_VERSION "${GIT_VERSION}")
set(FLOPPYDISK_BUILD ${BUILD_NUMBER})
set(FLOPPYDISK_COMMIT "${GIT_COMMIT}")

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

    # USB device descriptors
    USBD_VID=0xCAFE
    USBD_PID=0x0004
    USBD_MANUFACTURER="Raspberry Pico inc"
    USBD_PRODUCT="Floppy disk USB"
)

# Link minimal libraries, avoid pico_runtime/pico_stdio which pull in pico_cyw43_arch
# Use only the bare minimum libraries we actually need
target_link_libraries(floppydisk_usb
    pico_runtime
    pico_sync
    hardware_i2c
    hardware_spi
    hardware_pio
    hardware_dma
    hardware_gpio
    hardware_uart
    hardware_timer
    hardware_irq
    hardware_claim
    hardware_watchdog
    hardware_flash
    pico_malloc
    pico_printf
    pico_time
    pico_util
    tinyusb
    hardware_watchdog
)

# Remove pico_stdio and pico_stdio_usb which pull in pico_cyw43_arch
if(TARGET pico_stdio)
    target_link_libraries(floppydisk_usb PRIVATE -lpico_stdio)
endif()
if(TARGET pico_stdio_usb)
    target_link_libraries(floppydisk_usb PRIVATE -lpico_stdio_usb)
endif()
if(TARGET pico_cyw43_arch)
    target_link_libraries(floppydisk_usb PRIVATE -lpico_cyw43_arch)
endif()

# Also use set_target_properties to remove from LINK_LIBRARIES
get_target_property(LIBS floppydisk_usb LINK_LIBRARIES)
if(LIBS)
    string(REPLACE "pico_stdio_usb" "" NEW_LIBS "${LIBS}")
    string(REPLACE "pico_cyw43_arch" "" NEW_LIBS "${NEW_LIBS}")
    string(REPLACE "pico_stdio" "" NEW_LIBS "${NEW_LIBS}")
    set_target_properties(floppydisk_usb PROPERTIES LINK_LIBRARIES "${NEW_LIBS}")
endif()

pico_enable_stdio_uart(floppydisk_usb 1)

pico_add_extra_outputs(floppydisk_usb)