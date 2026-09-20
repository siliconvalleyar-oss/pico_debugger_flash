/*
 * main.cpp - Configurable USB Pendrive (Raspberry Pi Pico RP2040)
 *
 * SPDX-License-Identifier: MIT
 *
 * A professional USB pendrive whose storage lives in the Pico's onboard
 * W25Q16 flash (2 MB, formatted FAT). Everything is configurable:
 *   - compile time: config.h
 *   - runtime:      config.txt on the pendrive (hot-reloaded every 2 s)
 *
 * Architecture:
 *   main             -> orchestrator: boot, mount FS, USB, hot-plug watcher
 *   config_manager   -> read/write config.txt (+ auto-create) and hot watcher
 *   fatfs_interface  -> mount/format/sync the FAT volume on flash
 *   usb_storage      -> TinyUSB MSC callbacks bridging sectors to flash
 *   oled_display     -> SSD1306 status screen
 *   gpio_control     -> status LED patterns
 *   lib/fatfs        -> Elm-chan FatFS (flash-backed diskio)
 */

#include <cstdio>
#include <cstring>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "bsp/board_api.h"
#include "tusb.h"

#include "pendrive.h"
#include "config_manager.h"
#include "fatfs_interface.h"
#include "usb_storage.h"
#include "oled_display.h"
#include "gpio_control.h"
#include "diskio.h"
#include "config.h"

//====================================================================+
// GLOBAL STATE (declared extern in pendrive.h)
//====================================================================+
pendrive_cfg_t   g_cfg;
pendrive_state_t g_state;

//====================================================================+
// USB DESCRIPTORS (MSC only)
//====================================================================+

static uint16_t string_desc[32];

// Device descriptor: uses USB_VID / USB_PID from config.h (single source of truth).
#define USB_BCD   0x0200

tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,
    .bDeviceClass       = 0x00,          // class defined by interface (MSC)
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

uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};
#define EPNUM_MSC_OUT 0x01
#define EPNUM_MSC_IN  0x81

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 1, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_configuration;
}

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
};

static char const * string_desc_arr[] = {
    [STRID_LANGID]       = "Raspberry Pi",
    [STRID_MANUFACTURER] = USB_MANUFACTURER,
    [STRID_PRODUCT]      = USB_PRODUCT,
    [STRID_SERIAL]       = USB_SERIAL,
};

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    if (index == 0) {
        string_desc[1] = 0x0409;                 // English (US)
        string_desc[0] = (4 << 8) | 1;           // langid string
        return string_desc;
    }
    if (index >= 4) return NULL;
    uint8_t len = (uint8_t) strlen(string_desc_arr[index]);
    if (len > 31) len = 31;
    string_desc[0] = (uint16_t) (((2 + len) << 8) | 1);
    for (uint8_t i = 0; i < len; i++) {
        string_desc[1 + i] = string_desc_arr[index][i];
    }
    return string_desc;
}

//====================================================================+
// USB DEVICE CALLBACKS -> reflect state on the OLED / LED
//====================================================================+

extern "C" void tud_mount_cb(void)   { g_state.mounted = true;  gpio_led_set_solid(); }
extern "C" void tud_umount_cb(void)  { g_state.mounted = false; gpio_led_set_fast_blink(); }

/* Flush pending writes so nothing is lost if the drive is unplugged now. */
static void flush_on_disconnect(void) {
    extern void fatfs_sync(void);
    fatfs_sync();
    disk_flush();
}

extern "C" void tud_suspend_cb(bool rw) {
    (void) rw;
    /* Host is about to (re)configure or go idle; persist staged writes. */
    flush_on_disconnect();
}
extern "C" void tud_resume_cb(void)  { if (g_cfg.led_on_connect) gpio_led_set_solid(); }

//====================================================================+
// USB INIT (mirrors the newer TinyUSB API used by the SDK)
//====================================================================+
static void usb_device_init(void) {
    tusb_rhport_init_t dev_init = {
        .role  = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
    };
    tusb_init(BOARD_TUD_RHPORT, &dev_init);
}

