// panel.cpp - secuencia de arranque y control del SSD1963.
//
// Sobre la secuencia de arranque: la estructura y los valores derivados de la
// geometria (0xB0 con el ancho menos uno, 0xB7, 0xB1, la ventana de
// direcciones) son deterministas y estan calculados aqui.  Los bytes de reloj
// y de gamma son los valores habituales de la hoja de datos, y estan marcados
// como tales: no se han podido verificar en un panel real, asi que README.md
// lleva la tabla de puntos a comprobar en hardware.

#include "panel.h"

namespace tft {
namespace {

// --- registros del SSD1963 -------------------------------------------------
constexpr uint8_t REG_SLEEP_IN        = 0x01;
constexpr uint8_t REG_ADDRESS_INC     = 0x40;
constexpr uint8_t REG_COLUMN_ADDR     = 0x2A;
constexpr uint8_t REG_PAGE_ADDR       = 0x2B;
constexpr uint8_t REG_WRITE_RAM       = 0x2C;
constexpr uint8_t REG_SCROLL_AREA     = 0x33;
constexpr uint8_t REG_WRITE_RAM_START = 0x37;
constexpr uint8_t REG_MADCTL          = 0x36;
constexpr uint8_t REG_INVERSION       = 0x31;
constexpr uint8_t REG_LCD_MODE        = 0xB0;
constexpr uint8_t REG_V_PERIOD        = 0xB1;
constexpr uint8_t REG_V_BLANK         = 0xB2;
constexpr uint8_t REG_H_PERIOD        = 0xB4;
constexpr uint8_t REG_H_BLANK         = 0xB5;
constexpr uint8_t REG_V_BACK_PORCH    = 0xB6;
constexpr uint8_t REG_H_SYNC          = 0xB7;
constexpr uint8_t REG_H_BACK_PORCH    = 0xB8;
constexpr uint8_t REG_V_ADDRESS       = 0xBA;
constexpr uint8_t REG_RGB565          = 0xC1;
constexpr uint8_t REG_GAMMA           = 0xE0;
constexpr uint8_t REG_PLL_MUL         = 0xE2;
constexpr uint8_t REG_PLL_DIV         = 0xE3;
constexpr uint8_t REG_VCOM            = 0xE5;
constexpr uint8_t REG_PIN_FUNC        = 0xE6;
constexpr uint8_t REG_VDATA           = 0xEA;
constexpr uint8_t REG_TEST            = 0xF0;
constexpr uint8_t REG_CONTRAST        = 0xF7;
constexpr uint8_t REG_DISPLAY_ON      = 0x29;
constexpr uint8_t REG_DISPLAY_OFF     = 0x28;
constexpr uint8_t REG_BRIGHTNESS      = 0xBE;

// --- tiempos de barrido para 480x272 ---------------------------------------
// Reloj de pixel de 9 MHz: 496 x 288 = 142848 -> 63 Hz.
constexpr uint16_t kHTotal = TFT_PANEL_WIDTH + 16;
constexpr uint16_t kVTotal = TFT_PANEL_HEIGHT + 16;
constexpr uint16_t kHBlank = 16;
constexpr uint16_t kVBlank = 16;

inline void put_u16(uint8_t *p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v & 0xff);
}

// Curva de gamma habitual del SSD1963 (15 parametros).
constexpr uint8_t kGamma[15] = {0x07, 0x0A, 0x1F, 0x28, 0x32, 0x3B, 0x42, 0x48,
                                0x4D, 0x4E, 0x4F, 0x4F, 0x4F, 0x4F, 0x4F};

}  // namespace

Ssd1963::Ssd1963(TftBus &bus, uint16_t width, uint16_t height)
    : bus_(bus), width_(static_cast<int16_t>(width)),
      height_(static_cast<int16_t>(height)) {}

