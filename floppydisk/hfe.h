#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "fat.h"

/* HFE (HxC v1/v2/v3) disk images — raw per-cylinder bitcell tracks.
 *
 * Supports WRITING an HFE image to a real floppy: the NRZ bitcell stream of
 * each cylinder/side is converted to flux-reversal pulses (units of
 * 1/24 MHz s, i.e. the same units as mfm.cpp / floppy.cpp) which are fed to
 * floppy_write_track().  Reading a real disk *as* HFE (flux -> NRZ sync) is
 * not implemented; reads produce .IMA files.
 *
 * Layout (matches Keir Fraser's hfe.c / mk_hfe.py):
 *   - 512-byte header block: sig, revision, nr_tracks (cylinders),
 *     nr_sides, encoding, bitrate(kbit/s), track_list_offset (in 512B blocks)
 *   - TLUT block(s): 4-byte {offset16, len16} per cylinder (both sides); this
 *     is exactly the layout FlashFloppy and HxCFloppyEmulator use for the
 *     "HXCPICFE" (v1/v2) and "HXCHFEV3" signatures.
 *   - cylinder data: 512B blocks of [256B side0 cells][256B side1 cells].
 *   - v3 embeds byte-aligned opcodes (Nop/Index/Bitrate/SkipBits/Rand) in the
 *     cell stream; handled the same way as hfe_rdata_flux().
 */

typedef struct {
    bool is_v3;
    uint16_t nr_cyls;      /* cylinders (TLUT entries, both sides packed) */
    uint8_t nr_sides;      /* 1 or 2 */
    uint8_t encoding;      /* 0 ISOIBM MFM, 1 Amiga MFM, 2 ISOIBM FM      */
    uint16_t bitrate;      /* kbit/s (data rate, not cell rate)           */
    uint16_t tlut_blocks;  /* track-list offset, in 512B blocks           */
    uint32_t trk_off;      /* data file-block index of current cylinder   */
    uint32_t trk_len;      /* cell-bytes per SIDE of current cylinder     */
    uint32_t cell_ticks;   /* one HFE cell in 24 MHz counts (per bitrate) */
} hfe_t;

/* Parse the header block (byte 0) of an already-opened image file.  The
 * file must have been opened with fat_open_root() with a chain buffer, so
 * later per-cylinder block reads are cheap. */
bool hfe_open(hfe_t *hfe, fat_file_t *f);

/* Move the current-cylinder cursor (loads trk_off / trk_len from the TLUT). */
bool hfe_seek_cyl(hfe_t *hfe, fat_file_t *f, uint32_t cyl);

/* Generate the flux pulses for one full revolution (one pass over the side's
 * cell stream) of the current cylinder.  `pulses` gets up to max_pulses bytes
 * (24 MHz-count units).  Returns the number of pulses generated (0 on error
 * or if the stream does not fit in the buffer). */
size_t hfe_flux_for_side(hfe_t *hfe, fat_file_t *f, int side,
                         uint8_t *pulses, size_t max_pulses);

/* HD vs DD decision used by the app (bitrate >= 400 kbit/s => HD). */
bool hfe_is_hd(const hfe_t *hfe);