// bus_pio.cpp - backend del bus SSD1963 sobre PIO + DMA.
//
// Reparto de trabajo en cada rafaga:
//   * el PIO genera los pulsos de /WR,
//   * la DMA mete las palabras de 32 bits en la FIFO de salida,
//   * el software pone D/C y /CS y comprueba el final con pio_sm_get_pc().
//
// El final de cada rafaga lo decide el propio programa PIO (contador Y), de
// forma que nunca se mete un pixel de mas en la ventana ni se pierde el
// ultimo, y no hace falta ninguna espera por tiempo.

#include "bus_pio.h"

#include <string.h>

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

extern "C" {
#include "tft.pio.h"
}

namespace tft {
namespace {

constexpr uint32_t kBytesPerWord = 4;
constexpr uint32_t kBytesPerPixel = (TFT_BUS_WIDTH == 8) ? 3u : 2u;

// El panel solo captura un pixel por pulso de /WR, asi que una rafaga por DMA
// tiene que caer en multiplos de 4 pixeles (8 bits) o de 2 (16 bits) para no
// partir un pixel por la mitad.  Los ultimos pixels, que no llenan una palabra
// de 32 bits, van por el programa corto.
constexpr uint32_t kPixelsPerWord8 = 4;  // 4 pixeles = 12 bytes = 3 palabras

// Palabras por bloque de DMA.  En 8 bits tiene que ser multiplo de 3, porque
// el patron de bytes son 3 por pixel y el bloque se repite entero.
#if TFT_BUS_WIDTH == 8
constexpr uint32_t kPixelsPerWord = kPixelsPerWord8;
constexpr uint32_t kWordsPerDmaBlock = 255;  // 1020 bytes = 340 pixeles
constexpr uint32_t kPixelsPerDmaBlock = kWordsPerDmaBlock * kBytesPerWord / kBytesPerPixel;
constexpr uint32_t kFillPatternBytes = 12;    // 3 palabras = 4 pixeles
#else
constexpr uint32_t kWordsPerDmaBlock = TFT_TX_BUF_BYTES / kBytesPerWord;
constexpr uint32_t kPixelsPerDmaBlock = kWordsPerDmaBlock * kBytesPerWord / kBytesPerPixel;
constexpr uint32_t kFillPatternBytes = 4;     // 1 palabra = 2 pixeles
#endif

inline uint32_t rgb565(Rgb c) {
    return (static_cast<uint32_t>(c.r >> 3) << 11) |
           (static_cast<uint32_t>(c.g >> 2) << 5) |
           static_cast<uint32_t>(c.b >> 3);
}

}  // namespace

PioBus::PioBus() = default;
PioBus::~PioBus() = default;

void PioBus::begin() {
    if (initialised_) return;
    initialised_ = true;

    sm_ = static_cast<uint8_t>(pio_claim_unused_sm(pio0, true));
    dma_ch_ = static_cast<uint8_t>(dma_claim_unused_channel(false));

    // Programa de una palabra de bus (comandos y parametros).  El final de
    // la transmision es la direccion del pull del bloque `end`, que se lee
    // del propio header generado por el ensamblador.
    int off = pio_add_program(pio0, &tft_cmd_program);
    hard_assert(off >= 0);
    cmd_offset_ = static_cast<uint8_t>(off);
    cmd_halt_pc_ = static_cast<uint8_t>(cmd_offset_ + tft_cmd_offset_end);
    // El bloque `end` tiene que seguir siendo el pull donde para la state
    // machine: si se toca tft.pio y ya no lo es, aqui salta.
    hard_assert((tft_cmd_program.instructions[tft_cmd_offset_end] & 0xe000u) == 0x8000u);

    // Programa de rafaga por DMA.
#if TFT_BUS_WIDTH == 8
    off = pio_add_program(pio0, &tft_data8_program);
    data_halt_pc_ = static_cast<uint8_t>(tft_data8_offset_end);
#else
    off = pio_add_program(pio0, &tft_data16_program);
    data_halt_pc_ = static_cast<uint8_t>(tft_data16_offset_end);
#endif
    hard_assert(off >= 0);
    data_offset_ = static_cast<uint8_t>(off);
    data_halt_pc_ = static_cast<uint8_t>(data_offset_ + data_halt_pc_);
#if TFT_BUS_WIDTH == 8
    hard_assert((tft_data8_program.instructions[tft_data8_offset_end] & 0xe000u) == 0x8000u);
#else
    hard_assert((tft_data16_program.instructions[tft_data16_offset_end] & 0xe000u) == 0x8000u);
#endif

    // D/C y /CS los lleva la CPU.  /CS en alto: el panel no debe ver nada
    // hasta que la state machine haya puesto /WR en reposo.
    gpio_init(TFT_PIN_DC);
    gpio_set_dir(TFT_PIN_DC, GPIO_OUT);
    gpio_put(TFT_PIN_DC, 1);

    gpio_init(TFT_PIN_CS);
    gpio_set_dir(TFT_PIN_CS, GPIO_OUT);
    gpio_put(TFT_PIN_CS, 1);

    // Pines de datos: grupo contiguo del PIO.
    for (int i = 0; i < TFT_BUS_WIDTH; ++i) {
        pio_gpio_init(pio0, TFT_PIN_D0 + i);
    }

#if TFT_BL_PWM_ENABLE
    gpio_set_function(TFT_PIN_BL, GPIO_FUNC_PWM);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, 255);
    pwm_init(pwm_gpio_to_slice_num(TFT_PIN_BL), &cfg, true);
    pwm_set_gpio_level(TFT_PIN_BL, 0);
#endif
}

