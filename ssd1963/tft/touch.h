// touch.h - controlador tactil ADS7843 (resistivo) por SPI con calibracion.

#ifndef TFT_TOUCH_H
#define TFT_TOUCH_H

#include <stdint.h>

#include "hardware/spi.h"
#include "tft_config.h"

namespace tft {

struct Point {
    int16_t x = 0;
    int16_t y = 0;
};

// Los cuatro puntos de calibracion, en coordenadas de pantalla.
struct TouchCalibration {
    Point screen[4];
    Point raw[4];
};

// ADS7843: un solo canal, 12 bits.  X y Y se miden con distinta referencia
// (VREF y AIN3/AIN4), por eso hay dos Median() distintos.
class TouchAds7843 {
public:
    explicit TouchAds7843(TouchCalibration *cal = nullptr);

    void begin();

    // true si hay dedo pulsado.
    bool is_pressed();

    // Ultima posicion en coordenadas de pantalla.  Solo valida si el anterior
    // is_pressed() devolvio true.
    Point position();

    // calibrated = false devuelve el valor crudo de 12 bits.
    Point raw_position();

    // Calcula los coeficientes a partir de los puntos guardados.  Devuelve
    // false si los puntos no sirven (valores repetidos o determinant 0).
    bool build_calibration();

    // Error de la calibracion en pixeles: distancia entre el cuarto punto
    // medido y el esperado.  Sirve para saber si hay que recalibrar.
    int16_t calibration_error() const;

    void set_calibration(const TouchCalibration *cal) { cal_ = cal; }
    bool calibrated() const { return cal_ != nullptr; }

private:
    uint16_t read_adc(uint8_t command);
    bool read_xy(uint16_t &x, uint16_t &y);

    // Coeficientes de la transformada afin en Q16:
    //   x = (ax * rx + bx * ry + cx) >> 16
    //   y = (dx * rx + dy * ry + cy) >> 16
    int32_t ax_ = 65536, bx_ = 0, cx_ = 0;
    int32_t dx_ = 0, dy_ = 65536, cy_ = 0;
    bool affine_ready_ = false;

    spi_inst_t *spi_ = nullptr;
    const TouchCalibration *cal_ = nullptr;
    Point last_{};
};

}  // namespace tft

#endif  // TFT_TOUCH_H
