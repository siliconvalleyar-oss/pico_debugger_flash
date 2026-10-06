#include "canvas.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace tft {

// ---------------------------------------------------------------------------
// Rectangulos
// ---------------------------------------------------------------------------
void Canvas::fill_screen(Rgb c) { panel_.fill_rect(0, 0, panel_.width(), panel_.height(), c); }

void Canvas::fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, Rgb c) {
    if (w <= 0 || h <= 0) return;
    panel_.fill_rect(x, y, w, h, c);
}

void Canvas::draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, Rgb c) {
    if (w <= 0 || h <= 0) return;
    draw_hline(x, y, w, c);
    draw_hline(x, y + h - 1, w, c);
    draw_vline(x, y, h, c);
    draw_vline(x + w - 1, y, h, c);
}

void Canvas::draw_pixel(int16_t x, int16_t y, Rgb c) { panel_.draw_pixel(x, y, c); }

// ---------------------------------------------------------------------------
// Lineas
// ---------------------------------------------------------------------------
void Canvas::draw_hline(int16_t x, int16_t y, int16_t len, Rgb c) {
    if (len <= 0) return;
    if (y < 0 || y >= panel_.height()) return;
    if (x < 0) {
        len += x;
        x = 0;
    }
    if (x + len > panel_.width()) len = static_cast<int16_t>(panel_.width() - x);
    if (len <= 0) return;
    panel_.fill_rect(x, y, len, 1, c);
}

void Canvas::draw_vline(int16_t x, int16_t y, int16_t len, Rgb c) {
    if (len <= 0) return;
    if (x < 0 || x >= panel_.width()) return;
    if (y < 0) {
        len += y;
        y = 0;
    }
    if (y + len > panel_.height()) len = static_cast<int16_t>(panel_.height() - y);
    if (len <= 0) return;
    panel_.fill_rect(x, y, 1, len, c);
}

void Canvas::draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Rgb c) {
    // Bresenham, pero agrupando los tramos horizontales: cada uno es un solo
    // fill_rect en vez de un pixel a pixel.
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    int run_x = x0;
    int run_len = 0;
    for (;;) {
        // pixel actual, agrupado en tramos horizontales
        if (run_len == 0) {
            run_x = x0;
            run_len = 1;
        } else if (x0 == run_x + run_len) {
            ++run_len;
        } else {
            draw_hline(run_x, y0, run_len, c);
            run_x = x0;
            run_len = 1;
        }
        if (x0 == x1 && y0 == y1) break;

        const int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
            if (run_len > 0) {
                draw_hline(run_x, y0 - sy, run_len, c);
                run_len = 0;
            }
        }
    }
    if (run_len > 0) draw_hline(run_x, y0, run_len, c);
}

// ---------------------------------------------------------------------------
// Circulos
// ---------------------------------------------------------------------------
void Canvas::draw_circle(int16_t cx, int16_t cy, int16_t r, Rgb c) {
    if (r <= 0) {
        draw_pixel(cx, cy, c);
        return;
    }
    int x = r;
    int y = 0;
    int err = 1 - r;
    while (x >= y) {
        draw_hline(cx - x, cy + y, 2 * x + 1, c);
        draw_hline(cx - x, cy - y, 2 * x + 1, c);
        draw_hline(cx - y, cy + x, 2 * y + 1, c);
        draw_hline(cx - y, cy - x, 2 * y + 1, c);
        ++y;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            --x;
            err += 2 * (y - x) + 1;
        }
    }
}

void Canvas::fill_circle(int16_t cx, int16_t cy, int16_t r, Rgb c) {
    if (r <= 0) {
        draw_pixel(cx, cy, c);
        return;
    }
    for (int dy = -r; dy <= r; ++dy) {
        const int dx = static_cast<int>(r * r - dy * dy);
        int w = 0;
        while ((w + 1) * (w + 1) <= dx) ++w;
        if (w == 0) {
            draw_pixel(cx, cy + dy, c);
        } else {
            draw_hline(cx - w, cy + dy, 2 * w + 1, c);
        }
    }
}

