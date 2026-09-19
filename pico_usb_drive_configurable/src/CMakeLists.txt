add_executable(pico_usb_drive_configurable
        main.cpp
        config_manager.cpp
        fatfs_interface.cpp
        usb_storage.cpp
        oled_display.cpp
        gpio_control.cpp
        )

# Source + include directories (config.h lives in src/, headers in include/)
target_include_directories(pico_usb_drive_configurable PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../include
        )

# FatFS library + Pico SDK + TinyUSB device stack
target_link_libraries(pico_usb_drive_configurable PUBLIC
        fatfs
        pico_stdlib
        tinyusb_device
        tinyusb_board
        hardware_i2c
        )

# For the W25Q16 flash programming we need the pico-sdk flash + sync routines
# that diskio.c calls (_u2_flash_range_erase etc.), pulled by tinyusb_board's
# pico_flash dependency; add explicitly to be safe.
target_link_libraries(pico_usb_drive_configurable PUBLIC
        pico_flash
        )

# create map/bin/hex/uf2 files
pico_add_extra_outputs(pico_usb_drive_configurable)
