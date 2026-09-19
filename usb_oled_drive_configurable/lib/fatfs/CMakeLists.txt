# FatFS (Elm-chan) vendored library - compiled as C, linked into the app.
# diskio.c is our flash-backed override; ff.c / ffsystem.c / ffunicode.c are
# the stock FatFS core from the vendor.

add_library(fatfs STATIC
        source/ff.c
        source/ffsystem.c
        source/ffunicode.c
        source/diskio.c
        )

# FatFS needs its own header dir plus our config.h (for flash layout macros)
# and pendrive.h (runtime flash geometry globals).
target_include_directories(fatfs PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/source
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
        )

target_include_directories(fatfs SYSTEM PUBLIC
        ${PICO_SDK_PATH}/src/common/pico_base/include
        ${PICO_SDK_PATH}/src/rp2_common/hardware_base/include
        ${PICO_SDK_PATH}/src/rp2_common/hardware_flash/include
        ${PICO_SDK_PATH}/src/rp2_common/hardware_sync/include
        ${PICO_SDK_PATH}/src/rp2_common/hardware_regs/include
        ${PICO_SDK_PATH}/src/common/pico_binary_info/include
        ${PICO_SDK_PATH}/src/rp2_common/pico_flash/include
        ${PICO_SDK_PATH}/src/rp2040/hardware_structs/include
        )

# C standard required by FatFS types
set_target_properties(fatfs PROPERTIES C_STANDARD 11)

target_link_libraries(fatfs PUBLIC pico_stdlib)
