/**
 * @file usb_descriptors.cpp
 * @brief Descriptores USB estándar para enumerar como HID Boot Keyboard.
 */

#include "tusb.h"

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
static const tusb_desc_device_t kDeviceDescriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0xCafe,   // VID de prueba (pid.codes / TinyUSB ejemplo)
    .idProduct = 0x4004,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

const uint8_t *tud_descriptor_device_cb(void) {
    return (const uint8_t *)&kDeviceDescriptor;
}

//--------------------------------------------------------------------+
// HID Report Descriptor: Boot Keyboard estándar (8 bytes de reporte)
//--------------------------------------------------------------------+
static const uint8_t kHidReportDescriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

const uint8_t *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return kHidReportDescriptor;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
enum {
    kItfNumHid = 0,
    kItfNumTotal
};

#define kConfigTotalLen (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define kEpInHid 0x81

static const uint8_t kConfigDescriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, kItfNumTotal, 0, kConfigTotalLen,
                           TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    TUD_HID_DESCRIPTOR(kItfNumHid, 0, HID_ITF_PROTOCOL_KEYBOARD,
                        sizeof(kHidReportDescriptor), kEpInHid,
                        CFG_TUD_HID_EP_BUFSIZE, 10),
};

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return kConfigDescriptor;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+
static const char *kStringDescriptors[] = {
    (const char[]){0x09, 0x04}, // 0: idioma (English - US)
    "Pico Bridge Co.",          // 1: Manufacturer
    "Pico BLE-to-USB Keyboard", // 2: Product
    "PICOKB0001",               // 3: Serial
};

static uint16_t kStringDescBuf[32];

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t chr_count;

    if (index == 0) {
        memcpy(&kStringDescBuf[1], kStringDescriptors[0], 2);
        chr_count = 1;
    } else {
        if (index >= (sizeof(kStringDescriptors) / sizeof(kStringDescriptors[0]))) {
            return nullptr;
        }
        const char *str = kStringDescriptors[index];
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (size_t i = 0; i < chr_count; i++) {
            kStringDescBuf[1 + i] = str[i];
        }
    }

    kStringDescBuf[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return kStringDescBuf;
}

//--------------------------------------------------------------------+
// HID callbacks obligatorios (no usamos GET/SET_REPORT desde el host)
//--------------------------------------------------------------------+
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type, uint8_t *buffer,
                                uint16_t reqlen) {
    (void)instance; (void)report_id; (void)report_type;
    (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type, uint8_t const *buffer,
                            uint16_t bufsize) {
    (void)instance; (void)report_id; (void)report_type;
    (void)buffer; (void)bufsize;
}
