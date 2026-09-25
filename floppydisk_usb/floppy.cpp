// Floppy drive control + PIO flux capture/write.
//
// Ported from Adafruit_Floppy (MIT, Jeff Epler / Adafruit Industries):
//   - drive control: Adafruit_Floppy.cpp  (select / side / motor / seek / iface)
//   - flux I/O:      arch_rp2.cpp         (fluxread / fluxwrite PIO programs)
// The PIO instruction words are verified here with static_assert against the
// reference words proven on real hardware.

#include "floppy.h"

#include "config.h"
#include "floppy.pio.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/sync.h"

#include <pico/time.h>

#include <cstring>

/* ================================================================== PIO ==== */

/* Reference instruction words, exported from arch_rp2.cpp. */
static constexpr uint16_t fluxread_ref[] = {
    0x0041, /* jmp x--, wait_one_next              */
    0x00c3, /* jmp pin wait_zero                   */
    0x0000, /* jmp wait_one                        */
    0x0044, /* jmp x--, wait_zero_next             */
    0x01c3, /* jmp pin wait_zero [1]               */
    0x4001, /* in pins, 1                          */
    0x402f, /* in x, 15                            */
    0x0040, /* jmp x--, wait_one                   */
};
static constexpr uint16_t fluxwrite_ref[] = {
    0xe000, /* set pins, 0                         */
    0x6030, /* out x, 16                           */
    0xae42, /* nop [14]                            */
    0xe001, /* set pins, 1                         */
    0x0044, /* jmp x--, loop_high                  */
    0x0000, /* jmp loop_flux                       */
};

/* ============================================================ drive GPIO ===== */

static int g_track = -1;
static int g_side = -1;

void floppy_init(void) {
    /* inputs (open-collector drive outputs) */
    gpio_init(FLOPPY_INDEX_PIN);
    gpio_pull_up(FLOPPY_INDEX_PIN);
    gpio_set_dir(FLOPPY_INDEX_PIN, GPIO_IN);

    gpio_init(FLOPPY_RDATA_PIN);
    gpio_pull_up(FLOPPY_RDATA_PIN);
    gpio_set_dir(FLOPPY_RDATA_PIN, GPIO_IN);

    gpio_init(FLOPPY_TRK0_PIN);
    gpio_pull_up(FLOPPY_TRK0_PIN);
    gpio_set_dir(FLOPPY_TRK0_PIN, GPIO_IN);

    gpio_init(FLOPPY_WPT_PIN);
    gpio_pull_up(FLOPPY_WPT_PIN);
    gpio_set_dir(FLOPPY_WPT_PIN, GPIO_IN);

    gpio_init(FLOPPY_DCHG_PIN);
    gpio_pull_up(FLOPPY_DCHG_PIN);
    gpio_set_dir(FLOPPY_DCHG_PIN, GPIO_IN);

    /* write data / gate OFF */
    gpio_init(FLOPPY_WDATA_PIN);
    gpio_set_dir(FLOPPY_WDATA_PIN, GPIO_OUT);
    gpio_put(FLOPPY_WDATA_PIN, 1);

    gpio_init(FLOPPY_WG_PIN);
    gpio_set_dir(FLOPPY_WG_PIN, GPIO_IN);
    gpio_pull_up(FLOPPY_WG_PIN);

    /* outputs: drive deselected, motor off, inward, step idle */
    gpio_init(FLOPPY_DS0_PIN);
    gpio_init(FLOPPY_DS1_PIN);
    gpio_init(FLOPPY_MOTOR_PIN);
    gpio_init(FLOPPY_DIR_PIN);
    gpio_init(FLOPPY_STEP_PIN);
    gpio_init(FLOPPY_HS_PIN);
    gpio_init(FLOPPY_DENSEL_PIN);

    gpio_put(FLOPPY_DS0_PIN, 1);  /* deselected (active low) */
    gpio_put(FLOPPY_DS1_PIN, 1);  /* deselected               */
    gpio_put(FLOPPY_MOTOR_PIN, 1);/* motor off (active low)   */
    gpio_put(FLOPPY_DIR_PIN, 0);  /* inward                   */
    gpio_put(FLOPPY_STEP_PIN, 1); /* step idle                */
    gpio_put(FLOPPY_HS_PIN, 1);   /* side 0                   */
    gpio_put(FLOPPY_DENSEL_PIN, 0);/* low density              */

    gpio_set_dir_out_masked((1u << FLOPPY_DS0_PIN) | (1u << FLOPPY_DS1_PIN) |
                            (1u << FLOPPY_MOTOR_PIN) | (1u << FLOPPY_DIR_PIN) |
                            (1u << FLOPPY_STEP_PIN) | (1u << FLOPPY_HS_PIN) |
                            (1u << FLOPPY_DENSEL_PIN));

    g_track = -1;
    g_side = -1;
}

