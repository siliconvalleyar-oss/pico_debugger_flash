#include "tft_fonts.h"

#include "font_glcd_8x8.h"
#include "font_glcd_8x16.h"

namespace tft {

Glyph glyph_of(const TftFont &font, char c) {
    int rel = static_cast<unsigned char>(c) - font.first_char;
    if (rel < 0 || rel >= static_cast<int>(font.nchars)) rel = 0;

    const uint16_t begin = font.index[rel];
    const uint16_t end = font.index[rel + 1];

    Glyph g;
    g.rows = font.vert;
    g.cols = font.vert > 0 ? static_cast<int>(end - begin) * 8 / font.vert : 0;
    if (g.cols > font.horiz) g.cols = font.horiz;
    g.bits = font.data + begin;
    g.advance = font.horiz;  // monoespaciada
    return g;
}

const TftFont &font_8x16() { return Font8x16; }
const TftFont &font_8x8() { return Font8x8; }

}  // namespace tft
