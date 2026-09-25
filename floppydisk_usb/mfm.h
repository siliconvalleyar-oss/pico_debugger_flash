#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* MFM flux encoding/decoding for standard 512-byte-sector IBM tracks.
 *
 * Faithful port of the MIT-licensed logic from the Adafruit_Floppy library
 * (src/mfm_impl.h, Jeff Epler / Adafruit Industries), see docs/firmware.md.
 */

typedef struct {
    uint16_t t1_nom; /* nominal 1T gap, in 24 MHz counts          */
    uint16_t t2_max; /* 2T class boundary (2.5 * t1_nom)          */
    uint16_t t3_max; /* 3T class boundary (3.5 * t1_nom)          */
} mfm_timings_t;

/* Derive the thresholds exactly as Adafruit does:
 *   t1_nom = round(sample * bit_time_us * 1e-6)   (HD:1.0us DD:2.0us)
 *   t2_max = round(sample * bit_time_us * 2.5e-6)
 *   t3_max = round(sample * bit_time_us * 3.5e-6)
 */
void mfm_timings(mfm_timings_t *t, float nominal_bit_time_us,
                 uint32_t sample_freq_hz);

/* Override the post-sector gap (gap3) used for 512-byte sectors (default 108).
 * Out-of-range values are clamped.  Called from cfg_load after parsing FF.CFG. */
void mfm_set_gap3_512(uint16_t gap3);

/* Decode a captured track.  Returns the number of valid sectors (0..n).
 * `sectors` is 512*n_sectors bytes; `validity[]` is cleared iff
 * `clear_validity`. `logical_track` receives the cylinder byte of the last
 * decoded IDAM. */
size_t mfm_decode_track(const uint8_t *pulses, size_t n_pulses,
                        uint8_t *sectors, size_t n_sectors,
                        uint8_t *validity, const mfm_timings_t *t,
                        bool clear_validity, uint8_t *logical_track);

/* Encode one track of `n_sectors` x 512-byte sectors into flux pulses.
 * Fills `pulses[0..max_pulses)` (tailing gap bytes after the last sector).
 * Returns the number of pulse bytes produced (= max_pulses unless n==0). */
size_t mfm_encode_track(const uint8_t *sectors, size_t n_sectors,
                        uint8_t cylinder, uint8_t head, uint8_t *pulses,
                        size_t max_pulses, const mfm_timings_t *t);