void floppy_select(bool selected) {
    gpio_put(FLOPPY_DS0_PIN, selected ? 0 : 1);
    busy_wait_us(FLOPPY_SELECT_DELAY_US);
}

bool floppy_side(int head) {
    if (head != 0 && head != 1) return false;
    g_side = head;
    gpio_put(FLOPPY_HS_PIN, head ? 0 : 1); /* head 0 = logic 1 */
    return true;
}

int floppy_get_side(void) { return g_side; }
int floppy_track(void) { return g_track; }

bool floppy_get_write_protect(void) { return !gpio_get(FLOPPY_WPT_PIN); }
bool floppy_get_track0_sense(void) { return !gpio_get(FLOPPY_TRK0_PIN); }
bool floppy_get_ready_sense(void) { return !gpio_get(FLOPPY_DCHG_PIN); }

bool floppy_set_density(bool high) {
    gpio_put(FLOPPY_DENSEL_PIN, high ? 1 : 0);
    return true;
}

void floppy_step(bool dir, uint8_t times) {
    gpio_put(FLOPPY_DIR_PIN, dir ? 1 : 0);
    busy_wait_us(10);
    while (times--) {
        gpio_put(FLOPPY_STEP_PIN, 1);
        busy_wait_us(FLOPPY_STEP_DELAY_US);
        gpio_put(FLOPPY_STEP_PIN, 0);
        busy_wait_us(FLOPPY_STEP_DELAY_US);
        gpio_put(FLOPPY_STEP_PIN, 1); /* end high */
    }
    busy_wait_us(FLOPPY_STEP_DELAY_US); /* one more for 5.25" drives */
}

bool floppy_spin_motor(bool on) {
    gpio_put(FLOPPY_MOTOR_PIN, on ? 0 : 1); /* motor on = logic 0 */
    if (!on) return true;
    busy_wait_ms(FLOPPY_MOTOR_DELAY_MS);
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (gpio_get(FLOPPY_INDEX_PIN)) {
        if (to_ms_since_boot(get_absolute_time()) - start > 1000) return false;
    }
    return true;
}

bool floppy_goto_track(int track_num) {
    /* track 0: step out until the (active-low) TRK0 sensor asserts */
    if (g_track < 0 || track_num == 0) {
        uint8_t max_steps = 100;
        while (max_steps--) {
            if (floppy_get_track0_sense()) {
                g_track = 0;
                break;
            }
            floppy_step(true /* STEP_OUT */, 1);
        }
        if (!floppy_get_track0_sense()) {
            max_steps = 20;
            while (max_steps--) {
                if (floppy_get_track0_sense()) {
                    g_track = 0;
                    break;
                }
                floppy_step(false /* STEP_IN */, 1);
            }
            if (!floppy_get_track0_sense()) return false;
        }
    }
    busy_wait_ms(FLOPPY_SETTLE_DELAY_MS);

    if (track_num > FLOPPY_HD_TRACKS - 1) track_num = FLOPPY_HD_TRACKS - 1;
    if (g_track == track_num) return true;

    int steps = track_num - g_track;
    if (steps > 0) {
        floppy_step(false /* STEP_IN */, (uint8_t)steps);
    } else {
        floppy_step(true /* STEP_OUT */, (uint8_t)(-steps));
    }
    busy_wait_ms(FLOPPY_SETTLE_DELAY_MS);
    g_track = track_num;
    return true;
}

