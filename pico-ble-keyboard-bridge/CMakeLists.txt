cmake_minimum_required(VERSION 3.13)

# Requiere que la variable de entorno PICO_SDK_PATH apunte al Pico SDK
# (o que pico_sdk_import.cmake esté en este mismo directorio, ver README.md)
include(pico_sdk_import.cmake)

project(pico_ble_keyboard_bridge C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Pico W es necesario porque el chip CYW43439 provee el radio BLE.
# Si usás una Pico normal (RP2040 sin radio) este proyecto NO puede
# generar BLE nativo: necesitarías un módulo BLE externo (HC-08, etc.)
set(PICO_BOARD pico_w CACHE STRING "Board type")

pico_sdk_init()

add_executable(pico_ble_keyboard_bridge
    src/main.cpp
    src/usb_hid_keyboard.cpp
    src/ascii_to_hid.cpp
    src/key_queue.cpp
    src/ble_gatt_server.cpp
    src/usb_descriptors.cpp
)

target_include_directories(pico_ble_keyboard_bridge PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/config
)

# Genera el header .h a partir del archivo .gatt (compilado por BTstack)
pico_btstack_make_gatt_header(pico_ble_keyboard_bridge PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/gatt/pico_kb_bridge.gatt
)

target_link_libraries(pico_ble_keyboard_bridge PRIVATE
    pico_stdlib
    pico_cyw43_arch_none
    tinyusb_device
    tinyusb_board
    pico_btstack_ble
    pico_btstack_cyw43
    pico_multicore
)

# Debug por UART (console stdio): UART0 con GP0=TX / GP1=RX @115200.
# Estos pines son los que el Raspberry Pi Debug Probe expone como CDC-ACM
# en su segundo puerto USB (ver docs/DEBUG_UART.md).
target_compile_definitions(pico_ble_keyboard_bridge PRIVATE
    PICO_DEFAULT_UART=0
    PICO_DEFAULT_UART_TX_PIN=0
    PICO_DEFAULT_UART_RX_PIN=1
    PICO_DEFAULT_UART_BAUD_RATE=115200
)

# Salidas UF2 / ELF / etc.
pico_add_extra_outputs(pico_ble_keyboard_bridge)

# USB stdio deshabilitado (el USB lo maneja TinyUSB en modo HID, no CDC)
pico_enable_stdio_usb(pico_ble_keyboard_bridge 0)
pico_enable_stdio_uart(pico_ble_keyboard_bridge 1)
