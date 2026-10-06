// touch.cpp - ADS7843 por SPI.
//
// El ADS7843 no tiene entrada de Z, asi que la presion se deduce del valor
// leido: sin dedo las dos medidas caen en los extremos del rango de 12 bits.
// Con la calibracion puesta, la presion se deduce ademas de que el punto
// medido caiga dentro de la pantalla.

#include "touch.h"

#include <algorithm>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

namespace tft {
namespace {

// Comandos de una conversion simple (single ended), 12 bits, con power down:
//   bit7 S=1, bits6..4 canal, bit3 MODE=1, bit2 PD=1, bits1..0 DS=00
constexpr uint8_t CMD_X = 0xBC;  // AIN1 = X
constexpr uint8_t CMD_Y = 0x9C;  // AIN3 = Y

constexpr int kSamples = 7;         // impares, para que haya mediana
constexpr int kPressLow = 350;      // por debajo, no hay dedo
constexpr int kPressHigh = 3800;    // por encima, tampoco

inline uint16_t median(uint16_t *v, int n) {
    std::sort(v, v + n);
    return v[n / 2];
}

}  // namespace

TouchAds7843::TouchAds7843(TouchCalibration *cal) : cal_(cal) {}

void TouchAds7843::begin() {
#if TFT_TOUCH_ENABLE
    spi_ = spi1;

    gpio_init(TFT_TOUCH_PIN_CS);
    gpio_set_dir(TFT_TOUCH_PIN_CS, GPIO_OUT);
    gpio_put(TFT_TOUCH_PIN_CS, 1);

    // El ADS7843 trabaja con CPOL=0, CPHA=1 (modo 1 del控制器 SPI) y MSB
    // primero.  Si las lecturas salen a 0 o a 4095, es el modo del reloj:
    // cambiar TFT_TOUCH_SPI_MODE y probar con 0, 2 o 3.
    spi_init(spi_, TFT_TOUCH_SPI_BAUD);
    // Modo 1 = CPOL 0, CPHA 1.  Los bits del numero de modo son los del
    // protocolo SPI: bit1 = polaridad, bit0 = fase.
    spi_set_format(spi_, 8, (TFT_TOUCH_SPI_MODE & 2) ? SPI_CPOL_1 : SPI_CPOL_0,
                   (TFT_TOUCH_SPI_MODE & 1) ? SPI_CPHA_1 : SPI_CPHA_0, SPI_MSB_FIRST);
    spi_set_slave(spi_, TFT_TOUCH_PIN_CS);
#endif
}

uint16_t TouchAds7843::read_adc(uint8_t command) {
#if TFT_TOUCH_ENABLE
    // El primer byte de vuelta corresponde a la conversion anterior, asi que
    // se manda un byte de relleno y se leen los dos siguientes.
    uint8_t tx[2] = {command, 0x00};
    uint8_t rx[2] = {0, 0};
    gpio_put(TFT_TOUCH_PIN_CS, 0);
    spi_write_read_blocking(spi_, tx, rx, 2);
    gpio_put(TFT_TOUCH_PIN_CS, 1);
    return static_cast<uint16_t>((rx[0] << 4) | (rx[1] >> 4));
#else
    (void)command;
    return 0;
#endif
}

bool TouchAds7843::read_xy(uint16_t &x, uint16_t &y) {
    uint16_t xs[kSamples];
    uint16_t ys[kSamples];
    for (int i = 0; i < kSamples; ++i) {
        // El ADS7843 es de 12 bits: se recorta a ese rango.
        xs[i] = read_adc(CMD_X) & 0x0fff;
        ys[i] = read_adc(CMD_Y) & 0x0fff;
    }
    x = median(xs, kSamples);
    y = median(ys, kSamples);
    return true;
}

bool TouchAds7843::is_pressed() {
    uint16_t x = 0, y = 0;
    read_xy(x, y);
    const bool down = !(x < kPressLow || x > kPressHigh ||
                        y < kPressLow || y > kPressHigh);
    if (down) {
        last_ = Point{static_cast<int16_t>(x), static_cast<int16_t>(y)};
    }
    return down;
}

Point TouchAds7843::raw_position() { return last_; }

Point TouchAds7843::position() {
    if (!affine_ready_) return last_;
    const int32_t rx = last_.x;
    const int32_t ry = last_.y;
    Point p;
    p.x = static_cast<int16_t>((ax_ * rx + bx_ * ry + cx_) >> 16);
    p.y = static_cast<int16_t>((dx_ * rx + dy_ * ry + cy_) >> 16);
    return p;
}

bool TouchAds7843::build_calibration() {
    if (cal_ == nullptr) {
        affine_ready_ = false;
        return false;
    }

    // Se usan los tres primeros puntos (las dos esquinas superiores y la
    // inferior izquierda) para resolver la transformada afin.  El cuarto
    // (inferior derecha) queda como comprobacion.
    const Point *s = cal_->screen;
    const Point *r = cal_->raw;

    const int32_t dx1 = r[1].x - r[0].x, dy1 = r[1].y - r[0].y;
    const int32_t dx2 = r[2].x - r[0].x, dy2 = r[2].y - r[0].y;
    const int32_t det = dx1 * dy2 - dx2 * dy1;
    if (det == 0) {
        affine_ready_ = false;
        return false;
    }

    // Transformada afin por los tres puntos:
    //   A*rx + B*ry + C = sx        D*rx + E*ry + F = sy
    // Restando el primer punto a los otros dos queda un sistema de 2x2 que se
    // resuelve en Q16 con la determinante como divisor comun.
    const int32_t sx1 = s[1].x - s[0].x, sx2 = s[2].x - s[0].x;
    const int32_t sy1 = s[1].y - s[0].y, sy2 = s[2].y - s[0].y;

    ax_ = static_cast<int32_t>(((static_cast<int64_t>(sx1) * dy2 -
                                 static_cast<int64_t>(sx2) * dy1) << 16) / det);
    bx_ = static_cast<int32_t>(((static_cast<int64_t>(dx1) * sx2 -
                                 static_cast<int64_t>(dx2) * sx1) << 16) / det);
    dx_ = static_cast<int32_t>(((static_cast<int64_t>(sy1) * dy2 -
                                 static_cast<int64_t>(sy2) * dy1) << 16) / det);
    dy_ = static_cast<int32_t>(((static_cast<int64_t>(dx1) * sy2 -
                                 static_cast<int64_t>(dx2) * sy1) << 16) / det);
    cx_ = -((ax_ * r[0].x + bx_ * r[0].y) >> 16);
    cy_ = -((dx_ * r[0].x + dy_ * r[0].y) >> 16);

    affine_ready_ = true;
    return true;
}

int16_t TouchAds7843::calibration_error() const {
    if (!affine_ready_ || cal_ == nullptr) return -1;
    const Point r = cal_->raw[3];
    const int32_t x = (ax_ * r.x + bx_ * r.y + cx_) >> 16;
    const int32_t y = (dx_ * r.x + dy_ * r.y + cy_) >> 16;
    int32_t ex = x - cal_->screen[3].x;
    int32_t ey = y - cal_->screen[3].y;
    if (ex < 0) ex = -ex;
    if (ey < 0) ey = -ey;
    return static_cast<int16_t>((ex > ey) ? ex : ey);
}

}  // namespace tft