/* ====================================================== flux capture ========= */

typedef struct {
    PIO pio;
    uint sm;
    uint16_t offset;
    uint16_t half;
} floppy_reader_t;

static floppy_reader_t g_floppy_reader;

static bool init_capture(void) {
    if (g_floppy_reader.pio) return true;

    PIO pio = pio0;
    if (!pio_can_add_program(pio, &fluxread_program)) return false;
    uint sm = pio_claim_unused_sm(pio, false);
    if (sm == (uint)-1) return false;
    uint offset = pio_add_program(pio, &fluxread_program);

    g_floppy_reader.pio = pio;
    g_floppy_reader.sm = sm;
    g_floppy_reader.offset = (uint16_t)offset;

    gpio_pull_up(FLOPPY_INDEX_PIN);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + fluxread_program.length - 1);
    sm_config_set_jmp_pin(&c, FLOPPY_RDATA_PIN);
    sm_config_set_in_pins(&c, FLOPPY_INDEX_PIN);
    sm_config_set_in_shift(&c, true, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    float div = (float)clock_get_hz(clk_sys) / (3 * (float)MFM_SAMPLE_FREQ);
    sm_config_set_clkdiv(&c, div);
    pio_sm_set_pins_with_mask(pio, sm, 1u << FLOPPY_RDATA_PIN,
                              1u << FLOPPY_RDATA_PIN);

    pio_sm_init(pio, sm, offset, &c);
    return true;
}

static void start_common(void) {
    pio_sm_exec(g_floppy_reader.pio, g_floppy_reader.sm,
                g_floppy_reader.offset);
    pio_sm_restart(g_floppy_reader.pio, g_floppy_reader.sm);
}

static bool data_available(void) {
    return g_floppy_reader.half ||
           !pio_sm_is_rx_fifo_empty(g_floppy_reader.pio, g_floppy_reader.sm);
}

static uint16_t read_fifo(void) {
    if (g_floppy_reader.half) {
        uint16_t result = g_floppy_reader.half;
        g_floppy_reader.half = 0;
        return result;
    }
    uint32_t value = pio_sm_get_blocking(g_floppy_reader.pio, g_floppy_reader.sm);
    g_floppy_reader.half = value >> 16;
    return value & 0xffff;
}

