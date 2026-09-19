#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tusb.h"
#include "pico/unique_id.h"

// HID report IDs
enum {
    REPORT_ID_KEYBOARD = 1,
};

// USB Descriptors
#define USB_VID   0x2E8A  // Raspberry Pi
#define USB_PID   0x000A
#define USB_BCD   0x0200

// Keyboard HID Report Descriptor (standard 6-key rollover)
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD))
};

// Device descriptor
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// Configuration descriptor
#define EPNUM_HID   0x81
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 5)
};

// String descriptors
char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 }, // English
    "Raspberry Pi",                // Manufacturer
    "Pico Rubber Ducky",           // Product
    NULL,                          // Serial (use unique ID)
};

// TinyUSB callbacks
uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return desc_hid_report;
}

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_configuration;
}

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    static uint16_t _desc_str[32 + 1];
    size_t chr_count;

    switch (index) {
        case 0:
            memcpy(&_desc_str[1], string_desc_arr[0], 2);
            chr_count = 1;
            break;
        case 0xEE: // Microsoft OS 1.0
            return NULL;
        case 3: { // Serial
            pico_get_unique_board_id_string((char*)&_desc_str[1], 64);
            chr_count = strlen((char*)&_desc_str[1]);
            break;
        }
        default:
            if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
            const char *str = string_desc_arr[index];
            chr_count = strlen(str);
            for (size_t i = 0; i < chr_count; i++) {
                _desc_str[1 + i] = str[i];
            }
            break;
    }
    _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}

// Required HID callbacks (can be empty for keyboard-only)
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
}

// Keyboard state
static bool usb_mounted = false;
static bool script_executed = false;

void tud_mount_cb(void) {
    usb_mounted = true;
}

void tud_umount_cb(void) {
    usb_mounted = false;
    script_executed = false;
}

static void send_key(uint8_t modifier, uint8_t keycode) {
    uint8_t keycodes[6] = {keycode, 0, 0, 0, 0, 0};
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keycodes);
    sleep_ms(10);
    keycodes[0] = 0;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycodes);
    sleep_ms(10);
}

static void send_string(const char* str) {
    while (*str) {
        uint8_t modifier = 0;
        uint8_t keycode = 0;
        char c = *str++;

        if (c >= 'a' && c <= 'z') {
            keycode = c - 'a' + 4;  // HID_KEY_A = 4
        } else if (c >= 'A' && c <= 'Z') {
            modifier = 0x02; // Shift
            keycode = c - 'A' + 4;
        } else if (c >= '1' && c <= '9') {
            keycode = c - '1' + 30; // HID_KEY_1 = 30
        } else if (c == '0') {
            keycode = 39; // HID_KEY_0
        } else if (c == ' ') {
            keycode = 44; // HID_KEY_SPACE
        } else if (c == '\n') {
            keycode = 40; // HID_KEY_ENTER
        } else if (c == '\t') {
            keycode = 43; // HID_KEY_TAB
        } else if (c == '-') {
            keycode = 45; // HID_KEY_MINUS
        } else if (c == '_') {
            modifier = 0x02;
            keycode = 45;
        } else if (c == '/') {
            keycode = 54; // HID_KEY_SLASH
        } else if (c == '.') {
            keycode = 55; // HID_KEY_DOT
        } else if (c == ':') {
            modifier = 0x02;
            keycode = 55;
        } else if (c == ';') {
            keycode = 51; // HID_KEY_SEMICOLON
        } else if (c == '"') {
            modifier = 0x02;
            keycode = 51;
        } else if (c == '\'') {
            keycode = 52; // HID_KEY_APOSTROPHE
        } else if (c == '`') {
            keycode = 53; // HID_KEY_GRAVE
        } else if (c == '~') {
            modifier = 0x02;
            keycode = 53;
        } else if (c == '[') {
            keycode = 47; // HID_KEY_BRACKET_LEFT
        } else if (c == ']') {
            keycode = 48; // HID_KEY_BRACKET_RIGHT
        } else if (c == '\\') {
            keycode = 49; // HID_KEY_BACKSLASH
        } else if (c == '=') {
            keycode = 46; // HID_KEY_EQUAL
        } else if (c == '+') {
            modifier = 0x02;
            keycode = 46;
        } else {
            continue;
        }

        if (keycode) {
            send_key(modifier, keycode);
        }
    }
}

static void send_hotkey(uint8_t modifier, uint8_t keycode) {
    uint8_t keycodes[6] = {keycode, 0, 0, 0, 0, 0};
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keycodes);
    sleep_ms(50);
    keycodes[0] = 0;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycodes);
    sleep_ms(50);
}

static void execute_payload(void) {
    sleep_ms(1000);

    // Super + T (open terminal in GNOME/KDE)
    send_hotkey(0x04, 0x17); // GUI + T
    sleep_ms(500);

    send_string("echo 'Hola desde Pico Rubber Ducky!'\n");
    sleep_ms(300);

    send_string("whoami\n");
    sleep_ms(300);

    send_string("date\n");
    sleep_ms(300);
}

int main() {
    // Initialize TinyUSB
    tusb_init();

    while (true) {
        tud_task(); // TinyUSB device task

        if (usb_mounted && !script_executed) {
            execute_payload();
            script_executed = true;
        }

        sleep_ms(10);
    }

    return 0;
}