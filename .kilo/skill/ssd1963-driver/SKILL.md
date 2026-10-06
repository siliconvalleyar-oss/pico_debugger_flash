---
name: ssd1963-driver
description: Escribir, portar o depurar en C++ un driver de pantalla TFT con controlador SSD1963 (480x272, 800x480, 272x480) para Raspberry Pi Pico, Pico W, Pico 2, Pico 2 W (RP2040/RP2350) o Raspberry Pi 4, con bus paralelo por PIO/GPIO, DMA, primitivas de dibujo, fuentes, scroll, brillo y touch ADS7843. Usar cuando se pide un driver de pantalla, un controlador SSD1963, un LCD TFT para la Pico, o portar un driver existente de MicroPython/BCM2835 a C++.
---

# Skill: driver SSD1963 en C++

**Detalle técnico completo: `src/blink/docs/SSD1963.md`.** Leerlo antes de
escribir código: contiene los valores de registro, las secuencias de init
validadas, los programas PIO, los mapas de pines y la tabla de errores.

## 0. Antes de empezar: pedir estos 5 datos

No inventarlos. Preguntar en un solo turno:

1. **Placa**: `pico` | `pico_w` | `pico2` | `pico2_w` | `pi4`
2. **Panel**: resolución y modelo si se conoce (`480x272` = LB04301,
   `800x480` = AT070TN92/AT090TN10) o la foto del módulo
3. **Ancho del bus de host**: 8 bits (módulo de 40 pines) o 16 bits
4. **¿`/CS` a GND** (bus exclusivo) o a un GPIO?
5. **¿Backlight por PWM del SSD1963 (`0xBE`) o por GPIO externo?** ¿Hay
   touch ADS7843?

Con esos 5 datos el resto es determinista. Si el usuario no los tiene, elegir
valores por defecto y dejarlos en una cabecera de configuración claramente
marcada.

## 1. Arquitectura obligatoria (3 capas, 2 backends)

```
tft/panel.h/.cpp     Ssd1963   geometría, init, ventana, scroll, brillo   (C++ puro)
tft/canvas.h/.cpp    Canvas    primitivas RGB888: pixel, line, rect, circle, text
tft/bus.h            TftBus    interfaz abstracta (comando/datos/relleno/lectura)
  tft/bus_pio.h/.cpp           RP2040/RP2350: PIO + DMA          ← Pico
  tft/bus_gpio.h/.cpp          Pi 4: /dev/gpiomem GPFSET/GPFCLR   ← Pi 4
tft/tft_fonts.*      TftFont   glifos GLCD packed
pio/tft.pio          4 programas pioasm
```

Reglas de capa:

- `Ssd1963` **nunca** habla con el hardware: solo con `TftBus`. Eso es lo que
  hace portable el driver entre Pico y Pi 4.
- El formato de píxel en toda la API es **RGB888 de 3 bytes** (como la
  librería original). La conversión a 2 bytes 565 para bus de 16 bits ocurre
  **solo** en el backend, en `write_pixels()`.
- Nada de `malloc` por llamada: buffers de trabajo como miembros de la clase
  (`std::vector<uint8_t>` dimensionado al tamaño de la fuente).

## 2. Generar las fuentes de letra

Los `.c` de `fonts/` **no traen índice**. Conversión obligatoria (script en el
repo, `tools/gen_fonts.py`):

1. Leer el `.c` (GLCD Font Creator), extraer `//GLCD FontSize : W x H`.
2. Cada glifo: primer `uint16_t` = ancho, resto = pares `(mascara, datos)`.
3. Deshacer el bit-reversal, reempaquetar **por filas MSB-first** y transponer
   (igual que `cfonts_to_packed_py.py`).
4. Emitir `data[]` + `index[]` (`uint16_t` LE) + descriptor
   `{vert, horiz, nchars, first_char}`.

Alternativa rápida: convertir los `.py` (ya están empaquetados con índice) con
`ast.literal_eval`. Acceso al glifo:

