# keyboard/src/CMakeLists.txt — pico_keyboard_bridge: PUENTE BLE "doorbell"
# (GATT FFE0/FFE1, doorbell.gatt 1:1 del SDK Verified) -> USB HID keyboard
# (TinyUSB 6KRO). Patrón EXACTO del SDK ble_doorbell: UN SOLO executable.

add_executable(pico_keyboard_bridge
    main.c
    ble_server.c
    ascii_to_hid.cpp
    usb_kbd.c
    )
target_link_libraries(pico_keyboard_bridge
    pico_stdlib
    pico_btstack_ble
    pico_btstack_cyw43
    pico_cyw43_arch_none
    tinyusb_device
    )
target_include_directories(pico_keyboard_bridge PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../config
    ${CMAKE_CURRENT_LIST_DIR}/../BTstack
    ${CMAKE_CURRENT_BINARY_DIR}
    )
pico_btstack_make_gatt_header(pico_keyboard_bridge PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/doorbell.gatt
    )
# Console stdio por UART0 (GP0=TX, GP1=RX, 115200): se monitorea desde el
# puerto CDC-ACM del Raspberry Pi Debug Probe (ver docs/DEBUG_UART.md).
target_compile_definitions(pico_keyboard_bridge PRIVATE
    PICO_DEFAULT_UART=0
    PICO_DEFAULT_UART_TX_PIN=0
    PICO_DEFAULT_UART_RX_PIN=1
    PICO_DEFAULT_UART_BAUD_RATE=115200
)
pico_enable_stdio_usb(pico_keyboard_bridge 1)
pico_enable_stdio_uart(pico_keyboard_bridge 1)
pico_add_extra_outputs(pico_keyboard_bridge)
