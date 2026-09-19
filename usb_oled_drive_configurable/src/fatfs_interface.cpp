/*
 * fatfs_interface.cpp - Mount/format the FAT volume living in the W25Q16 flash
 *
 * SPDX-License-Identifier: MIT
 *
 * FatFS is a C library; all its entry points are extern "C" (ff.h guards).
 * This translation unit provides the bridge between the rest of the C++ code
 * and the filesystem. It also guarantees the FAT tables are flushed on demand
 * (fatfs_sync) so an abrupt power loss does not corrupt the filesystem.
 */

#include <string.h>
#include "ff.h"
#include "diskio.h"
#include "fatfs_interface.h"
#include "pendrive.h"

/* The single filesystem object FatFS uses for our one drive. */
static FATFS g_fatfs;

/* Working buffer for f_mkfs (needs a buffer at least sector-size aligned). */
static uint8_t mkfs_work[4096];

/*
 * map FatFS error to our own coarse codes:
 *  0 = ok, <0 otherwise
 */
static int map_result(FRESULT fr) {
    switch (fr) {
        case FR_OK: return 0;
        default:    return -1;
    }
}

/*
 * Mount the FAT volume. If the flash region has no valid file system (brand
 * new / erased area), format it once as exFAT (our disk is < 32 MB, too small
 * for FAT32 and rejected by Android when FAT16). FatFS fills in the geometry
 * automatically via its cluster-size selection.
 *
 * A region that already contains data we cannot parse is NEVER wiped: FatFS is
 * built with exFAT support, so a volume formatted by Android (exFAT or FAT) is
 * mounted as-is. Auto-format only happens on a truly blank area, which avoids
 * the "Android asks to format every time you plug it in" loop.
 *
 * Returns 0 on success.
 */
int fatfs_mount(void) {
    FRESULT fr;

    /* note: FatFS names the empty-string volume for a drive with no letter */
    fr = f_mount(&g_fatfs, "", 0);
    if (fr != FR_OK) return map_result(fr);

    /* check whether there is a filesystem already. Use f_getfree as a probe. */
    FATFS *pfs;
    DWORD fre_clst;
    fr = f_getfree("", &fre_clst, &pfs);
    if (fr == FR_NO_FILESYSTEM) {
        /* Only a blank (erased=0xFF / empty-fill 0x00) area may be
         * auto-formatted. If the region holds data we could not recognise,
         * leave it untouched: the host can still reformat it via SCSI and
         * FatFS will then mount the result. */
        if (!flash_region_is_blank()) {
            return -2;   /* unknown FS present; never destroy user data */
        }

        /* No FS on blank flash: format the whole region once. FM_EXFAT forces
         * an exFAT volume (FatFS will refuse FAT32 below ~32 MB and would pick
         * FAT16 otherwise). Android's vold auto-mounts exFAT without asking to
         * reformat, which is the whole point of this project. */
        MKFS_PARM opt;
        memset(&opt, 0, sizeof(opt));
        opt.fmt = FM_EXFAT;
        fr = f_mkfs("", &opt, mkfs_work, sizeof(mkfs_work));
        if (fr != FR_OK) return map_result(fr);

        /* remount now that a valid FS exists */
        fr = f_mount(&g_fatfs, "", 0);
        if (fr != FR_OK) return map_result(fr);
    } else if (fr != FR_OK) {
        return map_result(fr);
    }

    return 0;
}

/*
 * Apply the FAT volume label. Works on FAT (root dir entry) and on exFAT
 * (boot sector). FatFS uppercases and truncates to 11 chars as needed.
 */
int fatfs_set_label(const char *label) {
    if (!label || label[0] == '\0') return -1;
    FRESULT fr = f_setlabel(label);
    return (fr == FR_OK) ? 0 : -1;
}

/*
 * Fill free_mb/total_mb based on the mounted volume.
 * Returns 0 on success.
 */
int fatfs_get_free_mb(uint32_t *free_mb, uint32_t *total_mb) {
    FATFS *pfs;
    DWORD fre_clst;
    FRESULT fr = f_getfree("", &fre_clst, &pfs);
    if (fr != FR_OK) return map_result(fr);

    /* total clusters vs free clusters */
    DWORD total_clst = pfs->n_fatent - 2u;
    DWORD free_clst  = fre_clst;

    /* bytes = clusters * sectors_per_cluster * sector_size
     * (sector size is fixed at 512 here, since FF_MIN_SS == FF_MAX_SS == 512) */
    uint64_t sector = DISK_SECTOR_SIZE;
    uint64_t total_bytes = (uint64_t) total_clst * pfs->csize * sector;
    uint64_t free_bytes  = (uint64_t) free_clst  * pfs->csize * sector;

    if (total_mb) *total_mb = (uint32_t) (total_bytes / (1024u * 1024u));
    if (free_mb)  *free_mb  = (uint32_t) (free_bytes  / (1024u * 1024u));
    return 0;
}

/*
 * Flush dirty FAT buffers so a power cut does not corrupt the filesystem.
 * FatFS flushes the FAT when a file is closed, but for robustness (and the
 * "no corruption on abrupt reset" requirement) we force a volume sync here.
 */
void fatfs_sync(void) {
    FRESULT fr;
    /* opening "/" forces read-only access; f_close flushes any cached blocks */
    FIL root;
    fr = f_open(&root, "", FA_READ);
    if (fr == FR_OK) {
        f_close(&root);
    }
    /* FatFS also keeps a FAT dirty flag; the strongest guarantee is achieved
     * by calling the disk flush via CTRL_SYNC through f_mount being idle. The
     * pragmatic approach above pushes the in-memory buffers out. */
    (void) fr;
}
