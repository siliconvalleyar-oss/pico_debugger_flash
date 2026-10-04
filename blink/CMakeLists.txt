cmake_minimum_required(VERSION 3.13)

# Include pico-sdk init from the provided PICO_SDK_PATH
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(blink C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico_w CACHE STRING "Board type")

pico_sdk_init()

add_executable(blink
    src/main.cpp
)

target_link_libraries(blink PRIVATE
    pico_stdlib
)

pico_enable_stdio_usb(blink 1)
pico_enable_stdio_uart(blink 0)

pico_add_extra_outputs(blink)