void PioBus::delay_ms(uint32_t ms) { sleep_ms(ms); }

void PioBus::reset() {
    gpio_init(TFT_PIN_RST);
    gpio_set_dir(TFT_PIN_RST, GPIO_OUT);

    gpio_put(TFT_PIN_RST, 0);
    sleep_ms(20);
    gpio_put(TFT_PIN_RST, 1);
    sleep_ms(120);
}

// ---------------------------------------------------------------------------
// Arranque y final de una transmision
// ---------------------------------------------------------------------------
void PioBus::start_program(bool cmd, bool dc) {
    const uint8_t offset = cmd ? cmd_offset_ : data_offset_;
    pio_sm_config cfg = tft_cmd_program_get_default_config(offset);
#if TFT_BUS_WIDTH == 16
    if (!cmd) cfg = tft_data16_program_get_default_config(offset);
#else
    if (!cmd) cfg = tft_data8_program_get_default_config(offset);
#endif
    cfg = tft_program_init_config(cfg, TFT_PIN_D0, TFT_PIN_WR);
    pio_sm_init(pio0, sm_, offset, &cfg);

    // Al salir de pio_sm_init el side-set vale 0, o sea /WR asserted, pero /CS
    // sigue en alto (lo lleva la CPU): no entra nada en el panel.  Al
    // habilitar la state machine su primer side-set ya deja /WR en reposo y se
    // bloquea en el pull del contador; a partir de ahi se puede bajar /CS.
    pio_sm_set_enabled(pio0, sm_, true);
    gpio_put(TFT_PIN_DC, dc ? 1 : 0);
    gpio_put(TFT_PIN_CS, 0);
}

void PioBus::wait_done(uint8_t halt_pc, uint32_t words) {
    // En cuanto el contador Y llega a cero el programa cae en el bloque `end`
    // y se bloquea en su pull: en ese punto el ultimo pixel ya esta en el
    // panel y no sale ni un byte de mas.
    //
    // El PC es la senal de fin, no un temporizador.  El contador de abajo es
    // solo una red de seguridad: con 16 ciclos de reloj por palabra como
    // presupuesto, de sobra para la state machine, asi que solo dispara si el
    // hardware se queda colgado o si tft.pio y el C++ dejan de cuadrar.
    uint32_t budget = (words + 8u) * 32u;
    while (pio_sm_get_pc(pio0, sm_) != halt_pc) {
        if (--budget == 0) {
            hard_assert(("la state machine no ha llegado al final (pc=%u, halt=%u, "
                         "palabras=%u); revisa tft.pio y los pines"),
                        pio_sm_get_pc(pio0, sm_), halt_pc, words);
        }
    }
    gpio_put(TFT_PIN_CS, 1);
    pio_sm_set_enabled(pio0, sm_, false);
}

// ---------------------------------------------------------------------------
// Bytes sueltos (comandos, parametros y cola de una rafaga)
// ---------------------------------------------------------------------------
void PioBus::send_word(bool dc, uint32_t value) {
    start_program(true, dc);
    pio_sm_put(pio0, sm_, 1u);    // una palabra de datos
    pio_sm_put(pio0, sm_, value); // el ancho del bus lo decide el programa
    wait_done(cmd_halt_pc_, 1);
    words_streamed_ += 1;
}

void PioBus::command(uint8_t c) { send_word(false, c); }

void PioBus::write_params(const uint8_t *data, uint32_t len) {
    if (data == nullptr) return;
    // El registro de comando del SSD1963 es de 8 bits, asi que cada parametro
    // es su propia palabra de bus: nunca hay bytes de mas con D/C = 0.
    for (uint32_t i = 0; i < len; ++i) {
        send_word(false, data[i]);
    }
}

// ---------------------------------------------------------------------------
// Rafaga por DMA
// ---------------------------------------------------------------------------
void PioBus::stream_words(const uint8_t *src, uint32_t words) {
    if (words == 0) return;

    start_program(false, true);
    pio_sm_put(pio0, sm_, words);

    // La DMA rellena la FIFO del PIO.  El origen avanza, el destino no: la
    // palabra se repite en la FIFO, no en la memoria.
    dma_channel_config cfg = dma_channel_get_default_config(dma_ch_);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(pio0, sm_, true));

    dma_channel_configure(dma_ch_, &cfg, &pio0->txf[sm_], src, words, true);
    dma_channel_wait_for_finish_blocking(dma_ch_);

    wait_done(data_halt_pc_, words);
    words_streamed_ += words;
}

