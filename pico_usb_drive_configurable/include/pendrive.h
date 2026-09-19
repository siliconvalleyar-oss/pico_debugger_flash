/*
 * pendrive.h - Shared state and module interface for the Configurable USB Pendrive
 *
 * SPDX-License-Identifier: MIT
 *
 * Central header that all modules include. It holds the runtime configuration
 * struct (fed by config.txt), the global system state, and the public
 * prototypes of every subsystem so the whole firmware links coherently.
 */

#ifndef _PENDRIVE_H_
#define _PENDRIVE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/*
 * Runtime configuration, populated from config.txt (with config.h defaults
 * as fallback). Every field maps 1:1 to a config.txt key.
 */
typedef struct {
    char      volume_label[12];          // VOLUME_LABEL
    uint8_t   read_only;                 // READ_ONLY (0/1)
    uint8_t   enable_oled;               // ENABLE_OLED (0/1)
    uint8_t   led_on_connect;            // LED_ON_CONNECT (0/1)
    uint32_t  auto_mount_delay_ms;       // AUTO_MOUNT_DELAY_MS
    bool      config_valid;              // false if config.txt had syntax errors
} pendrive_cfg_t;

/*
 * Global system state used by the OLED and hot-plug watcher.
 */
typedef struct {
    bool  mounted;                       // host has mounted our MSC lun
    bool  reading;                       // an MSC READ is in progress
    bool  writing;                       // an MSC WRITE is in progress
    uint32_t total_mb;                   // capacity in MB (computed)
    uint32_t free_mb;                    // free space in MB (from f_getfree)
} pendrive_state_t;

/*
 * Runtime geometry of the flash-backed disk. Computed by flash_geom_init()
 * once at boot from the real (JEDEC-detected) QSPI flash size, so the same
 * firmware works on a 2 MB Pico (1.5 MB disk) or a 16 MB board (15 MB disk).
 */
typedef struct {
    uint32_t flash_size;     // physical flash detected, bytes
    uint32_t disk_offset;    // absolute flash offset where the FAT area begins
    uint32_t disk_size;      // exposed disk size in bytes (multiple of 512)
} pendrive_geom_t;

/* Global instances (defined in main.cpp) */
extern pendrive_cfg_t   g_cfg;
extern pendrive_state_t g_state;
extern pendrive_geom_t  g_geom;

/* Detect the physical flash size and derive the disk geometry (diskio.c). */
int  flash_geom_init(void);
int  flash_region_is_blank(void);      // true if the whole disk area is erased

/*--------------------------------------------------------------------+
 * config_manager
 *--------------------------------------------------------------------*/
void       config_manager_init(void);          // load or create config.txt
void       config_manager_poll(void);          // hot-plug watcher (call every ~2 s)
bool       config_get_updated_flag(void);      // changed since last apply
const char* config_get_volume_label(void);

/*--------------------------------------------------------------------+
 * fatfs_interface
 *--------------------------------------------------------------------*/
int        fatfs_mount(void);                  // mount volume (format if needed)
int        fatfs_get_free_mb(uint32_t *free_mb, uint32_t *total_mb);
void       fatfs_sync(void);                   // flush dirty FAT buffers
int        fatfs_reload_config(void);          // (re)apply config.txt on disk

/*--------------------------------------------------------------------+
 * usb_storage (TinyUSB MSC callbacks)
 *--------------------------------------------------------------------*/
void       usb_storage_init(void);

/*--------------------------------------------------------------------+
 * oled_display
 *--------------------------------------------------------------------*/
void       oled_init(void);
void       oled_render(const pendrive_cfg_t *cfg, const pendrive_state_t *st);

/*--------------------------------------------------------------------+
 * gpio_control (LED states)
 *--------------------------------------------------------------------*/
void       gpio_led_init(void);
void       gpio_led_set_fast_blink(void);      // init / error
void       gpio_led_set_solid(void);           // mounted OK
void       gpio_led_set_slow_blink(void);      // config syntax error
void       gpio_led_task(void);                // called from main loop

#ifdef __cplusplus
}
#endif

#endif /* _PENDRIVE_H_ */
