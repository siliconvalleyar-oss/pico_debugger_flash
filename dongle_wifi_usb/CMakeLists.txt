cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(dongle_wifi_usb C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico_w CACHE STRING "Board type")

pico_sdk_init()

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

set(SOURCES
    src/main.c
    src/nat.c
    src/dhcp_server.c
    src/usb_netif.c
    src/wifi_ap.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    pico_stdlib
    pico_cyw43_arch
    pico_lwip
    tinyusb_device
    tinyusb_board
    hardware_pio
    hardware_dma
    hardware_irq
    hardware_timer
    hardware_watchdog
    pico_multicore
    pico_sync
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)