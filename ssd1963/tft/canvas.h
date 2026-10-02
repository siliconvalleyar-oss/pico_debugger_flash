// canvas.h - primitivas de dibujo RGB888 sobre el panel.
//
// No hay framebuffer: la Pico no tiene RAM para uno (480x272x3 = 391 KB, y
// la RP2040 tiene 264 KB de SRAM).  Cada primitiva abre su ventana y dibuja
// solo lo suyo, asi que el coste depende del area que toca y no de la
// resolucion.

#ifndef TFT_CANVAS_H
#define TFT_CANVAS_H

#include <stdint.h>

#include "panel.h"
#include "tft_fonts.h"
#include "tft_config.h"

namespace tft {

// Modos de draw_bitmap().
enum BitmapMode : uint8_t {
    kBitmapMono = 1,     // 1 bpp, un color de fondo y uno de tinta
    kBitmapMonoTrans = 2,// 1 bpp, los bits a 0 son transparentes
    kBitmapGray4 = 4,    // 4 bpp, paleta de 16 Rgb
    kBitmap332 = 8,      // 1 byte por pixel, RGB332
    kBitmapRgb565 = 16,  // 2 bytes por pixel
    kBitmapRgb888 = 24,  // 3 bytes por pixel
};

class Canvas {
public:
    explicit Canvas(Ssd1963 &panel) : panel_(panel) {}

    Ssd1963 &panel() { return panel_; }

    // --- rectangulos ------------------------------------------------------
    void fill_screen(Rgb c);
    void fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, Rgb c);
    void draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, Rgb c);
    void draw_pixel(int16_t x, int16_t y, Rgb c);

    // --- lineas -----------------------------------------------------------
    void draw_hline(int16_t x, int16_t y, int16_t len, Rgb c);
    void draw_vline(int16_t x, int16_t y, int16_t len, Rgb c);
    void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Rgb c);

    // --- circulos ---------------------------------------------------------
    void draw_circle(int16_t cx, int16_t cy, int16_t r, Rgb c);
    void fill_circle(int16_t cx, int16_t cy, int16_t r, Rgb c);

    // --- bitmaps ----------------------------------------------------------
    // data va fila a fila, sin relleno entre filas.  palette solo se usa en
    // kBitmapGray4 (16 colores Rgb en memoria).
    void draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                     const uint8_t *data, BitmapMode mode, Rgb fg, Rgb bg,
                     const Rgb *palette = nullptr);

    // --- texto ------------------------------------------------------------
    void set_font(const TftFont &font) { font_ = &font; }
    const TftFont &font() const { return *font_; }

    void set_text_pos(int16_t x, int16_t y) { tx_ = x; ty_ = y; }
    void set_text_colour(Rgb c) { fg_ = c; }
    void set_text_bg(Rgb c) { bg_ = c; }
    // 0 = transparente, TFT_DIM_BG (1) = fondo atenuado, TFT_KEEP_BG (2) = fondo opaco.
    void set_text_style(uint8_t style) { style_ = style; }
    // Multiplicador de escala: 1 = tamano natural de la fuente.
    void set_text_scale(uint8_t s) { scale_ = s < 1 ? 1 : s; }

    int16_t text_x() const { return tx_; }
    int16_t text_y() const { return ty_; }
    int16_t string_width(const char *s) const;
    int16_t text_height() const { return font_->vert * scale_; }

    void draw_char(int16_t x, int16_t y, char c);
    void print_string(const char *s);
    // printf con buffer propio (sin malloc).
    void print(const char *fmt, ...);

private:
    void draw_glyph(int16_t x, int16_t y, const Glyph &g, Rgb fg, Rgb bg,
                    uint8_t style);

    Ssd1963 &panel_;
    const TftFont *font_ = nullptr;
    int16_t tx_ = 0;
    int16_t ty_ = 0;
    Rgb fg_ = RGB_WHITE;
    Rgb bg_ = RGB_BLACK;
    uint8_t style_ = TFT_KEEP_BG;
    uint8_t scale_ = 1;

    char printbuf_[96];
};

}  // namespace tft

#endif  // TFT_CANVAS_H
