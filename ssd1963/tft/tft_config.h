// tft_config.h - configuracion unica del driver SSD1963
//
// Todo se cambia aqui. El resto del driver no tiene numeros magicos.
//
// Placas soportadas: pico, pico_w, pico2, pico2_w  (RP2040 y RP2350).
// El codigo es identico para las cuatro: unica diferencia es el reloj de PIO,
// que se calcula a partir de clk_sys.
//
// ---------------------------------------------------------------------------
// BUS DE DATOS
// ---------------------------------------------------------------------------
// TFT_BUS_WIDTH = 8  -> 8 lineas de datos (GP0..GP7).   Valor por defecto.
// TFT_BUS_WIDTH = 16 -> 16 lineas de datos (GP0..GP15), pixeles RGB565.
//
// Para pasar de 8 a 16 bits basta con cambiar el valor de abajo. El mapa de
// pines ya esta preparado: los 8 bits altos se activan solos. Si el panel
// tiene el touch conectado, ver la nota de TFT_TOUCH_* mas abajo.

#ifndef TFT_CONFIG_H
#define TFT_CONFIG_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// 1. Ancho del bus paralelo hacia el SSD1963
// ---------------------------------------------------------------------------
#ifndef TFT_BUS_WIDTH
#define TFT_BUS_WIDTH 8
#endif

#if (TFT_BUS_WIDTH != 8) && (TFT_BUS_WIDTH != 16)
#error "TFT_BUS_WIDTH debe ser 8 o 16"
#endif

// ---------------------------------------------------------------------------
// 2. Panel (LB04301 y otros 480x272 con SSD1963)
// ---------------------------------------------------------------------------
#define TFT_PANEL_WIDTH  480
#define TFT_PANEL_HEIGHT 272

// Reloj de pixel del SSD1963. 9 MHz es el valor habitual para 480x272.
#define TFT_PIXEL_CLOCK_HZ 9000000

// ---------------------------------------------------------------------------
// 3. Mapa de pines de la pantalla
// ---------------------------------------------------------------------------
// Los pines de datos son contiguos (GP0..GP7 o GP0..GP15) porque el PIO
// necesita un grupo de pines contiguo para el puerto OUT.
#define TFT_PIN_D0        0
// D1..D7 y D8..D15 son implicitos y contiguos (TFT_PIN_D0 + n).

// D/C lo maneja el software como GPIO normal, asi el mismo programa PIO
// sirve para comandos y para datos.  El side-set del PIO se queda solo con
// los dos strobes, que son lo unico que el PIO tiene que generar: /WR, /RD.
#define TFT_PIN_DC        16
#define TFT_PIN_WR        17
#define TFT_PIN_RD        18
#define TFT_SIDE_COUNT    2

// /CS lo maneja el software como GPIO normal (no va en el side-set) para
// poder mantenerlo bajo durante toda una ráfaga de DMA y para que el PIO no
// pueda meter datos en el panel mientras la state machine esta apagada.
#define TFT_PIN_CS        19
#define TFT_PIN_RST       20

// Backlight por GPIO (PWM). El brillo por registro 0xBE del SSD1963 tambien
// esta implementado: ambos controles son independientes, ver tft_config.
#define TFT_PIN_BL        21

// ---------------------------------------------------------------------------
// 4. Touch ADS7843 (SPI1, pines libres)
// ---------------------------------------------------------------------------
#ifndef TFT_TOUCH_ENABLE
#define TFT_TOUCH_ENABLE 1
#endif

// Con bus de 8 bits (GP0..GP7) los pines GP12..GP15 estan libres.  Con bus de
// 16 bits (GP0..GP15) hay que moverlos a GP22..GP25, que es lo que hay por
// defecto en ese caso.  AVISO: en la Pico W y la Pico 2 W, GP23..GP25 los usa
// la radio (SDIO del CYW43 / RM2W).  Este driver no arranca el stack
// inalambrico, asi que no molestan, pero si necesitas WiFi tendras que
// mover el display o el touch.  Ver README.md.
#if TFT_BUS_WIDTH == 16
#define TFT_TOUCH_PIN_SCK   22
#define TFT_TOUCH_PIN_MISO  23
#define TFT_TOUCH_PIN_MOSI  24
#define TFT_TOUCH_PIN_CS    25
#else
#define TFT_TOUCH_PIN_SCK   12
#define TFT_TOUCH_PIN_MISO  13
#define TFT_TOUCH_PIN_MOSI  14
#define TFT_TOUCH_PIN_CS    15
#endif

#define TFT_TOUCH_SPI_BAUD  1000000
// Modo del reloj SPI: el ADS7843 manda el dato en el flanco de bajada, asi que
// lo normal es el modo 1.  Si las lecturas salen a 0 o a 4095, probar 0, 2, 3.
#define TFT_TOUCH_SPI_MODE  1