// ---------------------------------------------------------------------------
// Bitmaps
// ---------------------------------------------------------------------------
void Canvas::draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                         const uint8_t *data, BitmapMode mode, Rgb fg, Rgb bg,
                         const Rgb *palette) {
    if (data == nullptr || w <= 0 || h <= 0) return;
    if (x >= panel_.width() || y >= panel_.height()) return;

    const uint32_t bytes_per_row = (static_cast<uint32_t>(w) * mode + 7u) / 8u;

    for (int16_t row = 0; row < h; ++row) {
        const int16_t py = y + row;
        if (py < 0 || py >= panel_.height()) continue;
        const uint8_t *src = data + static_cast<uint32_t>(row) * bytes_per_row;

        if (mode == kBitmapRgb888) {
            // El formato coincide con el del bus: una ventana y la fila entera
            // por DMA, sin tocar un solo pixel.
            panel_.set_window(x, py, w, 1);
            panel_.write_pixels(src, static_cast<uint32_t>(w));
            continue;
        }

        // Cada modo se recorre por franjas horizontales del mismo color, que
        // es como mas rapido llega el panel: una ventana por franja.
        int32_t start = 0;      // primer pixel de la franja actual
        bool have = false;
        Rgb current{};

        auto flush = [&](int32_t upto) {
            if (have && upto > start) {
                fill_rect(x + static_cast<int16_t>(start), py,
                          static_cast<int16_t>(upto - start), 1, current);
            }
            have = false;
        };

        for (int16_t col = 0; col < w; ++col) {
            Rgb c{};
            bool opaque = true;
            switch (mode) {
                case kBitmapMono:
                    c = ((src[col >> 3] >> (7 - (col & 7))) & 1) ? fg : bg;
                    break;
                case kBitmapMonoTrans:
                    c = fg;
                    opaque = ((src[col >> 3] >> (7 - (col & 7))) & 1) != 0;
                    break;
                case kBitmapGray4: {
                    const uint8_t v =
                        (src[col >> 1] >> ((col & 1) ? 0 : 4)) & 0x0f;
                    c = (palette != nullptr) ? palette[v] : fg;
                    break;
                }
                case kBitmap332: {
                    const uint8_t v = src[col];
                    c = Rgb{static_cast<uint8_t>((v & 0xe0)),
                            static_cast<uint8_t>((v & 0x1c) << 3),
                            static_cast<uint8_t>((v & 0x03) << 5)};
                    break;
                }
                case kBitmapRgb565: {
                    const uint16_t v =
                        static_cast<uint16_t>(src[col * 2] | (src[col * 2 + 1] << 8));
                    c = Rgb{static_cast<uint8_t>((v >> 11) << 3),
                            static_cast<uint8_t>((v >> 5) << 2),
                            static_cast<uint8_t>((v & 0x1f) << 3)};
                    break;
                }
                case kBitmapRgb888:
                default:
                    c = Rgb{src[col * 3], src[col * 3 + 1], src[col * 3 + 2]};
                    break;
            }

            if (!opaque) {
                if (have) {
                    flush(col);
                }
                continue;
            }
            if (have && !(c.r == current.r && c.g == current.g && c.b == current.b)) {
                flush(col);
            }
            if (!have) {
                start = col;
                current = c;
                have = true;
            }
        }
        flush(w);
    }
}

// ---------------------------------------------------------------------------
// Texto
// ---------------------------------------------------------------------------
int16_t Canvas::string_width(const char *s) const {
    if (s == nullptr) return 0;
    return static_cast<int16_t>(strlen(s) * font_->horiz * scale_);
}

void Canvas::draw_glyph(int16_t x, int16_t y, const Glyph &g, Rgb fg, Rgb bg,
                        uint8_t style) {
    const int cell_w = font_->horiz * scale_;
    const int cell_h = font_->vert * scale_;

    if (style == TFT_KEEP_BG) {
        fill_rect(x, y, cell_w, cell_h, bg);
    } else if (style == TFT_DIM_BG) {
        // Fondo atenuado: la celda entera con el color de tinta a media
        // intensidad, para que el texto se lea sobre un fondo cualquiera.
        fill_rect(x, y, cell_w, cell_h,
                  Rgb{static_cast<uint8_t>(fg.r >> 1), static_cast<uint8_t>(fg.g >> 1),
                      static_cast<uint8_t>(fg.b >> 1)});
    }

    const int stride = (g.cols + 7) / 8;
    for (int row = 0; row < g.rows; ++row) {
        int run = -1;
        for (int col = 0; col <= g.cols; ++col) {
            const bool on =
                (col < g.cols) && ((g.bits[row * stride + (col >> 3)] >>
                                    (7 - (col & 7))) & 1);
            if (on) {
                if (run < 0) run = col;
                continue;
            }
            if (run >= 0) {
                fill_rect(x + run * scale_, y + row * scale_,
                          (col - run) * scale_, scale_, fg);
                run = -1;
            }
        }
    }
}

void Canvas::draw_char(int16_t x, int16_t y, char c) {
    if (font_ == nullptr) return;
    const Glyph g = glyph_of(*font_, c);
    draw_glyph(x, y, g, fg_, bg_, style_);
}

void Canvas::print_string(const char *s) {
    if (s == nullptr || font_ == nullptr) return;
    const int16_t step = static_cast<int16_t>(font_->horiz * scale_);
    while (*s != '\0') {
        if (*s == '\n') {
            tx_ = 0;
            ty_ = static_cast<int16_t>(ty_ + text_height() + scale_);
            ++s;
            continue;
        }
        draw_char(tx_, ty_, *s++);
        tx_ = static_cast<int16_t>(tx_ + step);
    }
}

void Canvas::print(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(printbuf_, sizeof(printbuf_), fmt, ap);
    va_end(ap);
    print_string(printbuf_);
}

}  // namespace tft
