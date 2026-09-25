# Compilacion y volcado

## Requisitos

- [pico-sdk](https://github.com/raspberrypi/pico-sdk) 2.x (RP2350).
  Mientras el camino no este en la variable de entorno `PICO_SDK_PATH`, el
  `CMakeLists.txt` busca un checkout hermano: `../../pico-sdk` relativo al proyecto.
- Toolchain ARM (`arm-none-eabi-gcc`), `cmake >= 3.13`, `make`.

En un Debian/Ubuntu:

```sh
sudo apt install cmake make gcc-arm-none-eabi libnewlib-arm-none-eabi
```

## Compilar

```sh
cd src/floppydisk
cmake -B build                    # o: cmake -B build -DPICO_BOARD=pico2_w
make -C build -j"$(nproc)"
```

Artifactos: `build/floppydisk.uf2`, `.elf`, `.hex`.

La placa por defecto es `pico2_w`. Si se quiere otra (p. ej. `pico2`), pasarla con
`-DPICO_BOARD=pico2` en la configuracion.

> En RP2040 (Pico 1) el buffer de flux se reduce a 128 KiB automaticamente
> (config.h) para caber en sus 264 KiB de SRAM.

## Build + flasheo con script

`scripts/build.sh` hace el paso anterior y ademas flashea con picoprobe:

```sh
scripts/build.sh              # menu interactivo de board
scripts/build.sh pico2_w      # compila para pico2_w
scripts/build.sh pico flash   # compila y flashea via SWD (picoprobe + OpenOCD)
scripts/build.sh pico flash   # RP2040
scripts/build.sh pico2_w picotool   # flashea por bootsel USB con picotool
scripts/build.sh pico clean   # borra build_pico
```

- Boards: `pico`, `pico_w`, `pico2`, `pico2_w` (acepta alias `pico_2w`).
- Cada board usa su propio directorio `build_<board>`.
- Para el flasheo por SWD necesita `openocd` con soporte de RP2350 y un transistor
  de depuracion picoprobe conectado en SWCLK=GP2 / SWDIO=GP3.
- `OPENOCD_INTERFACE` permite cambiar el interface de OpenOCD.

## Volcado

1. Pulsa el boton **BOOTSEL** de la Pico 2 W y conecta por USB.
2. Aparece un disco `RP2350` (o `RPI-RP2`). Copia `build/floppydisk.uf2`.
3. La placa se reinicia sola y arranca la app.

Con `picotool` (si estuviera instalado):

```sh
picotool load -x build/floppydisk.uf2    # (requiere USB en modo SWD/bootrom)
```

## Depuracion

UART0 por GPIO 0/1 a 115200 8N1 esta inicializado (`stdio` de la SDK). La UI se
muestra por OLED; se puede ampliar el codigo con `printf()` para ver trazas desde
un conversor USB-UART.