```c
int rel = code - first_char; if (rel < 0 || rel >= nchars) rel = 0;
const uint8_t* g = data + index[rel];
int rows = vert, cols = (index[rel + 1] - index[rel]) * 8 / rows;
```

## 3. Procedimiento de escritura

1. **Configuración** (`tft_config.h`): pines, resolución, timings, reloj PIO,
   MADCTL, opciones de touch. Todo en constantes con comentario.
2. **PIO** (`pio/tft.pio`): `cmd_write`, `data_write`, `data_write16`,
   `cmd_data_read`. Side-set en el orden `bit0=D/C, bit1=/WR, bit2=/RD`
   (§6.1 del doc). Reloj ≤ 25 MHz por el pulso de `/WR` de 1 ciclo.
3. **Bus**: init de PIO/DMA, `pack_cmd/pack_data` con D/C en el bit 23,
   `set_xy` pre-armado de 11 palabras en un solo arranque de SM.
4. **Panel**: init con la secuencia de §3.1 (480x272) o §3.2 (800x480) del
   doc, `0xB0` con tamaño−1, relaciones de §3 verificadas.
5. **Canvas**: portar las primitivas de UTFT (`drawHLine`, `drawVLine`,
   `drawLine` con Bresenham, rectángulos rectos y "clipped", círculos
   Bresenham, `drawBitmap` con modos 1/2/4/8/16/24).
6. **Texto**: `setTextPos`/`setTextStyle`/`printString`/`printChar` con los
   modos de transparencia `DIM_BG=1`, `KEEP_BG=2`.
7. **Scroll**: `setScrollArea` (`0x33`) + `setScrollStart` (`0x37`).
8. **Brillo**: `0xBE` y/o PWM por GPIO.
9. **Touch** (opcional): ADS7843 por SPI + calibración de 4 puntos.
10. **Demo**: sincronizar el estado del firmware con la pantalla.

## 4. Demo de "sincronización" (requisito habitual)

Un bucle cooperativo con una tasa fija (~20-30 FPS) que refleja el estado real
del firmware, no una animación desconectada:

- `t = now_ms - start_ms` como reloj maestro
- LED/botón/periodo leídos del estado real (`Blink::state()`,
  `Button::press_count()`)
- Redibujar **solo las zonas que cambiaron** (la Pico no tiene RAM para un
  framebuffer completo: ver §11 del doc)
- Mostrar además FPS medidos y el tiempo de redibujado por `printf` (USB CDC)

## 5. Verificación obligatoria antes de dar por terminado

```bash
cmake -S . -B build -DPICO_SDK_PATH=/home/bee/pico/pico-sdk -DPICO_BOARD=pico_w
cmake --build build -j$(nproc)
```

- Debe compilar con `-Wall -Wextra` **sin warnings**.
- Cambiar `-DPICO_BOARD` entre `pico`, `pico_w`, `pico2`, `pico2_w` y
  comprobar que compila en los cuatro (el código debe ser idéntico).
- No inventar resultados de hardware. Si no se puede probar en la placa,
  decirlo explícitamente y entregar la tabla de puntos a verificar (§13).

## 6. Errores que ya se cometieron (no repetirlos)

| Trampa | Consecuencia |
|---|---|
| `out pins, 16` en un solo lado | no funciona en RP2040; usar dos `out pins, 8` |
| PIO a >25 MHz | `/WR` de <40 ns → bus corrupto |
| Side-set en orden `RD, WR` | D/C y strobe invertidos: pantalla muda |
| `0xB0` con el tamaño sin el −1 | media imagen corrupta |
| `0xE0 0x03` sin los 10 ms de espera | panel no arranca |
| `PICO_SDK_PATH` en el entorno apuntando a `/opt/pico-sdk` inexistente | CMake aborta aunque el `include()` sea correcto |
| `pico_generate_pio_header` genera `_program` (SDK 2.x), no `_program_default` | error de compilación |
| DMA de relleno con `INCR_READ` activo | colorea en degradado en vez de liso |
| Framebuffer completo en la Pico | no cabe en 264 KB (ver §11) |
| Derivar el driver de la librería CC BY-NC-SA |=no comercial |