void Ssd1963::begin() {
    bus_.begin();
    bus_.reset();

    uint8_t p[16];

    // --- PLL ------------------------------------------------------------
    // 0xE2: multiplicador y divisor del PLL.  0x23 = VCO a 240 MHz.
    // 0xE3: divisor de la VCO.
    cmd(REG_PLL_MUL);
    p[0] = 0x23;
    p[1] = 0x02;
    p[2] = 0x54;
    send(p, 3);

    cmd(REG_PLL_DIV);
    p[0] = 0x81;
    send(p, 1);

    // --- modo de reposo --------------------------------------------------
    // Los 10 ms despues de 0xE0 0x03 no son opcionales: sin ellos el panel no
    // arranca (el PLL no da tiempo a asentarse).
    cmd(REG_TEST);
    p[0] = 0x01;
    send(p, 1);
    bus_.delay_ms(10);

    cmd(REG_TEST);
    p[0] = 0x03;
    send(p, 1);
    bus_.delay_ms(10);

    cmd(REG_SLEEP_IN);
    p[0] = 0x01;  // display on durante el reposo
    send(p, 1);

    // --- multiplexacion de pines ----------------------------------------
    cmd(REG_PIN_FUNC);
    p[0] = 0x01;
    p[1] = 0x99;
    p[2] = 0x9A;
    send(p, 3);

    // --- modo de panel ---------------------------------------------------
    // 0xB0 lleva el ancho MENOS UNO en los dos ultimos bytes: con el ancho
    // entero la mitad derecha de la imagen sale corrupta.
    cmd(REG_LCD_MODE);
    p[0] = static_cast<uint8_t>(0x20 | (TFT_BUS_WIDTH == 16 ? 0x01 : 0x00));
    put_u16(p + 1, static_cast<uint16_t>(width_ - 1));
    send(p, 3);

    // Interfaz 8080 de 16 o de 8 bits.
    cmd(REG_TEST);
    p[0] = static_cast<uint8_t>(TFT_BUS_WIDTH == 16 ? 0x01 : 0x00);
    send(p, 1);

    cmd(REG_DISPLAY_ON);
    bus_.delay_ms(10);

    // --- gamma -----------------------------------------------------------
    cmd(REG_GAMMA);
    p[0] = 0x00;
    send(p, 1);
    cmd(REG_GAMMA);
    for (int i = 0; i < 15; ++i) p[i] = kGamma[i];
    send(p, 15);
    cmd(REG_GAMMA);
    p[0] = 0x00;
    send(p, 1);

    // --- formato de pixel RGB565 -----------------------------------------
    cmd(REG_TEST);
    p[0] = 0x03;
    send(p, 1);

    cmd(REG_RGB565);
    p[0] = 0x03;
    p[1] = 0x1F;
    p[2] = 0x1F;
    send(p, 3);

    // --- tiempos de barrido ----------------------------------------------
    // 0xB4: periodo horizontal total y anchura de sincronismo.
    cmd(REG_H_PERIOD);
    put_u16(p, kHTotal);
    put_u16(p + 2, kHBlank);
    send(p, 4);

    cmd(REG_H_BLANK);
    put_u16(p, kHBlank);
    send(p, 2);

    cmd(REG_H_SYNC);
    p[0] = 0x00;
    put_u16(p + 1, kHBlank);
    send(p, 3);

    cmd(REG_H_BACK_PORCH);
    p[0] = static_cast<uint8_t>(kHBlank);
    send(p, 1);

    // 0xB1: periodo vertical total, en tercios de linea.
    cmd(REG_V_PERIOD);
    p[0] = 0x00;
    put_u16(p + 1, kVTotal);
    send(p, 3);

    cmd(REG_V_BLANK);
    put_u16(p, kVBlank);
    send(p, 2);

    cmd(REG_V_BACK_PORCH);
    put_u16(p, kVBlank);
    send(p, 2);

    cmd(REG_V_ADDRESS);
    p[0] = 0x00;
    p[1] = 0x00;
    send(p, 2);

    // --- VCOM y VDATA ----------------------------------------------------
    cmd(REG_VCOM);
    p[0] = 0x00;
    p[1] = 0x18;
    p[2] = 0x3F;
    send(p, 3);

    cmd(REG_VDATA);
    p[0] = 0x00;
    p[1] = 0x00;
    p[2] = 0x00;
    send(p, 3);

    cmd(REG_TEST);
    p[0] = 0x03;
    send(p, 1);

    // --- gamma por canal RGB (0xE0 0x0x) ---------------------------------
    for (int channel = 0; channel < 3; ++channel) {
        cmd(REG_GAMMA);
        p[0] = static_cast<uint8_t>(0x08 + channel);
        for (int i = 0; i < 15; ++i) p[i] = kGamma[i];
        send(p, 15);
    }
    cmd(REG_GAMMA);
    p[0] = 0x00;
    send(p, 1);

    // --- arranque final --------------------------------------------------
    cmd(REG_GAMMA);
    p[0] = 0x20;
    p[1] = 0x00;
    send(p, 2);

    cmd(REG_TEST);
    p[0] = 0x00;
    send(p, 1);

    cmd(REG_DISPLAY_OFF);
    bus_.delay_ms(10);
    cmd(REG_DISPLAY_ON);
    bus_.delay_ms(20);

    set_orientation(Orientation::kPortrait0);
    set_inc_mode(IncMode::kHorizontal);
    set_backlight(0xFF);
}

