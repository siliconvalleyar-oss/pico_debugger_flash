# floppydisk

Lectora / grabadora de disquetes 3.5" basada en **Raspberry Pi Pico 2 W (RP2350)**.

Lee un disquete real a traves de la interfaz Shugart de 34 pines y lo guarda como
imagen `.ima` en una microSD FAT32 (SPI), y escribe imagenes `.ima` de la microSD
en un disquete real. El estado se muestra en un OLED SSD1306 de 128x64 (I2C) y se
controla con dos botones.

## Funciones

| Funcion  | Descripcion                                                |
|----------|-------------------------------------------------------------|
| LEER     | Volca un disquete a `IMG/DISK0001.IMA` (el numero se autoincrementa) |
| ESCRIBIR | Selector de slots: elige con A/B cualquier `IMG/DISK*.IMA` o `*.HFE` y lo graba en el disquete |
| INFO     | Estado de la microSD: espacio libre y capacidad            |

El formato se detecta automaticamente al leer (1.44 MB de alta densidad o
720 KB de doble densidad via MFM). Al escribir `.ima`, el tamano de la imagen
decide el formato; al escribir `.hfe` se deduce de su bitrate (o se fuerza con
`FF.CFG`).

## Caracteristicas

- **C++17**, SDK pico-sdk 2.x, placas `pico2_w` (RP2350) y `pico` (RP2040).
- **PIO**: captura y escritura de flujo de transiciones de flujo ("flux") a 24 MHz
  (unidad de 41.67 ns), sincronizadas por el pulso de INDEX.
- **CoDec MFM** portado de Adafruit_Floppy (MIT), con deteccion automatica de
  densidad y CRC-16. `gap3` = 108 (compatible FlashFloppy) y configurable.
- **Imagenes HFE** (HxC v1/v2/v3): escritura a disquete real a partir del
  bitstream NRZ por cilindro (opcodes v3 incluidos).
- **Config**: `FF.CFG` en la raiz de la SD (gap3, densidad HD/DD).
- **FAT16/FAT32** propio sobre microSD SPI (mount, directorio `IMG`, *scan*
  ordenado de imagenes, creacion con pre-reserva de clusters, cache LRU de
  sectores, reescritura de la tabla).
- UART de depuracion (115200 8N1) en GP0/GP1.

## Estructura del proyecto

```
config.h        Pines, tiempos, formatos, buffering
floppy.pio      Programas PIO fluxread / fluxwrite
floppy.h/.cpp   Control de la disquetera + captura/escritura de flux
mfm.h/.cpp      CoDec MFM (port de Adafruit_Floppy)
fat.h/.cpp      FS FAT16/FAT32 minimo sobre la microSD + cache de bloques
sd_spi.h/.cpp   Driver SPI de la microSD
cfg.h/.cpp      Configuracion FF.CFG (gap3, densidad)
hfe.h/.cpp      Imagenes HFE v1/v2/v3 -> flux de escritura
ssd1306.h/.cpp  Driver del OLED
app.h/.cpp      Maquina de estados de la UI (menu LEER/ESCRIBIR/INFO)
main.cpp        Init de stdio, OLED y arranque de la app
```

## Documentacion

- [hardware.md](docs/hardware.md) — cableado 34 pines, microSD, OLED y botones.
- [build.md](docs/build.md) — compilacion y volcado del firmware.
- [usage.md](docs/usage.md) — uso en menu.
- [firmware.md](docs/firmware.md) — arquitectura interna (PIO, MFM, FAT).
- [image-format.md](docs/image-format.md) — formato de las imagenes `.ima`.

## Compilacion rapida

`scripts/build.sh` compila y flashea con picoprobe, eligiendo la board:

```sh
scripts/build.sh              # menu interactivo de board
scripts/build.sh pico2_w      # compila para Pico 2 W (por defecto)
scripts/build.sh pico flash   # compila y flashea (Pico 1 / RP2040)
scripts/build.sh pico2_w picotool   # flashea por bootsel USB
```

Boards soportadas: `pico`, `pico_w`, `pico2`, `pico2_w` (alias `pico_2w`).