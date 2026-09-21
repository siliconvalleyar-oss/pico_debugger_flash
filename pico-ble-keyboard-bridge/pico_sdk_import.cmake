# Este es un archivo estándar del Raspberry Pi Pico SDK.
# Se copia tal cual desde $PICO_SDK_PATH/external/pico_sdk_import.cmake
# No hace falta editarlo.

if(DEFINED ENV{PICO_SDK_PATH} AND (NOT PICO_SDK_PATH))
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    message("Using PICO_SDK_PATH from environment ('${PICO_SDK_PATH}')")
endif()

if(NOT PICO_SDK_PATH)
    message(FATAL_ERROR
        "PICO_SDK_PATH no está definido. Exportá la variable de entorno "
        "apuntando a tu copia del Raspberry Pi Pico SDK, por ejemplo:\n"
        "  export PICO_SDK_PATH=/home/usuario/pico-sdk"
    )
endif()

set(PICO_SDK_PATH "${PICO_SDK_PATH}" CACHE PATH "Path to the Raspberry Pi Pico SDK")
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
