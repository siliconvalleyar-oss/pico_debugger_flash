cmake_minimum_required(VERSION 3.13)

# Point to the Raspberry Pi Pico SDK (2 levels up from this project dir).
if (NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../pico-sdk")
endif ()

# Must be included before project()
include(pico_sdk_import.cmake)

project(pico_usb_drive_configurable C CXX ASM)

# The board carries (at least) 16 MB of QSPI flash. This only sizes the linker
# FLASH region; the firmware itself detects the real physical flash at runtime
# (flash_geom_init) and exposes 1.5 MB (2 MB part) or 15 MB (16 MB part).
if (NOT PICO_FLASH_SIZE_BYTES)
    set(PICO_FLASH_SIZE_BYTES 16777216)
endif ()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise the Pico SDK
pico_sdk_init()

# Vendor FatFS (Elm-chan) as a reusable static library
add_subdirectory(lib/fatfs)

# Main application (uses src/ for the firmware)
add_subdirectory(src)
