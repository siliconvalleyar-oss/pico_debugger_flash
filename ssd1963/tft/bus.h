// bus.h - interfaz abstracta del bus del panel.
//
// Esta capa no sabe nada del hardware: ni PIO, ni DMA, ni GPIO.  Es lo que
// permite que la clase Ssd1963 (panel.cpp) sea C++ puro y portable.

#ifndef TFT_BUS_H
#define TFT_BUS_H

#include <stdint.h>

#include "tft_config.h"

namespace tft {

class TftBus {
public:
    virtual ~TftBus() = default;

    // Prepara el hardware del bus.  Idempotente.
    virtual void begin() = 0;

    // Espera en milisegundos.  La necesita la secuencia de arranque.
    virtual void delay_ms(uint32_t ms) = 0;

    // Reset fisico del panel por su pin /RST, con las esperas que pide la
    // hoja de datos.
    virtual void reset() = 0;

    // Byte de comando (D/C = 0).  Un byte de bus, el panel lo captura en el
    // flanco de subida de /WR.
    virtual void command(uint8_t c) = 0;

    // rampa de parametros: uno o varios bytes de bus con D/C = 0.
    virtual void write_params(const uint8_t *data, uint32_t len) = 0;

    // n pixels RGB888 (3 bytes por pixel, R G B) enviados a la ventana activa.
    virtual void write_pixels(const uint8_t *rgb888, uint32_t n) = 0;

    // n pixels del mismo color.  Internamente no copia nada por pixel.
    virtual void fill_pixels(Rgb c, uint32_t n) = 0;

    // Brillo por PWM del GPIO de backlight (0..255).  No hace nada si el
    // control de brillo esta en TFT_BL_REGISTER_ENABLE.
    virtual void set_backlight_pwm(uint8_t duty) = 0;
};

}  // namespace tft

#endif  // TFT_BUS_H
