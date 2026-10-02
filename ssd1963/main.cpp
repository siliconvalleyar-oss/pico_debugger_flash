// main.cpp - demo de sincronizacion del driver SSD1963.
//
// El bucle no dibuja una animacion inventada: refleja el estado real del
// firmware (LED, boton en pantalla, touch, reloj) y solo repinta las zonas que
// han cambiado, porque en una Pico no hay RAM para un framebuffer completo
// (480x272x3 = 391 KB frente a 264 KB de SRAM).

#include <math.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "tft/bus_pio.h"
#include "tft/canvas.h"
#include "tft/panel.h"
#include "tft/tft_fonts.h"
#include "tft/touch.h"

#ifndef TFT_BOARD_NAME
#define TFT_BOARD_NAME "pico"
#endif

// El LED de la placa solo existe en las placas sin radio; en la Pico W y la
// Pico 2 W se usa un GPIO libre como indicador de estado.
#if defined(PICO_DEFAULT_LED_PIN)
constexpr uint kLedPin = PICO_DEFAULT_LED_PIN;
#else
constexpr uint kLedPin = 22;
#endif

namespace {

constexpr int kFrameMs = 40;  // 25 FPS
constexpr int kMargin = 10;

tft::TouchCalibration g_cal;
tft::TouchAds7843 g_touch(&g_cal);

struct Blink {
    int period_ms = 700;
    uint64_t next_ms = 0;
    bool on = false;

    void tick(uint64_t now) {
        if (now >= next_ms) {
            on = !on;
            gpio_put(kLedPin, on);
            next_ms = now + period_ms / 2;
        }
    }
};

void draw_background(tft::Canvas &cv, int w, int h) {
    cv.fill_screen(tft::RGB_BLACK);

    // Marco y cabecera
    cv.draw_rect(kMargin, kMargin, w - 2 * kMargin, h - 2 * kMargin, tft::RGB_GREY);
    cv.fill_rect(kMargin, kMargin, w - 2 * kMargin, 30, tft::RGB_BLUE);
    cv.set_text_style(TFT_KEEP_BG);
    cv.set_text_colour(tft::RGB_WHITE);
    cv.set_text_pos(kMargin + 8, kMargin + 8);
    cv.print_string("SSD1963 480x272 - bus de 8 bits");

    // Barra de colores de referencia
    const int bar_y = kMargin + 40;
    const int bar_h = 20;
    const tft::Rgb bar[7] = {tft::RGB_RED,   tft::RGB_GREEN, tft::RGB_BLUE,
                             tft::RGB_YELLOW, tft::RGB_CYAN,  tft::RGB_MAGENTA,
                             tft::RGB_WHITE};
    const int bar_w = (w - 2 * kMargin) / 7;
    for (int i = 0; i < 7; ++i) {
        cv.fill_rect(kMargin + i * bar_w, bar_y, bar_w, bar_h, bar[i]);
    }

    // Cuadro del LED
    cv.draw_rect(kMargin + 4, bar_y + bar_h + 6, 30, 30, tft::RGB_GREY);
    cv.set_text_pos(kMargin + 42, bar_y + bar_h + 12);
    cv.print_string("LED");

    // Boton en pantalla (lo pulsa el dedo)
    cv.draw_rect(w - kMargin - 60, bar_y + bar_h + 6, 56, 30, tft::RGB_GREY);
    cv.set_text_pos(w - kMargin - 52, bar_y + bar_h + 12);
    cv.print_string("ON/OFF");

    // Circulos de prueba
    cv.draw_circle(w / 2, h / 2 + 24, 46, tft::RGB_GREEN);
    cv.set_text_pos(w / 2 - 20, h / 2 + 18);
    cv.print_string("479x272");

    // Pie: se reescribe cada segundo
    cv.fill_rect(kMargin, h - kMargin - 24, w - 2 * kMargin, 18, tft::RGB_BLACK);
}

}  // namespace

