/*
 * usb_storage.h - TinyUSB Mass Storage Class callbacks bridging to flash/FatFS
 *
 * SPDX-License-Identifier: MIT
 *
 * The MSC class makes the Pico appear as a USB pendrive. The callbacks in the
 * matching .cpp translate SCSI READ10/WRITE10 into sector reads/writes on the
 * W25Q16 flash region (via disk_read / disk_write from FatFS's diskio.c). The
 * two buttons below are the ones that matter most:
 *
 *   - tud_msc_inquiry_cb  -> identity strings (Manufacturer / Product)
 *   - tud_msc_is_writable_cb -> respects READ_ONLY (hot re-configurable)
 */

#ifndef _USB_STORAGE_H_
#define _USB_STORAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the USB storage role (called after FS is mounted). */
void usb_storage_init(void);

/* Number of 512-byte sectors the MSC LUN exposes (from g_geom runtime size). */
uint32_t usb_storage_block_count(void);

/* True while a SYNCHRONIZE_CACHE command is pending (the host asked for a
 * flush and the data has not reached flash yet). The main loop force-flushes
 * the write-behind cache (letting it issue 40 ms erases) and then calls
 * usb_storage_sync_done() once everything is durable. See diskio.c. */
bool usb_storage_sync_pending(void);
void usb_storage_sync_done(void);

#ifdef __cplusplus
}
#endif

#endif /* _USB_STORAGE_H_ */
