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
#include "pico/time.h"
#include "config.h"
#include "pendrive.h"

/* W25Q/JEDEC flash erase sector size = 4096 bytes. */
#define FLASH_ERASE_SIZE  4096u
#define FLASH_PROGRAM_PAGE 256u

/*
 * Multi-block write-behind cache (one slot per 4 KB erase sector).
 *
 * Flash erase granularity is 4096 B, write granularity is 256 B. A 4 KB
 * erase takes ~40 ms with interrupts disabled, so doing a read-erase-program
 * cycle for every 512 B sector the host sends stalls the USB stack and the
 * host resets the bus (truncated/corrupted copies). The old single-block
 * cache only moved the stall: any commit happened inside the SCSI WRITE10
 * callback, i.e. inside tud_task(), with IRQs off for ~50 ms.
 *
 * Design here:
 *   - disk_write()/disk_read() only touch RAM (never flash) until the write
 *     request cannot be staged any more.
 *   - A background flusher (disk_cache_service(), called from the main loop
 *     between tud_task() calls) commits dirty blocks to flash.
 *   - Committing programs in 256 B pages: the IRQ-off window is ~2 ms per
 *     page, small enough that the USB controller keeps answering tokens.
 *   - A 4 KB erase (40 ms IRQ-off) is only issued when the USB is idle
 *     (> SCSI_IDLE_ERASE_MS since the last SCSI command) and only for blocks
 *     that are not already all-0xFF (blank blocks skip the erase entirely).
 *   - disk_flush() forces everything to flash (SYNCHRONIZE_CACHE / CTRL_SYNC,
 *     suspend, unmount) so copies complete durably.
 */
#define NCACHE_SLOTS     16u          /* 16 x 4 KB = 64 KB RAM write-behind */
#define SCSI_IDLE_ERASE_MS  5u        /* USB idle threshold before erasing */
#define CACHE_FULL_FLUSH    (NCACHE_SLOTS - 4u)  /* emergency flush level */

typedef struct {
    uint32_t  block_start;             /* flash offset of the 4 KB block */
    uint8_t   data[FLASH_ERASE_SIZE];  /* staged copy (RAM) */
    uint32_t  age;                     /* load order, oldest gets evicted first */
    bool      dirty;                   /* RAM newer than flash */
    bool      blank;                   /* flash block is all 0xFF (no erase) */
} cache_slot_t;

static cache_slot_t s_cache[NCACHE_SLOTS];
static uint32_t     s_age = 0;         /* monotonic allocation counter */
static uint32_t     s_last_scsi_ms = 0;/* last SCSI activity (idle detection) */

/* Find a slot holding a given flash block, or NULL. */
static cache_slot_t *cache_find(uint32_t block_start) {
    for (uint32_t i = 0; i < NCACHE_SLOTS; i++) {
        if (s_cache[i].block_start == block_start) return &s_cache[i];
    }
    return NULL;
}

/* Load a 4 KB flash block from XIP into a fresh slot. */
static void cache_load(cache_slot_t *slot, uint32_t block_start) {
    memcpy(slot->data, (const uint8_t *) (XIP_BASE + block_start), FLASH_ERASE_SIZE);
    slot->block_start = block_start;
    slot->age         = s_age++;
    slot->dirty       = false;
    slot->blank       = true;
    for (uint32_t i = 0; i < FLASH_ERASE_SIZE; i++) {
        if (slot->data[i] != 0xFFu) { slot->blank = false; break; }
    }
}

/*
 * Persist one dirty slot.
 * - Erase (40 ms IRQ off) only when the block is not blank and the caller
 *   allowed it; the caller checks idle/urgency.
 * - Program in 256 B pages so the IRQ-off window stays ~2 ms per page.
 */
static void slot_commit(cache_slot_t *slot, bool erase_ok) {
    if (!slot->dirty) return;

    if (erase_ok && !slot->blank) {
        flash_range_erase(slot->block_start, FLASH_ERASE_SIZE);
    }
    for (uint32_t off = 0; off < FLASH_ERASE_SIZE; off += FLASH_PROGRAM_PAGE) {
        flash_range_program(slot->block_start + off,
                            slot->data + off, FLASH_PROGRAM_PAGE);
    }
    slot->dirty = false;
    slot->blank = false;
}

/* Oldest (least recently loaded) dirty slot. */
static cache_slot_t *cache_oldest_dirty(void) {
    cache_slot_t *best = NULL;
    for (uint32_t i = 0; i < NCACHE_SLOTS; i++) {
        if (s_cache[i].dirty &&
            (best == NULL || s_cache[i].age < best->age)) best = &s_cache[i];
    }
    return best;
}

/* Oldest dirty slot that is still blank in flash (cheap commit, no erase). */
static cache_slot_t *cache_oldest_dirty_blank(void) {
    cache_slot_t *best = NULL;
    for (uint32_t i = 0; i < NCACHE_SLOTS; i++) {
        if (s_cache[i].dirty && s_cache[i].blank &&
            (best == NULL || s_cache[i].age < best->age)) best = &s_cache[i];
    }
    return best;
}

static uint32_t cache_dirty_count(void) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < NCACHE_SLOTS; i++) if (s_cache[i].dirty) n++;
    return n;
}

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
 * disk_read - Read sector(s), honouring uncommitted (dirty) cache content.
 * Reads from the staging RAM when the block is dirty, otherwise straight
 * from flash via XIP (fast, safe for reads).
 */
DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    (void) pdrv;
    if (sector + count > (LBA_t) (g_geom.disk_size / DISK_SECTOR_SIZE)) {
        return RES_PARERR;
    }
    while (count > 0) {
        uint32_t abs_offs = g_geom.disk_offset + (uint32_t) sector * DISK_SECTOR_SIZE;
        uint32_t block_start = abs_offs & ~(FLASH_ERASE_SIZE - 1u);
        uint32_t offset_in_block = abs_offs - block_start;

        uint32_t to_read = FLASH_ERASE_SIZE - offset_in_block;
        uint32_t want = (uint32_t) count * DISK_SECTOR_SIZE;
        if (want < to_read) to_read = want;

        cache_slot_t *slot = cache_find(block_start);
        if (slot != NULL && slot->dirty) {
            memcpy(buff, slot->data + offset_in_block, to_read);
        } else {
            /* nothing staged (or already committed): flash == source */
            memcpy(buff, (const uint8_t *) (XIP_BASE + abs_offs), to_read);
        }

        buff += to_read;
        sector += to_read / DISK_SECTOR_SIZE;
        count -= to_read / DISK_SECTOR_SIZE;
    }
    return RES_OK;
}

/*
 * disk_write - Stage sectors into the RAM write-behind cache.
 * NO flash access happens here: commits are performed by disk_cache_service()
 * from the main loop. Only when every RAM slot is dirty do we commit the
 * oldest block synchronously (back-pressure); this is the exceptional path.
 */
DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    (void) pdrv;
    if (sector + count > (LBA_t) (g_geom.disk_size / DISK_SECTOR_SIZE)) {
        return RES_WRPRT;
    }

    while (count > 0) {
        uint32_t abs_offs = g_geom.disk_offset + (uint32_t) sector * DISK_SECTOR_SIZE;
        uint32_t block_start = abs_offs & ~(FLASH_ERASE_SIZE - 1u);
        uint32_t offset_in_block = abs_offs - block_start;

        cache_slot_t *slot = cache_find(block_start);
        if (slot == NULL) {
            /* Find a clean slot; if none, commit the oldest dirty block.
             * A blank block is committed without its (40 ms) erase. */
            slot = NULL;
            for (uint32_t i = 0; i < NCACHE_SLOTS; i++) {
                if (!s_cache[i].dirty) { slot = &s_cache[i]; break; }
            }
            if (slot == NULL) {
                slot = cache_oldest_dirty_blank();
                if (slot == NULL) {
                    slot = cache_oldest_dirty();
                    if (slot == NULL) return RES_ERROR;   /* cannot happen */
                }
                slot_commit(slot, true);                  /* back-pressure */
            }
            cache_load(slot, block_start);
        }

        uint32_t to_write = FLASH_ERASE_SIZE - offset_in_block; /* bytes left in block */
        uint32_t want = (uint32_t) count * DISK_SECTOR_SIZE;
        if (want < to_write) to_write = want;
        memcpy(slot->data + offset_in_block, buff, to_write);
        slot->dirty = true;

        /* advance */
        uint32_t sectors_done = to_write / DISK_SECTOR_SIZE;
        sector += sectors_done;
        count  -= sectors_done;
        buff    += to_write;
    }
    return RES_OK;
}

/*
 * disk_cache_service - Background flusher, called from the main loop right
 * after tud_task() so the USB stack is served between flash commits.
 *
 * Policy (one block per call, keeps the IRQ-off windows short):
 *   - Nothing to do -> returns 0 immediately.
 *   - If USB has been idle for SCSI_IDLE_ERASE_MS or the cache is close to
 *     full, commit the oldest dirty block (erasing it only if needed).
 *   - Otherwise, commit the oldest dirty block that is still blank in flash
 *     (program-only, ~2 ms IRQ-off per page -> safe while writing).
 *
 * Returns the number of committed blocks (0 or 1).
 */
int disk_cache_service(void) {
    uint32_t dirty = cache_dirty_count();
    if (dirty == 0u) return 0;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    bool idle = (now - s_last_scsi_ms >= SCSI_IDLE_ERASE_MS) ||
                (now < s_last_scsi_ms);              /* wraps */

    cache_slot_t *slot = NULL;
    if (idle || dirty >= CACHE_FULL_FLUSH) {
        slot = cache_oldest_dirty();
    } else {
        slot = cache_oldest_dirty_blank();
    }
    if (slot == NULL) return 0;

    slot_commit(slot, idle || dirty >= CACHE_FULL_FLUSH);
    return 1;
}

/*
 * disk_flush - Force all pending writes out of the cache to flash.
 * Called on SYNCHRONIZE_CACHE, CTRL_SYNC, suspend and unmount so everything
 * is durable before the host unmounts / is unplugged.
 */
DRESULT disk_flush(void) {
    int left = 1;
    while (left > 0) {
        left = 0;
        cache_slot_t *slot = cache_oldest_dirty();
        if (slot != NULL) {
            slot_commit(slot, true);
            left = 1;
        }
    }
    return RES_OK;
}

/* Stamp "SCSI activity now" - called at the start of every MSC command so
 * the flusher knows when the USB is idle and can issue a 40 ms erase. */
void disk_scsi_ping(void) {
    s_last_scsi_ms = to_ms_since_boot(get_absolute_time());
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
            /* write-behind cache must be committed before FatFS considers the
             * media synced */
            return disk_flush();

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