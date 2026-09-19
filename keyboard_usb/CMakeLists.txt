cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(keyboard_usb C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(keyboard_usb
    keyboard_usb.c
)

# Make sure TinyUSB can find tusb_config.h
target_include_directories(keyboard_usb PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(keyboard_usb PUBLIC pico_stdlib tinyusb_device tinyusb_board)

pico_add_extra_outputs(keyboard_usb)