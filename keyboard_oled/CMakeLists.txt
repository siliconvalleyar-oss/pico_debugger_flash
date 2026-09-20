cmake_minimum_required(VERSION 3.13)

# keyboard/CMakeLists.txt — Pico W: teclado BLE->USB HID bridge (doorbell FFE0/FFE1).
# El import pico_sdk_import.cmake se copió de pico-sdk/external/ (Verified 3x).
include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)

set(PICO_BOARD pico_w CACHE STRING "")
project(pico_kb_bridge C CXX ASM)

pico_sdk_init()
add_subdirectory(src)
