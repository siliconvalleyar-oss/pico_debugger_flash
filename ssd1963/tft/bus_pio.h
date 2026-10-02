// bus_pio.h - backend del bus SSD1963 sobre PIO + DMA (RP2040 y RP2350).

#ifndef TFT_BUS_PIO_H
#define TFT_BUS_PIO_H

#include "bus.h"

namespace tft {

class PioBus final : public TftBus {
public:
    PioBus();
    ~PioBus() override;

    void begin() override;
    void delay_ms(uint32_t ms) override;
    void reset() override;
    void command(uint8_t c) override;
    void write_params(const uint8_t *data, uint32_t len) override;
    void write_pixels(const uint8_t *rgb888, uint32_t n) override;
    void fill_pixels(Rgb c, uint32_t n) override;
    void set_backlight_pwm(uint8_t duty) override;

    uint32_t words_streamed() const { return words_streamed_; }

private:
    // Una palabra de bus con D/C a elegir: 1 byte en modo 8 bits, 2 bytes
    // (un pixel RGB565) en modo 16 bits.  Es el camino corto, sin DMA.
    void send_word(bool dc, uint32_t value);

    // words palabras de 32 bits por DMA, D/C = 1.
    void stream_words(const uint8_t *src, uint32_t words);

    void start_program(bool cmd, bool dc);
    void wait_done(uint8_t halt_pc, uint32_t words);

    uint8_t sm_ = 0;
    uint8_t dma_ch_ = 0;
    uint8_t cmd_offset_ = 0;
    uint8_t data_offset_ = 0;
    uint8_t cmd_halt_pc_ = 0;
    uint8_t data_halt_pc_ = 0;
    bool initialised_ = false;

    // Buffer de conversion RGB888 -> RGB565 (solo se usa con bus de 16 bits).
    // En bus de 8 bits write_pixels() va directo a memoria del llamante.
    alignas(4) uint8_t txbuf_[TFT_TX_BUF_BYTES];

    // Patron de color plano para fill_pixels(), alineado a palabras de 32.
    alignas(4) uint8_t fillbuf_[12];

    uint32_t words_streamed_ = 0;
};

}  // namespace tft

#endif  // TFT_BUS_PIO_H
