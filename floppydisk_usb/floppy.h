#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Direct port of the drive-control and flux I/O from Adafruit_Floppy
 * (MIT).  Capture/write use the PIO programs in floppy.pio, whose
 * instruction words are validated at build time against the reference. */

/* ----------------------------------------------------------------- drive ---- */
void floppy_init(void);              /* sets up GPIO, deselects drive           */
void floppy_select(bool selected);   /* DS0, active low                         */
bool floppy_side(int head);          /* head 0 or 1 (HS is inverted)            */
int floppy_get_side(void);
int floppy_track(void);              /* cached head position, -1 if unknown     */
bool floppy_spin_motor(bool on);     /* false if no index pulse seen on on      */
bool floppy_goto_track(int track_num);
void floppy_step(bool dir, uint8_t times);
bool floppy_set_density(bool high);  /* DENSEL: true = high density             */
bool floppy_get_write_protect(void);
bool floppy_get_track0_sense(void);
bool floppy_get_ready_sense(void);   /* DCHG pin, active low                    */

/* ----------------------------------------------------------------- flux ----- */
/* Capture flux between two falling index edges (one revolution + overlap).
 * Returns number of pulse bytes stored (<= max_pulses). */
size_t floppy_capture_track(uint8_t *pulses, size_t max_pulses,
                            int32_t *falling_index_offset, uint32_t capture_ms,
                            uint32_t index_wait_ms);

/* Write flux pulses starting at the falling index edge, stopping at the next
 * one.  Values are in 24 MHz counts and get the OVERHEAD compensation. */
bool floppy_write_track(uint8_t *pulses, size_t n_pulses, bool use_index);