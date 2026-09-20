# keyboard/src/CMakeLists.txt - pico_kb_bridge: BLE doorbell FFE0/FFE1 (patrón
# Verified 3x del SDK ble_doorbell) + TinyUSB HID keyboard 6KRO.
#
# REGLA DE ORO de esta sesión: el CMake es CLAVE literal del SDK. Copiar el
# doorbell VERIFIED y SOLO renombrar puentes; nada de pico_btstack_make_gatt_header
# con target inventado (leccion aprendida en las 3 config fallidas anteriores).

cmake_minimum_required(VERSION 3.13)

project(keyboard_bridge C CXX ASM)

# --- 1) ASCII -> HID usage+shift (tabla de 95 chars, puente texto->teclado)
add_library(kb_ascii_hid STATIC ascii_to_hid.cpp)
target_include_directories(kb_ascii_hid PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_features(kb_ascii_hid PUBLIC cxx_std_17)
target_link_libraries(kb_ascii_hid PUBLIC pico_stdlib)

# --- 2) USB HID keyboard (TinyUSB device boot keyboard 6KRO)
add_library(kb_usb_hid STATIC usb_kbd.c)
target_include_directories(kb_usb_hid PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(kb_usb_hid PUBLIC pico_stdlib tinyusb_device)
target_include_directories(kb_usb_hid PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../)

# --- 3) BLE GATT server "doorbell" (FFE0 service / FFE1 write, Verified 3x)
#     Se llama EXACTAMENTE como el SDK: doorbell_server. El .gatt es el del SDK.
add_library(kb_ble_server STATIC ble_server.c)
target_include_directories(kb_ble_server PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        )
target_link_libraries(kb_ble_server PUBLIC
        pico_stdlib
        pico_btstack_ble
        pico_btstack_cyw43
        pico_cyw43_arch_none
        )

# --- 4) Puente final: doorbell(FFF1 write) -> ascii -> USB HID
add_executable(pico_keyboard_bridge main.c)
target_include_directories(pico_keyboard_bridge PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        )
target_link_libraries(pico_keyboard_bridge PRIVATE
        pico_stdlib
        kb_ascii_hid
        kb_usb_hid
        kb_ble_server
        pico_btstack_ble
        pico_btstack_cyw43
        pico_cyw43_arch_none
        tinyusb_device
        )

pico_enable_stdio_usb(pico_keyboard_bridge 1)
pico_enable_stdio_uart(pico_keyboard_bridge 0)
pico_add_extra_outputs(pico_keyboard_bridge)

pico_btstack_make_gatt_header(pico_keyboard_bridge PRIVATE ${CMAKE_CURRENT_LIST_DIR}/doorbell.gatt)
