cmake_minimum_required(VERSION 3.13)

# PICO_SDK_PATH explicito: si el entorno apunta a una ruta que no existe, el
# include() falla mas abajo con un error dificil de leer.
if(NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
if(NOT PICO_SDK_PATH)
    message(FATAL_ERROR
        "Falta PICO_SDK_PATH.  Usar: cmake -S . -B build -DPICO_SDK_PATH=/ruta/al/pico-sdk -DPICO_BOARD=pico2")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(ssd1963 CXX C ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT PICO_BOARD)
    set(PICO_BOARD pico2)
endif()

# Ancho del bus paralelo del panel: 8 (por defecto) o 16.  Con 16 hay que
# remapear los pines del touch o ponerlo a 0, ver tft/tft_config.h.
if(NOT DEFINED TFT_BUS_WIDTH)
    set(TFT_BUS_WIDTH 8)
endif()
if(NOT DEFINED TFT_TOUCH_ENABLE)
    set(TFT_TOUCH_ENABLE 1)
endif()

pico_sdk_init()

add_executable(ssd1963
    main.cpp
    tft/bus_pio.cpp
    tft/panel.cpp
    tft/canvas.cpp
    tft/tft_fonts.cpp
    tft/touch.cpp
)

target_include_directories(ssd1963 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/tft
)

pico_generate_pio_header(ssd1963 ${CMAKE_CURRENT_LIST_DIR}/pio/tft.pio)

target_link_libraries(ssd1963 PRIVATE
    pico_stdlib
    pico_unique_id
    hardware_pio
    hardware_dma
    hardware_spi
    hardware_pwm
    hardware_sync
)

target_compile_definitions(ssd1963 PRIVATE
    TFT_BOARD_NAME="${PICO_BOARD}"
    TFT_BUS_WIDTH=${TFT_BUS_WIDTH}
    TFT_TOUCH_ENABLE=${TFT_TOUCH_ENABLE}
)

target_compile_options(ssd1963 PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)
