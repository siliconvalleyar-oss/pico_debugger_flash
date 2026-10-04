cmake_minimum_required(VERSION 3.13)

include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(blink_locked C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico_w CACHE STRING "Board type")

pico_sdk_init()

# Unique board ID for this specific Pico (replace with actual ID)
# Get it by running pico_get_unique_board_id() once and hardcoding it here
# Format: 8 bytes = 16 hex chars
set(EXPECTED_BOARD_ID "0000000000000000" CACHE STRING "Expected unique board ID (16 hex chars)")

add_executable(blink_locked
    src/main.cpp
)

target_compile_definitions(blink_locked PRIVATE
    EXPECTED_BOARD_ID="${EXPECTED_BOARD_ID}"
)

target_link_libraries(blink_locked PRIVATE
    pico_stdlib
    pico_unique_id
)

pico_enable_stdio_usb(blink_locked 1)
pico_enable_stdio_uart(blink_locked 0)

pico_add_extra_outputs(blink_locked)