static uint8_t *capture_foreground(uint8_t *start, uint8_t *end,
                                   int32_t *falling_index_offset,
                                   uint32_t capture_counts,
                                   uint32_t max_wait_time) {
    uint8_t *ptr = start;
    if (falling_index_offset) *falling_index_offset = -1;
    start_common();

    /* wait for a falling edge of the index pulse before enabling capture */
    if (max_wait_time) {
        uint32_t start_time = to_ms_since_boot(get_absolute_time());
        while (!gpio_get(FLOPPY_INDEX_PIN)) {
            if (to_ms_since_boot(get_absolute_time()) - start_time >
                max_wait_time) {
                pio_sm_set_enabled(g_floppy_reader.pio, g_floppy_reader.sm,
                                   false);
                return ptr;
            }
        }
        while (gpio_get(FLOPPY_INDEX_PIN)) {
            if (to_ms_since_boot(get_absolute_time()) - start_time >
                max_wait_time) {
                pio_sm_set_enabled(g_floppy_reader.pio, g_floppy_reader.sm,
                                   false);
                return ptr;
            }
        }
    }

    uint32_t total_counts = 0;

    uint32_t saved_irq = save_and_disable_interrupts();
    pio_sm_clear_fifos(g_floppy_reader.pio, g_floppy_reader.sm);
    pio_sm_set_enabled(g_floppy_reader.pio, g_floppy_reader.sm, true);
    int last = read_fifo();
    bool last_index = gpio_get(FLOPPY_INDEX_PIN);
    while (ptr != end) {
        bool now_index = gpio_get(FLOPPY_INDEX_PIN);
        if (!now_index && last_index) {
            if (falling_index_offset) {
                *falling_index_offset = ptr - start;
                if (!capture_counts) break;
            }
        }
        last_index = now_index;

        if (!data_available()) continue;

        int data = read_fifo();
        int delta = last - data;
        if (delta < 0) delta += 65536;
        delta /= 2;

        last = data;
        total_counts += delta;
        *ptr++ = delta > 255 ? 255 : (uint8_t)delta;
        if (capture_counts != 0 && total_counts >= capture_counts) break;
    }
    restore_interrupts(saved_irq);

    pio_sm_set_enabled(g_floppy_reader.pio, g_floppy_reader.sm, false);
    return ptr;
}

static void free_capture(void) {
    if (!g_floppy_reader.pio) return;
    pio_sm_set_enabled(g_floppy_reader.pio, g_floppy_reader.sm, false);
    pio_sm_unclaim(g_floppy_reader.pio, g_floppy_reader.sm);
    pio_remove_program(g_floppy_reader.pio, &fluxread_program,
                       g_floppy_reader.offset);
    memset(&g_floppy_reader, 0, sizeof(g_floppy_reader));
}

size_t floppy_capture_track(uint8_t *pulses, size_t max_pulses,
                            int32_t *falling_index_offset, uint32_t capture_ms,
                            uint32_t index_wait_ms) {
    memset(pulses, 0, max_pulses);
    if (!init_capture()) return 0;
    uint8_t *e =
        capture_foreground(pulses, pulses + max_pulses, falling_index_offset,
                           capture_ms * (MFM_SAMPLE_FREQ / 1000), index_wait_ms);
    size_t result = (size_t)(e - pulses);
    free_capture();
    return result;
}

/* ======================================================== flux write ======== */

#define OVERHEAD (20) /* min pulse length due to PIO overhead, ~0.833 us */

typedef struct {
    PIO pio;
    uint sm;
    uint16_t offset;
} floppy_writer_t;

static floppy_writer_t g_floppy_writer;

static bool init_write(void) {
    if (g_floppy_writer.pio) return true;

    PIO pio = pio0;
    if (!pio_can_add_program(pio, &fluxwrite_program)) return false;
    uint sm = pio_claim_unused_sm(pio, false);
    if (sm == (uint)-1) return false;
    uint offset = pio_add_program(pio, &fluxwrite_program);

    g_floppy_writer.pio = pio;
    g_floppy_writer.sm = sm;
    g_floppy_writer.offset = (uint16_t)offset;

    uint32_t wrdata_bit = 1u << FLOPPY_WDATA_PIN;

    pio_gpio_init(pio, FLOPPY_WDATA_PIN);
    pio_sm_set_pindirs_with_mask(pio, sm, wrdata_bit, wrdata_bit);
    pio_sm_set_pins_with_mask(pio, sm, wrdata_bit, wrdata_bit);
    pio_sm_set_pins_with_mask(pio, sm, 0, wrdata_bit);
    pio_sm_set_pins_with_mask(pio, sm, wrdata_bit, wrdata_bit);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + fluxwrite_program.length - 1);
    sm_config_set_set_pins(&c, FLOPPY_WDATA_PIN, 1);
    sm_config_set_out_shift(&c, true, true, 16);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    float div = (float)clock_get_hz(clk_sys) / (float)MFM_SAMPLE_FREQ;
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    return true;
}

