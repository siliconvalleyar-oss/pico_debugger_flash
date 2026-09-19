/*
 * fatfs_interface.h - FatFS mounting, formatting and free-space helpers
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _FATFS_INTERFACE_H_
#define _FATFS_INTERFACE_H_

#include "pendrive.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mount the FAT volume; formats it once if it has no valid filesystem. */
int fatfs_mount(void);

/* Apply the volume label (FAT/exFAT) so the host shows, e.g., "MiPendrive". */
int fatfs_set_label(const char *label);

/* Fill free_mb/total_mb with space info from the mounted volume. */
int fatfs_get_free_mb(uint32_t *free_mb, uint32_t *total_mb);

/* Flush any dirty FAT buffers (f_sync / volume sync) so a power loss does not
 * corrupt the FAT tables. */
void fatfs_sync(void);

#ifdef __cplusplus
}
#endif

#endif /* _FATFS_INTERFACE_H_ */
