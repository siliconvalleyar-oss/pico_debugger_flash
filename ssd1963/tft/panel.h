// panel.h - control del controlador SSD1963.
//
// Esta capa no sabe nada del hardware: habla solo con TftBus.  Es C++ puro,
// asi que el mismo panel sirve para el backend PIO de la Pico y para el
// backend GPIO de la Raspberry Pi 4.

#ifndef TFT_PANEL_H
#define TFT_PANEL_H

#include <stdint.h>

#include "bus.h"
#include "tft_config.h"

namespace tft {

// Modo de incremento automatico de la direccion de memoria.
enum class IncMode : uint8_t {
    kHorizontal,  // de pixel en pixel
    kVertical,
    kWrap,        // al llegar al final de la ventana, vuelve al principio
};

class Ssd1963 {
public:
    Ssd1963(TftBus &bus, uint16_t width = TFT_PANEL_WIDTH,
            uint16_t height = TFT_PANEL_HEIGHT);

    // Reset fisico + secuencia de arranque.  Hay que llamarla una vez, antes
    // de dibujar nada.  Es la unica funcion con esperas largas.
    void begin();

    int16_t width() const { return width_; }
    int16_t height() const { return height_; }

    // --- ventana de memoria ---------------------------------------------
    // Selecciona el rectangulo destino de las siguientes escrituras.  Las
    // coordenadas van en pixeles y se recortan al panel.
    void set_window(int16_t x, int16_t y, int16_t w, int16_t h);
    void set_window_full();
    void set_inc_mode(IncMode mode);

    // --- escritura de pixeles -------------------------------------------
    // n pixeles RGB888 (3 bytes: R G B) hacia la ventana activa.
    void write_pixels(const uint8_t *rgb888, uint32_t n);
    void fill(Rgb c, uint32_t n);
    void fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, Rgb c);
    void draw_pixel(int16_t x, int16_t y, Rgb c);

    // --- brillo ----------------------------------------------------------
    // Brillo del propio SSD1963 (registro 0xBE) y/o PWM del GPIO de
    // backlight.  duty 0..255.
    void set_backlight(uint8_t duty);

    // --- scroll ----------------------------------------------------------
    // 0x33: area de scroll (lineas superior e inferior, paso vertical).
    void set_scroll_area(int16_t top, int16_t bottom, uint8_t vertical_step);
    // 0x37: direccion de inicio dentro de la ventana de scroll.
    void set_scroll_start(uint16_t y);

    // --- orientacion y ajustes -------------------------------------------
    // Cambia MADCTL y el tamano logico de la pantalla segun la rotacion.
    void set_orientation(Orientation o);
    Orientation orientation() const { return orientation_; }

    void set_invert(bool on);
    void set_contrast(uint8_t c);  // 0xF7

    TftBus &bus() { return bus_; }

private:
    void send(const uint8_t *params, uint32_t n) { bus_.write_params(params, n); }
    void cmd(uint8_t c) { bus_.command(c); }

    TftBus &bus_;
    int16_t width_;
    int16_t height_;
    Orientation orientation_ = Orientation::kPortrait0;
};

}  // namespace tft

#endif  // TFT_PANEL_H