/* Wait until the index pulse is idle, then pass its falling edge.  Returns
 * false (and aborts the write) if the drive has no index signal. */
static bool wait_for_index(void) {
    uint32_t t0 = to_ms_since_boot(get_absolute_time());
    /* don't start inside an index pulse */
    while (!gpio_get(FLOPPY_INDEX_PIN)) {
        if (to_ms_since_boot(get_absolute_time()) - t0 > 1000) return false;
    }
    /* wait for a falling edge of index */
    while (gpio_get(FLOPPY_INDEX_PIN)) {
        if (to_ms_since_boot(get_absolute_time()) - t0 > 1000) return false;
    }
    return true;
}

static bool write_foreground(uint8_t *pulses, uint8_t *pulse_end,
                             bool use_index) {
    if (use_index && !wait_for_index()) return false;

    gpio_init(FLOPPY_WG_PIN);
    gpio_set_dir(FLOPPY_WG_PIN, GPIO_OUT);
    gpio_put(FLOPPY_WG_PIN, 0);
    /* WGATE debounce/settle: hold the pen down for a few microseconds before
     * clocking data, so drives with WGATE glitch filters latch cleanly. */
    busy_wait_us(FLOPPY_WG_SETUP_US);

    uint32_t saved_irq = save_and_disable_interrupts();
    pio_sm_set_enabled(g_floppy_writer.pio, g_floppy_writer.sm, false);
    pio_sm_clear_fifos(g_floppy_writer.pio, g_floppy_writer.sm);
    pio_sm_exec(g_floppy_writer.pio, g_floppy_writer.sm, g_floppy_writer.offset);
    while (!pio_sm_is_tx_fifo_full(g_floppy_writer.pio, g_floppy_writer.sm)) {
        unsigned value = *pulses++;
        value = (value < OVERHEAD) ? 1 : value - OVERHEAD;
        pio_sm_put_blocking(g_floppy_writer.pio, g_floppy_writer.sm, value);
        if (pulses == pulse_end) break;
    }
    pio_sm_set_enabled(g_floppy_writer.pio, g_floppy_writer.sm, true);

    bool old_index_state = false;
    while (pulses != pulse_end) {
        bool index_state = gpio_get(FLOPPY_INDEX_PIN);
        if (old_index_state && !index_state) {
            /* one revolution done */
            break;
        }
        while (!pio_sm_is_tx_fifo_full(g_floppy_writer.pio, g_floppy_writer.sm)) {
            unsigned value = *pulses++;
            value = (value < OVERHEAD) ? 1 : value - OVERHEAD;
            pio_sm_put_blocking(g_floppy_writer.pio, g_floppy_writer.sm, value);
            if (pulses == pulse_end) break;
        }
        old_index_state = index_state;
    }
    restore_interrupts(saved_irq);

    pio_sm_set_enabled(g_floppy_writer.pio, g_floppy_writer.sm, false);
    gpio_init(FLOPPY_WG_PIN);
    gpio_set_dir(FLOPPY_WG_PIN, GPIO_IN);
    gpio_pull_up(FLOPPY_WG_PIN);
    busy_wait_us(FLOPPY_WG_SETUP_US);
    return true;
}

static void free_write(void) {
    if (!g_floppy_writer.pio) return;
    pio_sm_set_enabled(g_floppy_writer.pio, g_floppy_writer.sm, false);
    pio_sm_unclaim(g_floppy_writer.pio, g_floppy_writer.sm);
    pio_remove_program(g_floppy_writer.pio, &fluxwrite_program,
                       g_floppy_writer.offset);
    memset(&g_floppy_writer, 0, sizeof(g_floppy_writer));
}

bool floppy_write_track(uint8_t *pulses, size_t n_pulses, bool use_index) {
    if (!init_write()) return false;
    bool ok = write_foreground(pulses, pulses + n_pulses, use_index);
    free_write();
    return ok;
}