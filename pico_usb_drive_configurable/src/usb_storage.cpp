/*
 * usb_storage.cpp - TinyUSB MSC callbacks: expose the flash region as a pendrive
 *
 * SPDX-License-Identifier: MIT
 *
 * Bridges SCSI READ10/WRITE10 to sector accesses on the W25Q16 flash area via
 * FatFS's disk_read()/disk_write() (see lib/fatfs/source/diskio.c).
 *
 * The "tud_msc_*" symbols are referenced by TinyUSB from C, so they must have
 * C linkage. We force it with extern "C" below.
 */

#include <string.h>
#include "tusb.h"
#include "ff.h"
#include "diskio.h"
#include "pendrive.h"
#include "usb_storage.h"

/*
 * A staging sector buffer, used because disk_read/disk_write work on whole
 * 512-byte sectors while TinyUSB may deliver sub-sector requests.
 */
static uint8_t s_sector_buf[DISK_SECTOR_SIZE];

/* Capacity reported to the host: derived from the runtime flash geometry
 * (g_geom), which depends on the detected physical flash (2 MB -> 1.5 MB
 * disk, 16 MB -> 15 MB disk). */
static const uint16_t s_block_size = DISK_SECTOR_SIZE;

static uint32_t block_count(void) {
    return (uint32_t) (g_geom.disk_size / DISK_SECTOR_SIZE);
}

/* (de)clared called by TinyUSB from C */
extern "C" {

/* Invoked when the host sends SCSI_CMD_INQUIRY. Returns identity strings. */
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
    (void) lun;
    /* fixed-width, space padded 8-byte VID and 16-byte PID */
    char v[8], p[16];
    memset(v, ' ', sizeof(v));
    memset(p, ' ', sizeof(p));
    size_t vl = strlen(USB_MANUFACTURER);
    if (vl > sizeof(v)) vl = sizeof(v);
    memcpy(v, USB_MANUFACTURER, vl);
    size_t pl = strlen(USB_PRODUCT);
    if (pl > sizeof(p)) pl = sizeof(p);
    memcpy(p, USB_PRODUCT, pl);
    memcpy(vendor_id, v, 8);
    memcpy(product_id, p, 16);
    product_rev[0] = '1'; product_rev[1] = '.'; product_rev[2] = '0'; product_rev[3] = ' ';
}

/* Invoked when received Test Unit Ready */
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void) lun;
    /* always ready (no eject/removal support) */
    return true;
}

/* Invoked when received READ_CAPACITY_10 and READ_FORMAT_CAPACITY */
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    (void) lun;
    *block_count = usb_storage_block_count();
    *block_size  = s_block_size;
}

/* Invoked when received SCSI_CMD_START_STOP_UNIT (load/eject) */
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition,
                           bool start, bool load_eject) {
    (void) lun; (void) power_condition; (void) start; (void) load_eject;
    /* We do not actually park the drive. */
    return true;
}

/*
 * Invoked when the host asks whether the medium is writable.
 * Honours READ_ONLY from config.txt so toggling it re-applies on the fly
 * (the PC re-queries this on each access / on remount).
 */
bool tud_msc_is_writable_cb(uint8_t lun) {
    (void) lun;
    return !(g_cfg.read_only != 0);
}

/*
 * Invoked when received a READ10 command. Reads from the flash region.
 * TinyUSB hands us (lun, lba, offset, buffer, bufsize). We only support
 * requests aligned within one sector (offset==0 || offset+bufsize<=512).
 */
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void* buffer, uint32_t bufsize) {
    (void) lun;
    uint32_t bc = block_count();
    if (lba >= bc) return -1;
    // bounds check against whole disk
    if ((uint64_t) lba * s_block_size + offset + bufsize >
        (uint64_t) bc * s_block_size) {
        return -1;
    }

    /* read the full sector then copy the requested window */
    if (disk_read(0, s_sector_buf, (LBA_t) lba, 1) != RES_OK) return -1;
    memcpy(buffer, s_sector_buf + offset, bufsize);
    return (int32_t) bufsize;
}

/*
 * Invoked when received a WRITE10 command. Writes (persists) to the flash.
 */
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t* buffer, uint32_t bufsize) {
    (void) lun;
    uint32_t bc = block_count();
    if (lba >= bc) return -1;
    if (g_cfg.read_only != 0) return -1;     // read-only mode (hot-applied)
    if ((uint64_t) lba * s_block_size + offset + bufsize >
        (uint64_t) bc * s_block_size) {
        return -1;
    }

    /* read-modify-write for a sub-sector range; full sector write otherwise */
    if (bufsize == s_block_size && offset == 0) {
        if (disk_write(0, buffer, (LBA_t) lba, 1) != RES_OK) return -1;
    } else {
        if (disk_read(0, s_sector_buf, (LBA_t) lba, 1) != RES_OK) return -1;
        memcpy(s_sector_buf + offset, buffer, bufsize);
        if (disk_write(0, s_sector_buf, (LBA_t) lba, 1) != RES_OK) return -1;
    }
    return (int32_t) bufsize;
}

/* Invoked for SCSI commands not handled by TinyUSB's built-in list */
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16],
                        void* buffer, uint16_t bufsize) {
    (void) lun; (void) scsi_cmd; (void) buffer; (void) bufsize;
    /* report Invalid Command Operation */
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
    return -1;
}

/* Invoked when a SYNCHRONIZE_CACHE_10 arrives (host wants all data flushed). */
bool tud_msc_synchronize_cache_cb(uint8_t lun) {
    (void) lun;
    /* flash writes are synchronous already, but flush FatFS buffers to be safe */
    extern void fatfs_sync(void);
    fatfs_sync();
    return true;
}

} // extern "C"

/*
 * Install MSC: nothing specific to do beyond making sure the FS volume is the
 * one TinyUSB will read. The descriptors / endpoints are configured in
 * tusb_config.h + the device descriptors in main.cpp.
 */
void usb_storage_init(void) {
    /* LBA0 does not need any device-side preparation: the FAT volume is
     * mounted by FatFS in fatfs_mount() before the host enumerates the LUN. */
}

/* Number of 512-byte sectors presented to the host (runtime geometry). */
uint32_t usb_storage_block_count(void) {
    return block_count();
}
