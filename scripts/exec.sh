

#openocd -f scripts/debugprobe-openocd.cfg -c "program dongle_wifi_usb/build/dongle_wifi_usb.elf verify reset exit"

BOARD=pico_w scripts/flash_nosudo.sh usb_oled_drive_configurable

