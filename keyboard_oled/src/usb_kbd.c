/*
 * usb_kbd.c — TinyUSB HID Boot Keyboard (boot protocol, 6KRO).
 * Also provides USB descriptors required by TinyUSB.
 */
#include "usb_kbd.h"
#include "tusb.h"
#include <string.h>

//-----------------------------------
// USB Descriptors
//-----------------------------------

// HID Report Descriptor for Boot Keyboard (6KRO)
static const uint8_t hid_report_desc[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

// Device Descriptor
static const tusb_desc_device_t device_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xC0DE,
    .idProduct          = 0x0001,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01,
};

// Configuration Descriptor
enum {
    ITF_NUM_HID = 0,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID         0x81

static const uint8_t config_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_PROTOCOL_BOOT, sizeof(hid_report_desc), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 10),
};

// String Descriptors
static const char *string_desc_arr[] = {
    [0] = (const char[]){0x09, 0x04},  // English
    [1] = "Pico KB Bridge",
    [2] = "BLE USB Keyboard",
    [3] = "0001",
};

static uint16_t _desc_str[32];

// Helper to convert ASCII string to USB string descriptor (UTF-16LE)
static uint16_t *get_string_desc(uint8_t desc_id, uint16_t *len) {
    if (desc_id == 0) {
        memcpy(_desc_str, string_desc_arr[0], 2);
        *len = 1;
        return _desc_str;
    }
    if (desc_id > 3) return NULL;
    const char *str = string_desc_arr[desc_id];
    uint8_t chr_count = strlen(str);
    if (chr_count > 31) chr_count = 31;
    for (uint8_t i = 0; i < chr_count; i++) {
        _desc_str[1 + i] = str[i];
    }
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * (chr_count + 1));
    *len = chr_count;
    return _desc_str;
}

//-----------------------------------
// TinyUSB Callbacks
//-----------------------------------

// Device Descriptor
const uint8_t *tud_descriptor_device_cb(void) {
    return (const uint8_t *)&device_desc;
}

// Configuration Descriptor
const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return config_desc;
}

// String Descriptors
const uint16_t *tud_descriptor_string_cb(uint8_t desc_id, uint16_t langid) {
    (void)langid;
    uint16_t len = 0;
    return get_string_desc(desc_id, &len);
}

// HID Descriptor (report descriptor)
const uint8_t *tud_hid_descriptor_report_cb(uint8_t itf) {
    (void)itf;
    return hid_report_desc;
}

// HID Get Report
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
    (void)itf; (void)report_id; (void)report_type; (void)reqlen;
    return 0;
}

// HID Set Report
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, const uint8_t *buffer, uint16_t bufsize) {
    (void)itf; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}


