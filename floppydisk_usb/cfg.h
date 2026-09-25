#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Optional per-card configuration read at boot from /FF.CFG on the SD root
 * (INI-style, "#"/";" comments).  Keys:
 *
 *   gap3 = 108     gap after each sector (512B), bytes.  Default 108.
 *                  Lower = tighter tracks; raise if a host rejects writes.
 *   density = auto | hd | dd     default format for HFE images.  "auto"
 *                  infers HD/DD from the image bitrate.  Default auto.
 *
 * There is deliberately no "magic first-line" as some tools use; a missing
 * FF.CFG simply means defaults (FlashFloppy-style leniency).
 */

typedef enum { DENSITY_AUTO = 0, DENSITY_HD, DENSITY_DD } floppy_density_t;

typedef struct {
    bool present;          /* found + parsed at least one key            */
    uint16_t gap3_512;     /* overridden gap3 for 512B sectors (0=none)  */
    floppy_density_t density;
} floppy_cfg_t;

/* Parse /FF.CFG and apply settings (gap3 pushes into the MFM encoder).
 * `fs_impl` is the mounted fat_vfs_t (cast to void*); fat volume must be
 * mounted already.  Returns false if no config file was found. */
bool cfg_load(void *fs_impl, floppy_cfg_t *out);