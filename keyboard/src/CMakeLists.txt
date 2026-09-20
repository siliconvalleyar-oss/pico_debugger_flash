cmake_minimum_required(VERSION 3.13)

# keyboard/src/CMakeLists.txt — pico_keyboard_bridge: PUENTE BLE "doorbell"
# (GATT FFE0/FFE1, doorbell.gatt 1:1 del SDK Verified) -> USB HID keyboard
# (TinyUSB 6KRO). Patrón EXACTO del SDK ble_doorbell: UN SOLO executable
# (sin librerías kb_* STATIC — esa fue la desobediencia que aisló main.c:4
# de cyw43_arch.h; aquí es 1 executable como doorbell_server Verified).

add_executable(pico_keyboard_bridge
    main.c
    ble_server.c
    ascii_to_hid.cpp
    usb_kbd.c
    )
target_include_directories(pico_keyboard_bridge PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../config # common btstack config (doorbell)
    ${CMAKE_CURRENT_LIST_DIR}/../BTstack # generated doorbell.h
    ${CMAKE_CURRENT_LIST_DIR}/../../config # root keyboard config
    )
target_link_libraries(pico_keyboard_bridge PUBLIC
    pico_stdlib
    pico_btstack_ble
    pico_btstack_cyw43
    pico_cyw43_arch_none
    tinyusb_device
    )
pico_btstack_make_gatt_header(pico_keyboard_bridge ${CMAKE_CURRENT_LIST_DIR}/doorbell.gatt)
pico_enable_stdio_usb(pico_keyboard_bridge 1)
pico_enable_stdio_uart(pico_keyboard_bridge 0)
pico_add_extra_outputs(pico_keyboard_bridge) — PUENTE BLE doorbell(FFE0/FFE1) -> USB HID keyboard
# LITERAL 1:1 del SDK ble_doorbell/CMakeLists.txt (que Verified [100%] dio el
# ELF de doorbell en esta sesión 3× — la lección: NADA de librerías kb_* con
# includes inventados; TODO lo que el SDK enlaza es lo que la PROBE SWD acepta)

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
pico_enable_stdio_usb(pico_keyboard_bridge 1)
pico_enable_stdio_uart(pico_keyboard_bridge 0)
pico_add_extra_outputs(pico_keyboard_bridge)
