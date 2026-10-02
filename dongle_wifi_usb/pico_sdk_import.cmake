cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH "" CACHE PATH "Path to pico-sdk")

if(NOT PICO_SDK_PATH)
    if(DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    else()
        set(PICO_SDK_PATH "../../pico-sdk" CACHE PATH "Path to pico-sdk")
    endif()
endif()

if(NOT EXISTS "${PICO_SDK_PATH}/pico_sdk_init.cmake")
    message(FATAL_ERROR "pico-sdk not found at ${PICO_SDK_PATH}. Set PICO_SDK_PATH or place pico-sdk at ../pico-sdk")
endif()

include("${PICO_SDK_PATH}/pico_sdk_init.cmake")
