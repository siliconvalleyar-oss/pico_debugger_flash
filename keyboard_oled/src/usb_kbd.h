#pragma once
#include <stdint.h>
#include <stdbool.h>

/* usb_kbd.c provides USB HID descriptors and TinyUSB callbacks.
 * main.c calls tud_hid_keyboard_report() directly from TinyUSB. */
