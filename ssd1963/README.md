# Driver SSD1963 para Raspberry Pi Pico / Pico W / Pico 2 / Pico 2 W

Driver en C++17 de un TFT con controlador **SSD1963** (bus paralelo 8080) sobre
el PIO y la DMA de la Pico, pensado para un panel de **480x272** (LB04301) con
touch resistivo **ADS7843** opcional.

| Placa | Chip | Estado |
|---|---|---|
| `pico` | RP2040 | compila, 0 warnings |
| `pico_w` | RP2040 | compila, 0 warnings |
| `pico2` | RP2350 | compila, 0 warnings |
| `pico2_w` | RP2350 | compila, 0 warnings |

Los cuatro usan **el mismo código**: no hay ni un `#ifdef` de placa.

## Compilar

```bash
cmake -S . -B build/pico2 -DPICO_SDK_PATH=/ruta/al/pico-sdk -DPICO_BOARD=pico2
cmake --build build/pico2 -j$(nproc)
```

Sale `build/pico2/ssd1963.elf`. Para generar el UF2:

```bash
picotool convert build/pico2/ssd1963.elf ssd1963.uf2
```

Verificado con `-Wall -Wextra` sin un solo warning en las cuatro placas, en
bus de 8 y de 16 bits (8 combinaciones). No se ha podido probar en un panel
real: ver **Puntos a verificar en hardware** más abajo.

## Cómo se organiza

```
tft/tft_config.h   un solo archivo con pines, geometría y tiempos
tft/bus.h          TftBus: interfaz del bus, sin nada de hardware
tft/bus_pio.*      backend RP2040/RP2350: PIO + DMA + GPIO
tft/panel.*        Ssd1963: geometría, init, ventana, scroll, brillo  (C++ puro)
tft/canvas.*       primitivas RGB888 y texto
tft/tft_fonts.*    glifos empaquetados
tft/touch.*        ADS7843 por SPI y calibración
pio/tft.pio        3 programas PIO
tools/gen_fonts.py generador de las fuentes
main.cpp           demo de sincronización
```

Reglas que se respetan:

- `Ssd1963` **no habla nunca con el hardware**, solo con `TftBus`. Por eso el
  panel es C++ puro y se podría portar a una Raspberry Pi 4 con otro backend.
- El formato de píxel en toda la API es **RGB888 de 3 bytes**. La conversión a
  RGB565 del bus de 16 bits ocurre **solo** en el backend, en `write_pixels()`.
- **Cero `malloc`**: los buffers de trabajo (1 KB de conversión, 12 bytes de
  patrón de relleno, 96 bytes de texto) son miembros de la clase. Total de RAM
  estática: 2.3 KB.

## Mapa de pines

| Señal | 8 bits | 16 bits | Notas |
|---|---|---|---|
| D0..D7 | GP0..GP7 | GP0..GP7 | grupo OUT del PIO, contiguo |
| D8..D15 | — | GP8..GP15 | solo en bus de 16 bits |
| D/C | GP16 | GP16 | GPIO de CPU |
| /WR | GP17 | GP17 | side-set bit 0 del PIO |
| /RD | GP18 | GP18 | side-set bit 1 del PIO (el driver no lee) |
| /CS | GP19 | GP19 | GPIO de CPU |
| /RST | GP20 | GP20 | GPIO de CPU |
| Backlight | GP21 | GP21 | PWM por GPIO (slice A) |
| Touch SPI | GP12..GP15 | GP22..GP25 | ver aviso abajo |
| Touch SPI | | | SCK, MISO, MOSI, CS |

**Aviso Pico W / Pico 2 W**: GP23..GP25 los usa la radio (SDIO del CYW43 y del
RM2W). En el bus de 8 bits (el de por defecto) el touch está en GP12..GP15 y no
hay conflicto. En bus de 16 bits el touch cae en GP22..GP25 y, si algún día
hace falta WiFi, habrá que mover el display o el touch.

## Cambiar el bus a 16 bits

Un valor, y el resto se adapta (pines, empaquetado a RGB565, programa PIO):

```bash
cmake -S . -B build16 -DPICO_SDK_PATH=/ruta/al/pico-sdk -DPICO_BOARD=pico2 \
      -DTFT_BUS_WIDTH=16
```

## Cómo funciona la transmisión

Lo importante del backend es que **no hay esperas por tiempo** ni bytes de
colación:

1. Cada programa de PIO recibe como **primera palabra de 32 bits el número de
   palabras de datos** que hay que enviar, y a continuación esas palabras.
2. La DMA las mete en la FIFO de salida (`read_increment` sí, `write_increment`
   no: la palabra se repite en la FIFO, no en la memoria).
3. El programa cuenta en Y y, al llegar a cero, cae en un bloque `end` y se
   queda bloqueado en su `pull`. Esa dirección es el **fin exacto** de la
   transmisión, y se comprueba con `pio_sm_get_pc()`.
4. `/CS` lo lleva la CPU como GPIO normal, no va en el side-set. Así se puede
   mantener bajo durante toda una ráfaga de DMA, y una state machine parada
   nunca puede meter datos en el panel: `/WR` solo lo baja el PIO.

Los últimos pixels que no llenan una palabra de 32 bits (3 como máximo) van
por el programa corto, sin DMA, para no partir un pixel por la mitad. Por eso
`write_pixels()` nunca mete ni un pixel de más en la ventana.

