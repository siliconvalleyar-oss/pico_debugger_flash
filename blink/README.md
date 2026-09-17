# Blink Firmware for Raspberry Pi Pico 1 (RP2040)

Firmware parpadea el LED onboard (GPIO 25) a 4 Hz y, al iniciar, imprime un
**autotest** por USB CDC (serial nativo):

```
=== RP2040 SELF-TEST ===
CPU clock: 125 MHz
SDK version: 2.1.0
LED blink test started...
blink #1
blink #2
...
```

Ver la salida serial:

```bash
minicom -D /dev/ttyACM1 -b 115200   # el target debe tener su USB conectado a la PC
```

## Hardware Setup

### Sonda / Programador (Pico A)
- Raspberry Pi Pico 1 con firmware **`debugprobe_on_pico.uf2`** (ver `firmware/`).
- IMPORTANTE: usar la variante `on_pico`, NO `debugprobe.uf2` (pines distintos; ver `docs/DEBUGPROBE_LEARNINGS.md`).

### Target (Pico B, el que se programa)
- Raspberry Pi Pico 1 (RP2040)
- SWD:

| Pico A (sonda) | Pico B (target) | Señal |
|---|---|---|
| GP2 | GP2 | SWDIO |
| GP3 | GP3 | SWCLK |
| GND | GND | GND |
| 3V3 | 3V3 | Alimentación (opcional) |

```
Debug Probe (Programmer)   Target Pico
┌────────────────────┐     ┌────────────────────┐
│  GP2 ──────────────┼─────┤  GP2 (SWDIO)       │
│  GP3 ──────────────┼─────┤  GP3 (SWCLK)       │
│  GND ──────────────┼─────┤  GND               │
│  3V3 ──────────────┼─────┤  3V3 (opcional)    │
└────────────────────┘     └────────────────────┘
```

> El target NO debe estar en modo BOOTSEL (USB que muestre `RPI-RP2`) mientras se usa SWD.

## Build

```bash
cd <repo>/blink          # o desde la raíz del repo
../scripts/build.sh
```

Salidas en `build/`: `blink.uf2` (drag & drop BOOTSEL) y `blink.elf` (SWD).

## Programar vía SWD

| Script | sudo | Descripción |
|---|---|---|
| `../scripts/flash_nosudo.sh` | No | Build + program (requiere regla udev) |
| `../scripts/flash_simple.sh` | Sí | Build + program (pide contraseña una vez) |
| `../scripts/program.sh` | Sí | Programar un build existente |
| `../scripts/flash_rescue.sh` | Sí | Rescue mode (target colgado) |

Todos los scripts detectan `PICO_SDK_PATH`, `OPENOCD_BIN` y `HIDAPI_LIB`
automáticamente; se pueden sobre-escribir con variables de entorno.

## Programar vía BOOTSEL (Drag & Drop)

1. Mantener BOOTSEL en el target, conectar USB, soltar.
2. Copiar `build/blink.uf2` a la unidad `RPI-RP2`.

## Files

- `blink.c` - Firmware de autotest (LED GPIO25 + salida USB CDC)
- `CMakeLists.txt` - Configuración CMake
- `pico_sdk_import.cmake` - Importador del SDK
- `README.md` - Este archivo

## Requirements

- Pico SDK 2.x (`PICO_SDK_PATH`)
- OpenOCD con soporte CMSIS-DAP
- hidapi (`libhidapi-hidraw.so`)
- Firmware `debugprobe_on_pico.uf2` en la sonda
- Regla udev para `2e8a:000c` si se quiere programar sin sudo (`../scripts/install_udev.sh`)