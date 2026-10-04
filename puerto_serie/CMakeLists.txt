cmake_minimum_required(VERSION 3.13)

include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(puerto_serie C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico_w CACHE STRING "Board type")

pico_sdk_init()

add_executable(puerto_serie
    src/main.cpp
)

target_link_libraries(puerto_serie PRIVATE
    pico_stdlib
    pico_unique_id
)

pico_enable_stdio_usb(puerto_serie 1)
pico_enable_stdio_uart(puerto_serie 0)

pico_add_extra_outputs(puerto_serie)