// ---------------------------------------------------------------------------
// Pixels
// ---------------------------------------------------------------------------
void PioBus::write_pixels(const uint8_t *rgb888, uint32_t n) {
    if (rgb888 == nullptr || n == 0) return;

    const uint32_t in_blocks = (n / kPixelsPerDmaBlock) * kPixelsPerDmaBlock;

#if TFT_BUS_WIDTH == 8
    // RGB888 va directo a memoria: bus y formato coinciden, cero copias.
    for (uint32_t done = 0; done < in_blocks; done += kPixelsPerDmaBlock) {
        const uint8_t *p = rgb888 + done * kBytesPerPixel;
        stream_words(p, kPixelsPerDmaBlock * kBytesPerPixel / kBytesPerWord);
    }
#else
    // Bus de 16 bits: hay que empaquetar a RGB565, 2 pixeles por palabra.
    for (uint32_t done = 0; done < in_blocks; done += kPixelsPerDmaBlock) {
        for (uint32_t i = 0; i < kPixelsPerDmaBlock; i += 2) {
            const uint8_t *p = rgb888 + (done + i) * kBytesPerPixel;
            uint32_t lo = rgb565(Rgb{p[0], p[1], p[2]});
            uint32_t hi = rgb565(Rgb{p[3], p[4], p[5]});
            uint8_t *w = txbuf_ + i * 2u;
            w[0] = static_cast<uint8_t>(lo & 0xff);
            w[1] = static_cast<uint8_t>(lo >> 8);
            w[2] = static_cast<uint8_t>(hi & 0xff);
            w[3] = static_cast<uint8_t>(hi >> 8);
        }
        stream_words(txbuf_, kPixelsPerDmaBlock * kBytesPerPixel / kBytesPerWord);
    }
#endif

    // Lo que queda no llena un bloque entero: se agrupa en palabras de 32
    // bits mientras se pueda y los 3 pixels finales van uno a uno por el
    // programa corto, para no partir ningun pixel por la mitad.
    uint32_t i = in_blocks;
#if TFT_BUS_WIDTH == 8
    const uint32_t tail_words =
        ((n - i) / kPixelsPerWord) * kPixelsPerWord;  // multiplo de 4 pixeles
    if (tail_words > 0) stream_words(rgb888 + i * kBytesPerPixel, tail_words * 3 / 4);
    i += tail_words;
#endif
    for (; i < n; ++i) {
        const uint8_t *p = rgb888 + i * kBytesPerPixel;
        send_word(true, (TFT_BUS_WIDTH == 8)
                            ? p[0]
                            : (static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)));
    }
}

void PioBus::fill_pixels(Rgb c, uint32_t n) {
    if (n == 0) return;

    const uint32_t in_blocks = (n / kPixelsPerDmaBlock) * kPixelsPerDmaBlock;

    // Patron de color plano.  En 8 bits son 3 bytes por pixel, asi que el
    // patron son 12 bytes (3 palabras): repetir 4 bytes daria un degradado.
    const uint8_t pat[12] = {c.r, c.g, c.b, c.r, c.g, c.b,
                             c.r, c.g, c.b, c.r, c.g, c.b};
    const uint32_t words = kPixelsPerDmaBlock * kBytesPerPixel / kBytesPerWord;

    for (uint32_t done = 0; done < in_blocks; done += kPixelsPerDmaBlock) {
        for (uint32_t w = 0; w < words; ++w) {
            memcpy(fillbuf_ + w * kBytesPerWord, pat, kFillPatternBytes);
        }
        stream_words(fillbuf_, words);
    }

    // Cola igual que en write_pixels(): primero lo que llena palabras
    // enteras, y como mucho 3 pixels sueltos por el programa corto.
    uint32_t i = in_blocks;
#if TFT_BUS_WIDTH == 8
    const uint32_t tail_px = ((n - i) / kPixelsPerWord) * kPixelsPerWord;
    if (tail_px > 0) {
        const uint32_t tail_words = tail_px * 3 / 4;
        for (uint32_t w = 0; w < tail_words; ++w) {
            memcpy(fillbuf_ + w * kBytesPerWord, pat, kFillPatternBytes);
        }
        stream_words(fillbuf_, tail_words);
    }
    i += tail_px;
#endif
    for (; i < n; ++i) {
        if (TFT_BUS_WIDTH == 8) {
            send_word(true, c.r);
            send_word(true, c.g);
            send_word(true, c.b);
        } else {
            send_word(true, rgb565(c));
        }
    }
}

void PioBus::set_backlight_pwm(uint8_t duty) {
#if TFT_BL_PWM_ENABLE
    pwm_set_gpio_level(TFT_PIN_BL, duty);
#else
    (void)duty;
#endif
}

}  // namespace tft