//====================================================================+
// MAIN
//====================================================================+
int main(void) {
    stdio_init_all();

    // Fast blink = boot / init (until the FS is available).
    gpio_led_init();
    gpio_led_set_fast_blink();

    // Detect the physical flash (JEDEC ID) and derive the disk geometry:
    // 2 MB Pico -> 1.5 MB disk, 16 MB board -> 15 MB disk.
    flash_geom_init();
    printf("\r\nFlash detected: %u MB (0x%08X)\r\n",
           (unsigned) (g_geom.flash_size >> 20), (unsigned) g_geom.flash_size);
    printf("Disk: offset 0x%08X, size %u MB (%u sectors of %u B)\r\n",
           (unsigned) g_geom.disk_offset,
           (unsigned) (g_geom.disk_size >> 20),
           (unsigned) (g_geom.disk_size / DISK_SECTOR_SIZE),
           (unsigned) DISK_SECTOR_SIZE);

    // Bring up the flash-backed FAT volume (formats it once if blank).
    fatfs_mount();

    // Load runtime config from config.txt (creates file on first boot).
    config_manager_init();

    // Give the volume the configured label so the host shows e.g. "MiPendrive".
    fatfs_set_label(g_cfg.volume_label);

    // Refresh capacity stats.
    uint32_t total_mb = 0, free_mb = 0;
    if (fatfs_get_free_mb(&free_mb, &total_mb) == 0) {
        g_state.total_mb = total_mb;
        g_state.free_mb  = free_mb;
    }

    // OLED (if enabled).
    if (g_cfg.enable_oled) {
        oled_init();
        oled_render(&g_cfg, &g_state);
    }

    // USB: wait a configurable delay so the FS is fully ready before the host
    // enumerates the drive (avoids "device not recognized" race conditions).
    usb_device_init();
    if (g_cfg.auto_mount_delay_ms > 0) {
        sleep_ms(g_cfg.auto_mount_delay_ms);
    }

    // Human-readable boot marker over UART (stdio).
    printf("\r\nConfigurable USB Pendrive booted\r\n");
    printf("Volume: %s, read_only=%u, oled=%u\r\n",
           g_cfg.volume_label, g_cfg.read_only, g_cfg.enable_oled);

    uint32_t last_poll = 0;
    uint32_t start_ms  = board_millis();

    while (true) {
        tud_task();

        /* Background write-behind flusher: commit dirty 4 KB blocks to flash
         * one 256-B page at a time so each IRQ-off window is ~2 ms and the
         * USB stack stays responsive. Drain a few pages per iteration
         * (serving USB between them) so a big `sync` drains fast enough;
         * erases (40 ms) are only issued when the USB is idle, or while a
         * SYNCHRONIZE_CACHE / SYNCHRONIZE_CACHE_10 flush is pending (the
         * host asked for a flush, so it will not issue new writes until we
         * signal completion -- safe to erase then). See diskio.c. */
        if (usb_storage_sync_pending()) {
            extern bool g_cache_force_erase;
            extern uint32_t disk_cache_dirty(void);
            g_cache_force_erase = true;               /* allow 40 ms erases */
            uint32_t spins = 0;
            while (disk_cache_dirty() > 0u && spins < 32u) {
                disk_cache_service();
                tud_task();                           /* serve USB between */
                spins++;
            }
            if (disk_cache_dirty() == 0u) {
                usb_storage_sync_done();
                g_cache_force_erase = false;
            }
        } else {
            for (uint32_t f = 0; f < 8u; f++) {
                if (!disk_cache_service()) break;
                tud_task();
            }
        }

        // Hot-plug config watcher: every CONFIG_POLL_INTERVAL_MS.
        uint32_t now = board_millis();
        if (now - last_poll >= CONFIG_POLL_INTERVAL_MS) {
            last_poll = now;
            config_manager_poll();          // may update g_cfg (READ_ONLY etc.)
        }

        // Periodic OLED refresh + LED tick.
        gpio_led_task();
        if (g_cfg.enable_oled) {
            oled_render(&g_cfg, &g_state);
        }

        (void) start_ms;
    }

    return 0;
}