#if TFT_TOUCH_ENABLE
// El touch no puede caer ni en el grupo de datos ni en el bloque de control
// (D/C, /WR, /RD, /CS, /RST, BL), o dos periféricos pelearían por el mismo GPIO.
#if (TFT_TOUCH_PIN_SCK >= TFT_PIN_D0) && (TFT_TOUCH_PIN_SCK < TFT_PIN_D0 + TFT_BUS_WIDTH)
#error "TFT_TOUCH_PIN_SCK cae en el bus de datos"
#endif
#if (TFT_TOUCH_PIN_MISO >= TFT_PIN_D0) && (TFT_TOUCH_PIN_MISO < TFT_PIN_D0 + TFT_BUS_WIDTH)
#error "TFT_TOUCH_PIN_MISO cae en el bus de datos"
#endif
#if (TFT_TOUCH_PIN_MOSI >= TFT_PIN_D0) && (TFT_TOUCH_PIN_MOSI < TFT_PIN_D0 + TFT_BUS_WIDTH)
#error "TFT_TOUCH_PIN_MOSI cae en el bus de datos"
#endif
#if (TFT_TOUCH_PIN_CS >= TFT_PIN_D0) && (TFT_TOUCH_PIN_CS < TFT_PIN_D0 + TFT_BUS_WIDTH)
#error "TFT_TOUCH_PIN_CS cae en el bus de datos"
#endif
#if (TFT_TOUCH_PIN_SCK >= TFT_PIN_DC) && (TFT_TOUCH_PIN_SCK <= TFT_PIN_BL)
#error "TFT_TOUCH_PIN_SCK cae en el bloque de control de la pantalla"
#endif
#if (TFT_TOUCH_PIN_MISO >= TFT_PIN_DC) && (TFT_TOUCH_PIN_MISO <= TFT_PIN_BL)
#error "TFT_TOUCH_PIN_MISO cae en el bloque de control de la pantalla"
#endif
#if (TFT_TOUCH_PIN_MOSI >= TFT_PIN_DC) && (TFT_TOUCH_PIN_MOSI <= TFT_PIN_BL)
#error "TFT_TOUCH_PIN_MOSI cae en el bloque de control de la pantalla"
#endif
#if (TFT_TOUCH_PIN_CS >= TFT_PIN_DC) && (TFT_TOUCH_PIN_CS <= TFT_PIN_BL)
#error "TFT_TOUCH_PIN_CS cae en el bloque de control de la pantalla"
#endif
#endif  // TFT_TOUCH_ENABLE

// ---------------------------------------------------------------------------
// 5. Brillo
// ---------------------------------------------------------------------------
// Registro 0xBE del SSD1963 (duty 0..255) y/o PWM del GPIO TFT_PIN_BL.
#ifndef TFT_BL_PWM_ENABLE
#define TFT_BL_PWM_ENABLE 1
#endif

#ifndef TFT_BL_REGISTER_ENABLE
#define TFT_BL_REGISTER_ENABLE 1
#endif

// El slice/canal PWM se calculan en el codigo con pwm_gpio_to_slice_num(),
// asi este header no necesita incluir hardware/pwm.h.

// ---------------------------------------------------------------------------
// 6. PIO
// ---------------------------------------------------------------------------
// Maximo 25 MHz: por encima de eso el pulso de /WR baja de 40 ns y el bus se
// corrompe (ver tabla de trampas en .kilo/skill/ssd1963-driver/SKILL.md).
#define TFT_PIO_MAX_CLOCK_HZ 25000000

// Buffer de trabajo del backend.  Sin malloc: es miembro de la clase.
//   bus de 8 bits  -> 1020 bytes por rafaga (multiplo de 12) = 340 pixels
//   bus de 16 bits -> 1024 bytes por rafaga              = 256 pixels
#define TFT_TX_BUF_BYTES 1024

// ---------------------------------------------------------------------------
// 7. Orientacion / MADCTL
// ---------------------------------------------------------------------------
// Bit7 BGR, bit6 RGB (tabla), bit5 MH, bit4 ML, bit3 BGR?, bit2 MV, bit1 MX,
// bit0 MY. Para 480x272 en horizontal: MX|MY = 0x00 + BGR 0x08 = 0x08.
#define TFT_MADCTL_MY  0x01
#define TFT_MADCTL_MX  0x02
#define TFT_MADCTL_MV  0x04
#define TFT_MADCTL_ML  0x10
#define TFT_MADCTL_MH  0x20
#define TFT_MADCTL_BGR 0x08

#define TFT_MADCTL_DEFAULT \
    (TFT_MADCTL_BGR | TFT_MADCTL_MX | TFT_MADCTL_MY)

// ---------------------------------------------------------------------------
// 8. Textura
// ---------------------------------------------------------------------------
// Modos de transparencia de UTFT.
#define TFT_DIM_BG  1
#define TFT_KEEP_BG 2

// ---------------------------------------------------------------------------
// 9. Estructuras compartidas
// ---------------------------------------------------------------------------
namespace tft {

struct Rgb {
    uint8_t r, g, b;
};

constexpr Rgb RGB_BLACK{0, 0, 0};
constexpr Rgb RGB_WHITE{255, 255, 255};
constexpr Rgb RGB_RED{255, 0, 0};
constexpr Rgb RGB_GREEN{0, 255, 0};
constexpr Rgb RGB_BLUE{0, 0, 255};
constexpr Rgb RGB_YELLOW{255, 255, 0};
constexpr Rgb RGB_CYAN{0, 255, 255};
constexpr Rgb RGB_MAGENTA{255, 0, 255};
constexpr Rgb RGB_GREY{128, 128, 128};

// Orientacion logica de la pantalla.
enum class Orientation : uint8_t {
    kPortrait0 = 0,   // 480x272
    kPortrait90,      // 272x480
    kPortrait180,
    kPortrait270,
};

}  // namespace tft

#endif  // TFT_CONFIG_H