void Ssd1963::set_window(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (w <= 0 || h <= 0) return;

    // Recorte contra los limites del panel: escribir fuera no esta protegido.
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > width_) w = static_cast<int16_t>(width_ - x);
    if (y + h > height_) h = static_cast<int16_t>(height_ - y);
    if (w <= 0 || h <= 0) return;

    const uint16_t x1 = static_cast<uint16_t>(x);
    const uint16_t y1 = static_cast<uint16_t>(y);
    const uint16_t x2 = static_cast<uint16_t>(x + w - 1);
    const uint16_t y2 = static_cast<uint16_t>(y + h - 1);

    uint8_t p[4];
    cmd(REG_COLUMN_ADDR);
    put_u16(p, x1);
    put_u16(p + 2, x2);
    send(p, 4);

    cmd(REG_PAGE_ADDR);
    put_u16(p, y1);
    put_u16(p + 2, y2);
    send(p, 4);

    cmd(REG_WRITE_RAM);
}

void Ssd1963::set_window_full() { set_window(0, 0, width_, height_); }

void Ssd1963::set_inc_mode(IncMode mode) {
    uint8_t v = 0x00;
    switch (mode) {
        case IncMode::kHorizontal: v = 0x00; break;
        case IncMode::kVertical:   v = 0x01; break;
        case IncMode::kWrap:       v = 0x02; break;
    }
    cmd(REG_ADDRESS_INC);
    bus_.write_params(&v, 1);
}

void Ssd1963::write_pixels(const uint8_t *rgb888, uint32_t n) {
    if (rgb888 != nullptr && n > 0) bus_.write_pixels(rgb888, n);
}

void Ssd1963::fill(Rgb c, uint32_t n) {
    if (n > 0) bus_.fill_pixels(c, n);
}

void Ssd1963::fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, Rgb c) {
    if (w <= 0 || h <= 0) return;
    set_window(x, y, w, h);
    fill(c, static_cast<uint32_t>(w) * static_cast<uint32_t>(h));
}

void Ssd1963::draw_pixel(int16_t x, int16_t y, Rgb c) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    set_window(x, y, 1, 1);
    fill(c, 1);
}

void Ssd1963::set_backlight(uint8_t duty) {
    // 0xBE controla el PWM de backlight que genera el propio SSD1963 (solo si
    // el modulo lo saca de ahi).  El duty 0 apaga la luz.
    if (TFT_BL_REGISTER_ENABLE) {
        cmd(REG_BRIGHTNESS);
        bus_.write_params(&duty, 1);
    }
    // Y en paralelo el PWM del GPIO, para modulos con el backlight en un
    // transistor externo.
    if (TFT_BL_PWM_ENABLE) bus_.set_backlight_pwm(duty);
}

void Ssd1963::set_scroll_area(int16_t top, int16_t bottom, uint8_t vertical_step) {
    uint8_t p[6];
    put_u16(p, static_cast<uint16_t>(top));
    put_u16(p + 2, static_cast<uint16_t>(bottom));
    p[4] = vertical_step;
    p[5] = 0x00;
    cmd(REG_SCROLL_AREA);
    send(p, 6);
}

void Ssd1963::set_scroll_start(uint16_t y) {
    uint8_t p[4];
    put_u16(p, 0);   // columnas: toda la pantalla
    put_u16(p + 2, y);
    cmd(REG_WRITE_RAM_START);
    send(p, 4);
}

void Ssd1963::set_orientation(Orientation o) {
    orientation_ = o;

    uint8_t madctl = 0x00;
    switch (o) {
        case Orientation::kPortrait0:
            madctl = TFT_MADCTL_BGR | TFT_MADCTL_MX | TFT_MADCTL_MY;
            break;
        case Orientation::kPortrait90:
            madctl = TFT_MADCTL_BGR | TFT_MADCTL_MV;
            break;
        case Orientation::kPortrait180:
            madctl = TFT_MADCTL_BGR;
            break;
        case Orientation::kPortrait270:
            madctl = TFT_MADCTL_BGR | TFT_MADCTL_MV | TFT_MADCTL_MH;
            break;
    }
    cmd(REG_MADCTL);
    bus_.write_params(&madctl, 1);

    // En vertical la ventana de direcciones va en coordenas de panel.
    const bool swapped = (o == Orientation::kPortrait90 ||
                          o == Orientation::kPortrait270);
    if (swapped) {
        width_ = TFT_PANEL_HEIGHT;
        height_ = TFT_PANEL_WIDTH;
    } else {
        width_ = TFT_PANEL_WIDTH;
        height_ = TFT_PANEL_HEIGHT;
    }
}

void Ssd1963::set_invert(bool on) {
    uint8_t v = on ? 0x01 : 0x00;
    cmd(REG_INVERSION);
    bus_.write_params(&v, 1);
}

void Ssd1963::set_contrast(uint8_t c) {
    cmd(REG_CONTRAST);
    bus_.write_params(&c, 1);
}

}  // namespace tft
