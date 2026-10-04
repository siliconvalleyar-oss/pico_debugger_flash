

#openocd -f scripts/debugprobe-openocd.cfg -c "program dongle_wifi_usb/build/dongle_wifi_usb.elf verify reset exit"

BOARD=pico_w EXPECTED_BOARD_ID=tu_id_aqui scripts/flash_nosudo.sh blink_locked_oled


#openocd -f scripts/debugprobe-openocd.cfg -c "program /mnt/disk/src/rpico/pico_debugger_flash/blink/build/src/blink.elf verify reset exit"
