# OLED SSD1306 Firmware for Raspberry Pi Pico W

Firmware que muestra información del sistema en un **display OLED SSD1306 de
128x64** por **I2C** (I2C1: **SDA=GP2, SCL=GP3**) y hace parpadear el LED
onboard del Pico W (driver CYW43).

## Hardware Setup

- **Target:** Raspberry Pi Pico W (RP2040)
- **Display:** SSD1306 128x64 (I2C, dirección 0x3C)

### Conexión OLED

| Pico W | OLED |
|---|---|
| **GP2** (pin 4) | SDA |
| **GP3** (pin 5) | SCL |
| **3V3** | VCC |
| **GND** | GND |

> **IMPORTANTE:** GP2/GP3 son los mismos pines que la sonda usa para SWD
> (SWDIO/SWCLK). Desconecta el OLED mientras programas por SWD para evitar
> interferencias, o conéctalo después del flash.

## Building

```bash
# Desde el repo
./scripts/build.sh            # sin BOARD -> Pico 1
BOARD=pico_w ./scripts/build.sh   # Pico W (recomendado)
```

Outputs:
- `build/oled_ssd1306.uf2` (drag & drop BOOTSEL)
- `build/oled_ssd1306.elf` (SWD via Debug Probe)

## Programming via Debug Probe (SWD, sin sudo)

```bash
PROJECT=oled_ssd1306 BOARD=pico_w ./scripts/flash_nosudo.sh
```

El proyecto también se compila solo dentro de `tools`/`pico_src/oled_ssd1306`:

```bash
cd /mnt/disk/src/rpico/pico_src/oled_ssd1306
BOARD=pico_w ./flash_nosudo.sh
```

## Files

- `main.c` — Pantalla de estado (I2C1 GP2/GP3, CPU 125 MHz, TICK #N) + LED
- `ssd1306.h` / `ssd1306.c` — Driver SSD1306 128x64 en modo horizontal (framebuffer 1 KB)
- `ssd1306_font.h` — Fuente 8x8 (A-Z, 0-9 y símbolos) en bitmap vertical
- `CMakeLists.txt` — Config CMake (enlaza `pico_cyw43_arch_none` en Pico W)
- `build.sh` / `flash_nosudo.sh` — Scripts de build y programa por SWD

## Requirements

- Pico SDK en `PICO_SDK_PATH` (o `../pico-sdk`)
- OpenOCD con CMSIS-DAP y regla udev (ver README raíz y `scripts/`)