int main() {
    stdio_init_all();

    tft::PioBus bus;
    tft::Ssd1963 panel(bus);
    panel.begin();

    tft::Canvas cv(panel);
    const int w = panel.width();
    const int h = panel.height();

    cv.set_font(tft::font_8x16());
    draw_background(cv, w, h);

#if TFT_TOUCH_ENABLE
    g_touch.begin();
    // Puntos de calibracion: se rellenan pulsando en las cuatro esquinas con
    // TouchAds7843::raw_position().  De momento se usan los valores crudos
    // tipicos de un ADS7843 con el panel montado.
    g_cal.screen[0] = {10, 10};
    g_cal.screen[1] = {static_cast<int16_t>(w - 10), 10};
    g_cal.screen[2] = {static_cast<int16_t>(w - 10), static_cast<int16_t>(h - 10)};
    g_cal.screen[3] = {10, static_cast<int16_t>(h - 10)};
    g_cal.raw[0] = {300, 300};
    g_cal.raw[1] = {3800, 300};
    g_cal.raw[2] = {3800, 3800};
    g_cal.raw[3] = {300, 3800};
    g_touch.build_calibration();
#endif

    printf("SSD1963 listo en %s (bus de %d bits, ventana %dx%d)\n", TFT_BOARD_NAME,
           TFT_BUS_WIDTH, w, h);

    gpio_init(kLedPin);
    gpio_set_dir(kLedPin, GPIO_OUT);

    Blink blink;
    blink.next_ms = to_ms_since_boot(get_absolute_time()) + blink.period_ms / 2;

    uint32_t frames = 0;
    uint32_t fps = 0;
    uint32_t last_fps_ms = 0;
    uint32_t redraw_us_max = 0;

    bool button_state = false;
    bool last_button = false;
    int last_progress = -1;
    int last_second_drawn = -1;
    bool last_led = false;
    bool last_touch = false;

    const uint64_t start_us = get_absolute_time();
    uint64_t next_frame = start_us;

    for (;;) {
        const uint64_t now_us = get_absolute_time();
        const uint32_t now_ms = to_ms_since_boot(now_us);
        const uint32_t t_ms = static_cast<uint32_t>((now_us - start_us) / 1000);
        const uint32_t frame_start = now_ms;

        blink.tick(now_ms);

        // --- touch ------------------------------------------------------
        bool touching = false;
        bool touch_changed = false;
        tft::Point tp{};
#if TFT_TOUCH_ENABLE
        touching = g_touch.is_pressed();
        if (touching) {
            tp = g_touch.position();
            // Boton en pantalla
            const int bx = w - kMargin - 60;
            const int by = kMargin + 40 + 20 + 6;
            if (tp.x >= bx && tp.x < bx + 56 && tp.y >= by && tp.y < by + 30) {
                button_state = !button_state;
                touch_changed = true;
            }
        }
#endif

        const uint32_t draw_start_us = get_absolute_time();

        // --- zonas que cambian ------------------------------------------
        // 1. LED
        if (blink.on != last_led) {
            cv.fill_rect(kMargin + 5, kMargin + 40 + 20 + 7, 28, 28,
                         blink.on ? tft::RGB_GREEN : tft::RGB_BLACK);
            last_led = blink.on;
        }

        // 2. Boton en pantalla
        if (button_state != last_button) {
            const int bx = w - kMargin - 60;
            const int by = kMargin + 40 + 20 + 6;
            cv.fill_rect(bx + 2, by + 2, 52, 26,
                         button_state ? tft::RGB_GREEN : tft::RGB_BLACK);
            last_button = button_state;
        }

        // 3. Barra de progreso: una vez por vuelta, no por frame
        const int progress = static_cast<int>((t_ms % 2000) * (w - 2 * kMargin) / 2000);
        if (progress != last_progress) {
            const int bx = kMargin;
            const int by = kMargin + 40 + 20 + 40;
            const int bw = w - 2 * kMargin;
            cv.fill_rect(bx, by, bw, 12, tft::RGB_BLACK);
            if (progress > 0) {
                cv.fill_rect(bx, by, progress, 12, tft::RGB_YELLOW);
            }
            last_progress = progress;
        }

        // 4. Punto del dedo
        if (touching) {
            cv.fill_rect(tp.x - 3, tp.y - 3, 7, 7, tft::RGB_MAGENTA);
        }
        if (touch_changed && tp.x > 0) {
            cv.draw_circle(tp.x, tp.y, 6, tft::RGB_MAGENTA);
        }
        if (last_touch && !touching && tp.x > 0) {
            cv.draw_circle(tp.x, tp.y, 6, tft::RGB_GREY);
        }
        last_touch = touching;

        // 5. Texto del pie: una vez por segundo
        if (t_ms / 1000 != static_cast<uint32_t>(last_second_drawn)) {
            last_second_drawn = t_ms / 1000;
            cv.set_text_pos(kMargin + 4, h - kMargin - 22);
            cv.set_text_bg(tft::RGB_BLACK);
            cv.print("t=%lus  fps=%lu  redraw=%luus  bus=%dbit", t_ms / 1000,
                     static_cast<unsigned long>(fps),
                     static_cast<unsigned long>(redraw_us_max), TFT_BUS_WIDTH);
        }

        // --- ritmo fijo -------------------------------------------------
        const uint32_t redraw_us =
            static_cast<uint32_t>(get_absolute_time() - to_us_since_boot(draw_start_us));
        if (redraw_us > redraw_us_max) redraw_us_max = redraw_us;
        if (now_ms - last_fps_ms >= 1000) {
            fps = frames;
            frames = 0;
            last_fps_ms = now_ms;
            printf("t=%lus fps=%lu redraw=%luus boton=%d\n", t_ms / 1000,
                   static_cast<unsigned long>(fps),
                   static_cast<unsigned long>(redraw_us_max), button_state ? 1 : 0);
            redraw_us_max = 0;
        }
        ++frames;

        next_frame += static_cast<uint64_t>(kFrameMs) * 1000;
        const uint64_t now2 = get_absolute_time();
        if (next_frame < now2) next_frame = now2;  // si se ha perdido el ritmo
        absolute_time_t target = {next_frame};
        sleep_until(target);
        (void)frame_start;
    }
}