D/C también es GPIO de CPU, y por eso el mismo programa sirve para un byte de
comando, para un parámetro y para una ráfaga de píxeles.

Rendimiento medido en ciclo de PIO (25 MHz, 4 bytes por palabra):

| Bus | ciclos por 4 bytes | píxeles/s | pantalla completa |
|---|---|---|---|
| 8 bits (RGB888) | 13 | ~2,5 M | ~52 ms (19 FPS) |
| 16 bits (RGB565) | 12 | ~4,1 M | ~31 ms (32 FPS) |

## Fuentes de letra

`tft/font_glcd_8x8.h` y `tft/font_glcd_8x16.h` están generadas y versionadas.
Para regenerarlas, o para traer una fuente de GLCD Font Creator:

```bash
python3 tools/gen_fonts.py --psf /usr/share/consolefonts/Lat15-VGA16.psf.gz \
    --name Font8x16 --out tft/font_glcd_8x16.h

python3 tools/gen_fonts.py --psf /usr/share/consolefonts/Lat15-VGA8.psf.gz \
    --name Font8x8 --out tft/font_glcd_8x8.h

python3 tools/gen_fonts.py --glcd fonts/mifont.c --name FontMi --out tft/font_glcd_mi.h

python3 tools/gen_fonts.py --psf /usr/share/consolefonts/Lat15-VGA16.psf.gz \
    --preview "Prueba"     # dibuja la fuente en la consola
```

El formato de salida es el que espera `tft_fonts.cpp`:
`data[]` sin huecos + `index[]` de offsets `uint16_t` + descriptor
`{vert, horiz, nchars, first_char}`, con los bits de cada fila en MSB primero y
el carácter alineado a la izquierda.

## La demo

`main.cpp` es una demo de **sincronización**, no una animación suelta:

- reloj maestro `t = now - arranque`, ritmo fijo de 25 FPS;
- el LED, el botón en pantalla y el touch se leen del estado real;
- **solo se repinta lo que cambia**: el LED, el botón, la barra de progreso (una
  vuelta cada 2 s) y el texto del pie (una vez por segundo);
- por USB CDC se imprimen los FPS medidos y el peor tiempo de redibujado.

En la Pico no hay RAM para un framebuffer completo (480x272x3 = 391 KB frente a
264 KB de SRAM), así que las primitivas abren su ventana y dibujan solo lo suyo.

## Puntos a verificar en hardware

**Nada de esto se ha podido probar en un panel real.** Lo que sigue es lo que
hay que mirar en la primera prueba, en este orden. La estructura del driver, el
mapa de pines, el empaquetado y los finales de transmisión son deterministas y
están verificados por compilación y por el desensamblado del PIO; lo que no
está verificado son los **valores de reloj y temporización** del controlador.

| # | Qué comprobar | Dónde | Síntoma si está mal |
|---|---|---|---|
| 1 | Cableado: D/C, /WR, /CS, /RST y el bus en el orden correcto | `tft_config.h` | pantalla muda |
| 2 | Polaridad de /WR y de /CS (activos a nivel bajo) | cableado | pantalla muda |
| 3 | `0xE0 0x03` con los 10 ms de espera | `panel.cpp` (`begin`) | panel no arranca, se ve ruido |
| 4 | Reloj del PLL: `0xE2 0x23 0x02 0x54` y `0xE3 0x81` | `panel.cpp` | imagen tearing o panel sin señal |
| 5 | Periodos de barrido: `kHTotal`, `kVTotal`, `kHBlank`, `kVBlank` | `panel.cpp` | imagen descentrada, líneas de más, oRefresh vertical raro |
| 6 | Interfaz: `0xF0` con `0x01` (16 bits) o `0x00` (8 bits) | `panel.cpp` | bytes descuadrados, imagen doble |
| 7 | `0xB0` con el ancho **menos uno** | `panel.cpp` | mitad derecha de la imagen corrupta |
| 8 | Pixel clock a 25 MHz o menos | `tft_config.h` | bus corrupto, imagen con ruido |
| 9 | `MADCTL` y rotación (BGR) | `tft_config.h` | colores R y B intercambiados |
| 10 | Brillo: registro `0xBE` y/o PWM de GP21 | `panel.cpp` / `bus_pio.cpp` | retroiluminación fija o apagada |
| 11 | Modo SPI del ADS7843 (`TFT_TOUCH_SPI_MODE`, por defecto 1) | `tft_config.h` | lecturas a 0 o a 4095 |
| 12 | Orientación del touch (X/Y intercambiados) | `touch.cpp` | el punto va al revés |

Los valores de gamma (`kGamma`) y VCOM (`0xE5 0x00 0x18 0x3F`) son los
habituales de la hoja de datos; si el contraste se ve raro, son los primeros a
tocar después de los puntos 4 y 5.

## Decisiones que conviene conocer

- **No hay framebuffer**: cada primitiva abre su ventana. Es lo que permite
  funcionar en 264 KB de SRAM.
- **`/CS` y D/C son GPIO de CPU, no side-set.** El side-set se queda con `/WR` y
  `/RD`, que es lo único que el PIO tiene que generar con precisión de ciclo.
- **DMA de relleno con patrón de 12 bytes en bus de 8 bits**: repetir un patrón
  de 4 bytes daría un degradado en vez de un color plano, porque el stream es
  `R G B R G B ...`.
- **`in pins, 16` en el programa de 16 bits** solo sirve para desplazar el ISR y
  sacar el segundo píxel; lo que se lee en los pines da igual.
