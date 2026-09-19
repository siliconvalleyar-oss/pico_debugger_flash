/*-----------------------------------------------------------------------/
/  diskio.c - Low level disk interface for the internal QSPI flash        /
/  Overrides the SD/MMC sample from FatFS.                                /
/                                                                         /
/  Maps the FAT filesystem to a region of the RP2040's onboard flash      /
/  (2 MB..16 MB, auto-detected from the JEDEC ID) starting at             /
/  g_geom.disk_offset. Uses the pico-sdk XIP-safe flash_range_erase() /   /
/  flash_range_program() to persist data.                                 /
/                                                                         /
/  (c) ChaN - original FatFS diskio module                                /
/  SPDX-License-Identifier: MIT                                           /
/-----------------------------------------------------------------------*/

#include "ff.h"          /* Obtains integer types */
#include "diskio.h"      /* Declarations of disk functions */

#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "config.h"
#include "pendrive.h"

/* W25Q/JEDEC flash erase sector size = 4096 bytes. */
#define FLASH_ERASE_SIZE  4096u

/*
 * Runtime flash geometry. Initialised with the 2 MB fallback and overwritten
 * by flash_geom_init() as soon as the JEDEC ID is available (start of main()).
 */
pendrive_geom_t g_geom = {
    .flash_size  = 2u * 1024u * 1024u,
    .disk_offset = OFFSET_EN_FLASH,
    .disk_size   = TAMAÑO_MAXIMO_EN_BYTES,
};

/*--------------------------------------------------------------------------
   Public geometry (called first thing at boot).                        */

/*
 * flash_geom_init - Detect the physical flash size from the JEDEC ID
 * (0x9F) sent through the raw QSPI path (flash_do_cmd) and derive the
 * disk geometry:
 *
 *   flash <=  4 MB  -> firmware 512 KB, disk = flash - 512 KB  (2 MB -> 1.5 MB)
 *   flash >   4 MB  -> firmware   1 MB, disk = flash -   1 MB (16 MB -> 15 MB)
 *
 * Returns 0 on success, <0 if no usable geometry could be derived.
 */
int flash_geom_init(void) {
    uint8_t tx[4] = { 0x9Fu, 0u, 0u, 0u }; /* JEDEC Read ID */
    uint8_t rx[4] = { 0u, 0u, 0u, 0u };
    flash_do_cmd(tx, rx, 4);

    /* rx[1]=manufacturer, rx[2]=memory type, rx[3]=capacity byte.
     * For the common parts the capacity byte maps as
     * size = 1 MiB << (cap - 0x14): 0x15 -> 2 MB, 0x16 -> 4 MB,
     * 0x17 -> 8 MB, 0x18 -> 16 MB, ... */
    uint32_t flash_size = 0u;
    uint8_t  cap = rx[3];
    if (cap >= 0x14u && cap <= 0x1Fu) {
        flash_size = (1u << (cap - 0x14u)) * 1024u * 1024u;
    }
    if (flash_size == 0u) {
        flash_size = 16u * 1024u * 1024u;   /* unknown part: assume largest */
    }
    if (flash_size > 16u * 1024u * 1024u) {
        flash_size = 16u * 1024u * 1024u;   /* RP2040 XIP address space cap */
    }

    uint32_t firmware = (flash_size <= 4u * 1024u * 1024u)
                        ? OFFSET_EN_FLASH         /* 512 KB on 2/4 MB */
                        : OFFSET_EN_FLASH_16MB;   /* 1 MB on 8/16 MB */

    g_geom.flash_size  = flash_size;
    g_geom.disk_offset = (firmware + 0xFFFu) & ~0xFFFu;           /* 4-KB aligned */
    g_geom.disk_size   = (flash_size - g_geom.disk_offset) & ~0xFFFu;
    g_geom.disk_size  &= ~0x1FFu;                                  /* whole 512-B sectors */

    return (g_geom.disk_offset > 0u && g_geom.disk_size > 0u) ? 0 : -1;
}

/*
 * flash_region_is_blank - True if the whole disk area holds no meaningful
 * user data, i.e. every byte is 0xFF (erased) or 0x00 (bulk-programmed empty
 * fill, seen on some 16 MB clone flash chips). Used as a guard: we refuse to
 * auto-format a region that contains data we cannot read, so an existing
 * (e.g. Android/exFAT) volume is never wiped behind the host's back.
 */
int flash_region_is_blank(void) {
    const uint8_t *p = (const uint8_t *) (XIP_BASE + g_geom.disk_offset);
    for (uint32_t off = 0u; off < g_geom.disk_size; off += FLASH_ERASE_SIZE) {
        for (uint32_t i = 0u; i < FLASH_ERASE_SIZE; i++) {
            uint8_t b = p[off + i];
            if (b != 0xFFu && b != 0x00u) return 0;   /* real data present */
        }
    }
    return 1;
}

