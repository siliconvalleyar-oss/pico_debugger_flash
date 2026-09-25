#include "hfe.h"

#include <string.h>

/* hfe.cpp — HxC "HXCPICFE"/"HXCHFEV3" (v1/v2/v3) image support.
 *
 * The conversion written here mirrors hfe_rdata_flux() from FlashFloppy
 * (public domain, Keir Fraser); the only intentional difference is the
 * output pulse unit, which is a 24 MHz count matching mfm.cpp & floppy.cpp.
 */

/* HFEv3 opcodes.  Values as they appear in the file (bit order reversed,
 * matching the raw HFE bit order, exactly like FlashFloppy's hfe.c). */
enum {
    OP_Nop = 0x0f, OP_Index = 0x8f, OP_Bitrate = 0x4f,
    OP_SkipBits = 0xcf, OP_Rand = 0x2f
};

static inline uint16_t le16u(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint8_t rev8(uint8_t b) {
    b = (uint8_t)(((b & 0x55) << 1) | ((b >> 1) & 0x55));
    b = (uint8_t)(((b & 0x33) << 2) | ((b >> 2) & 0x33));
    b = (uint8_t)(((b & 0x0f) << 4) | ((b >> 4) & 0x0f));
    return b;
}

bool hfe_open(hfe_t *hfe, fat_file_t *f) {
    memset(hfe, 0, sizeof(*hfe));

    uint8_t h[512];
    if (!fat_read_block(f, 0, h)) return false;

    if (memcmp(h, "HXCPICFE", 8) == 0) {
        if (h[8] > 1) return false; /* revision 0=v1, 1=v2 */
        hfe->is_v3 = false;
    } else if (memcmp(h, "HXCHFEV3", 8) == 0) {
        if (h[8] != 0) return false;
        hfe->is_v3 = true;
    } else {
        return false;
    }

    hfe->nr_cyls = (uint16_t)h[9];   /* TLUT entries (= cylinders) */
    hfe->nr_sides = h[10];
    hfe->encoding = h[11];
    hfe->bitrate = le16u(h + 12);
    hfe->tlut_blocks = le16u(h + 18);

    if (hfe->nr_sides < 1 || hfe->nr_sides > 2 || hfe->bitrate == 0 ||
        hfe->nr_cyls == 0) {
        return false;
    }

    /* Cell duration in 24 MHz counts.
     *   MFM: cell clock = 2 x data rate  -> 24e6 / (bitrate*1000*2)
     *   FM : cell clock = 1 x data rate  -> 24e6 / (bitrate*1000)
     * Amiga MFM shares the ISO-MFM cell clock (two cells per data bit).   */
    uint32_t div = hfe->encoding == 2 ? 1u : 2u;
    hfe->cell_ticks = 24000u / ((uint32_t)hfe->bitrate * div);
    if (hfe->cell_ticks == 0) return false;
    return true;
}

bool hfe_seek_cyl(hfe_t *hfe, fat_file_t *f, uint32_t cyl) {
    if (cyl >= hfe->nr_cyls) return false;
    uint8_t t[512];
    /* TLUT lives in block(s) after the header; each entry is 4 bytes. */
    uint32_t tlut_block = (uint32_t)hfe->tlut_blocks + cyl / 128;
    if (!fat_read_block(f, tlut_block, t)) return false;
    uint32_t off = (cyl % 128) * 4;
    hfe->trk_off = le16u(t + off);          /* in 512B blocks            */
    hfe->trk_len = le16u(t + off + 2) / 2;  /* cell-bytes per side        */
    if (hfe->trk_off == 0) return false;
    return true;
}

bool hfe_is_hd(const hfe_t *hfe) { return hfe->bitrate >= 400u; }

/* Small deterministic PRNG (OP_Rand) — static, fine at this scale. */
static uint32_t hfe_rand_state = 0x12345678u;
static uint8_t hfe_rand8(void) {
    hfe_rand_state = hfe_rand_state * 1664525u + 1013904223u;
    return (uint8_t)(hfe_rand_state >> 24);
}

/* Append a flux pulse with accumulated gap, splitting runs > 255 so the
 * 8-bit pulse encoding matches the reader/encoder convention. */
static size_t hfe_push(uint8_t *pulses, size_t max, size_t np, uint32_t *t) {
    while (*t > 255u) {
        if (np >= max) return 0;
        pulses[np++] = 255u;
        *t -= 255u;
    }
    if (*t > 0) {
        if (np >= max) return 0;
        pulses[np++] = (uint8_t)*t;
    }
    *t = 0;
    return np;
}

size_t hfe_flux_for_side(hfe_t *hfe, fat_file_t *f, int side,
                         uint8_t *pulses, size_t max_pulses) {
    if (side < 0 || side >= 2) return 0;
    if (!hfe->trk_off || !hfe->trk_len) return 0;

    uint32_t cell = hfe->cell_ticks;
    uint32_t t = 0;          /* time since last flux pulse, in cells */
    size_t np = 0;
    uint32_t nblk = (hfe->trk_len + 255u) / 256u;
    uint8_t b[512];

    for (uint32_t j = 0; j < nblk; j++) {
        if (!fat_read_block(f, hfe->trk_off + j, b)) return 0;
        const uint8_t *cells = b + (side ? 256 : 0);
        uint32_t nb = (j + 1 == nblk) ? (hfe->trk_len - j * 256u) : 256u;
        for (uint32_t k = 0; k < nb; k++) {
            uint8_t x = cells[k];
            if (hfe->is_v3 && (x & 0x0f) == 0x0f) {
                switch (x) {
                default:                     /* OP_Nop / OP_Index / rsvd */
                    continue;
                case OP_Bitrate: {
                    /* next byte (bit-reversed) = new cell half-clock */
                    uint8_t arg = 0xff;
                    if (k + 1 < nb) {
                        arg = cells[k + 1];
                    } else if (j + 1 < nblk) {
                        if (!fat_read_block(f, hfe->trk_off + j + 1, b))
                            return 0;
                        arg = b[side ? 256 : 0];
                    }
                    cell = (2u * rev8(arg)) / 3u;
                    if (cell == 0) cell = 1;
                    k++;
                    continue;
                }
                case OP_SkipBits: {
                    /* next byte (reversed) & 7 = skip bits in following byte */
                    uint8_t arg = 0xff;
                    if (k + 1 < nb) {
                        arg = cells[k + 1];
                    } else if (j + 1 < nblk) {
                        if (!fat_read_block(f, hfe->trk_off + j + 1, b))
                            return 0;
                        arg = b[side ? 256 : 0];
                    }
                    uint8_t skip = rev8(arg) & 7u;
                    k++; /* consumed arg byte */
                    if (k >= nb) continue; /* nothing left this block */
                    x = cells[k];
                    /* skip the first `skip` cells of the following byte,
                     * emit flux for the remaining (8-skip) cells */
                    for (uint8_t bit = skip; bit < 8; bit++) {
                        t += cell;
                        if (x & (1u << bit)) {
                            size_t r = hfe_push(pulses, max_pulses, np, &t);
                            if (!r) return 0;
                            np = r;
                        }
                    }
                    continue;
                }
                case OP_Rand:
                    x = hfe_rand8();
                    break;
                }
            }
            /* data cells: 8 bits, LSB first (raw HFE byte order) */
            for (uint8_t bit = 0; bit < 8; bit++) {
                t += cell;
                if (x & (1u << bit)) {
                    size_t r = hfe_push(pulses, max_pulses, np, &t);
                    if (!r) return 0;
                    np = r;
                }
            }
        }
    }
    return np;
}