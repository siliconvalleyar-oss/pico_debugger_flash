// tft_fonts.h - glifos empaquetados de fuente monoespaciada.
//
// Formato (el que produce tools/gen_fonts.py):
//   index[rel] .. index[rel+1]  -> bytes del glifo dentro de data[]
//   rows = vert
//   cols = (index[rel + 1] - index[rel]) * 8 / rows
// Los bits de cada fila van en MSB primero, con el caracter alineado a la
// izquierda.

#ifndef TFT_FONTS_H
#define TFT_FONTS_H

#include <stdint.h>

namespace tft {

struct TftFont {
    const uint8_t *data;
    const uint16_t *index;
    uint8_t vert;    // filas por glifo
    uint8_t horiz;   // columnas nominales
    uint16_t nchars;
    uint16_t first_char;
};

// Datos de un glifo.  Un caracter fuera de rango se dibuja como el primero.
struct Glyph {
    const uint8_t *bits;
    int rows;
    int cols;
    int advance;  // columnas que ocupa el glifo mas su separacion
};

Glyph glyph_of(const TftFont &font, char c);

// Fuentes incluidas en el driver.
const TftFont &font_8x16();
const TftFont &font_8x8();

}  // namespace tft

#endif  // TFT_FONTS_H