/*--------------------------------------------------------------------------

   Public Functions (see diskio.h for descriptions)

---------------------------------------------------------------------------*/

/* 
 * disk_initialize - Prepare the physical medium.
 * The flash is always present; nothing to do beyond clearing the "NOINIT"
 * flag. Called by f_mount().
 */
DSTATUS disk_initialize(BYTE pdrv) {
    (void) pdrv;
    return 0; /* no init error, drive ready */
}

/*
 * disk_status - Return the current status of the drive.
 * Always ready (no medium-removal, no hardware write-protect switch).
 */
DSTATUS disk_status(BYTE pdrv) {
    (void) pdrv;
    return 0; /* STA_NOINIT clear, STA_PROTECT clear -> ready & writable */
}

/*
 * disk_read - Read sector(s) from flash.
 * The pico-sdk XIP (execute in place) lets us read flash directly with memcpy
 * while code runs from flash, so a plain copy is safe and fast.
 */
DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    (void) pdrv;
    if (sector + count > (LBA_t) (g_geom.disk_size / DISK_SECTOR_SIZE)) {
        return RES_PARERR;
    }
    const uint8_t *src = (const uint8_t *) (XIP_BASE + g_geom.disk_offset + (sector * DISK_SECTOR_SIZE));
    memcpy(buff, src, (size_t) count * DISK_SECTOR_SIZE);
    return RES_OK;
}

/*
 * disk_write - Write sector(s) to flash.
 * Buttons: Erase granularity is 4096 B, write granularity is 256 B. To keep
 * it simple and correct we read-modify-erase-write whole 4 KB erase blocks.
 * NOTE: flash_range_program() disables interrupts and uses XIP-safe code;
 * temporarily blocks the USB stack (acceptable for small writes).
 */
DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    (void) pdrv;
    if (sector + count > (LBA_t) (g_geom.disk_size / DISK_SECTOR_SIZE)) {
        return RES_WRPRT;
    }

    /* small internal buffer must be in normal RAM (not flash XIP) */
    static uint8_t erase_buf[FLASH_ERASE_SIZE];
    /* process one full erase block at a time */
    while (count > 0) {
        /* absolute flash offset of the first sector of this erase block */
        uint32_t abs_offs = g_geom.disk_offset + (uint32_t) sector * DISK_SECTOR_SIZE;
        uint32_t block_start = abs_offs & ~(FLASH_ERASE_SIZE - 1u);
        uint32_t offset_in_block = abs_offs - block_start;

        /* read the current block content (it may be not fully rewritten) */
        memcpy(erase_buf, (const uint8_t *) (XIP_BASE + block_start), FLASH_ERASE_SIZE);

        /* overlay the sectors we must update */
        uint32_t to_write = FLASH_ERASE_SIZE - offset_in_block; /* bytes left in block */
        uint32_t want = (uint32_t) count * DISK_SECTOR_SIZE;
        if (want < to_write) to_write = want;
        memcpy(erase_buf + offset_in_block, buff, to_write);

        /* erase + program the whole 4 KB block */
        uint32_t saved = save_and_disable_interrupts();
        flash_range_erase(block_start, FLASH_ERASE_SIZE);
        flash_range_program(block_start, erase_buf, FLASH_ERASE_SIZE);
        restore_interrupts(saved);

        /* advance */
        uint32_t sectors_done = to_write / DISK_SECTOR_SIZE;
        sector += sectors_done;
        count  -= sectors_done;
        buff    += to_write;
    }
    return RES_OK;
}

/*
 * disk_ioctl - Control device dependent features.
 * We support CTRL_SYNC (flush) and GET_SECTOR_COUNT which are the ones the
 * FAT layer actually needs. flash writes are synchronous already.
 */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    (void) pdrv;
    switch (cmd) {
        case CTRL_SYNC:
            /* flash_range_program returns after bytes are committed -> synced */
            return RES_OK;

        case GET_SECTOR_COUNT:
            *(LBA_t*) buff = (LBA_t) (g_geom.disk_size / DISK_SECTOR_SIZE);
            return RES_OK;

        case GET_SECTOR_SIZE:
            *(WORD*) buff = DISK_SECTOR_SIZE;
            return RES_OK;

        case GET_BLOCK_SIZE:
            *(DWORD*) buff = FLASH_ERASE_SIZE / DISK_SECTOR_SIZE;
            return RES_OK;

        default:
            return RES_PARERR;
    }
}
