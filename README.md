# pico_debugger_flash

Programación y depuración de un **Raspberry Pi Pico (RP2040)** por **SWD** usando un
segundo **Pico como Debug Probe** (protocolo CMSIS-DAP), con OpenOCD compilado desde
fuente y **sin necesidad de sudo** (regla udev).

Incluye el firmware blink de autotest, todos los scripts reproducibles y la
documentación que explica *por qué* un firmware falló y *cómo* replicar el setup
en otra PC — pensado para que cualquier persona **o IA** pueda interpretarlo y
ejecutarlo.

## Repositorios y herramientas externas (no versionados aquí)

| Componente | Origen | Nota |
|---|---|---|
| Pico SDK | https://github.com/raspberrypi/pico-sdk | variable `PICO_SDK_PATH` |
| OpenOCD (CMSIS-DAP) | https://github.com/openocd-org/openocd | binario + carpeta `tcl` |
| hidapi | https://github.com/libusb/hidapi | `libhidapi-hidraw.so.0` |
| Firmware de sonda | https://github.com/raspberrypi/debugprobe | `debugprobe_on_pico.uf2` (en `firmware/`) |

## Estructura

```
pico_debugger_flash/
├── blink/                  # Firmware de autotest (LED + USB CDC)
├── docs/                   # Toda la documentación aprendida
│   ├── DEBUGPROBE_LEARNINGS.md   # Firmwares, por qué falló, setup sin sudo (receta para IAs)
│   ├── PICO_DEBUGGER_GUIDE.md    # Guía de depuración derivada de la doc oficial de RPi
│   └── REPORT.md                  # Reporte histórico de la sesión
├── firmware/               # UF2 de la sonda (listos para BOOTSEL)
├── pics/                   # Imágenes de referencia (pinout del Pico)
├── scripts/                # Scripts portables (build + flash + udev)
└── *.cfg                   # Configuraciones OpenOCD (SWD y rescue)
```

## Requisitos

- Linux con `cmake`, `arm-none-eabi-gcc`, `autoconf`, `libusb`.
- Pico SDK clonado y `PICO_SDK_PATH` apuntando a él.
- OpenOCD con soporte CMSIS-DAP e hidapi compiladas (ver `docs/DEBUGPROBE_LEARNINGS.md` §6).
- Un Pico con `firmware/debugprobe_on_pico.uf2` (sonda) y otro Pico target.

Los scripts detectan automáticamente las rutas de SDK/OpenOCD/hidapi si están en
`../pico-sdk`, `../openocd-src` y `../hidapi-install`, y pueden sobre-escribirse:

```bash
PICO_SDK_PATH=/x/pico-sdk OPENOCD_BIN=/x/openocd ./scripts/flash_nosudo.sh
```

## Quick start

```bash
# 1) Instalar regla udev para programar sin sudo (una vez), luego RECONECTAR la sonda
sudo ./scripts/install_udev.sh

# 2) Compilar y programar el target por SWD SIN sudo
./scripts/flash_nosudo.sh
# Salida esperada: Programming Finished / Verified OK
```

Si la udev no está instalada, usar los scripts con sudo:

```bash
./scripts/build_and_program.sh     # build + program (sudo)
./scripts/flash_simple.sh          # build + program (sudo, una contraseña)
```

## Verificación del target

El blink imprime por USB CDC (target conectado por USB a la PC):

```bash
minicom -D /dev/ttyACM1 -b 115200
```

```
=== RP2040 SELF-TEST ===
CPU clock: 125 MHz
SDK version: 2.1.0
blink #1 ...
```

## Cableado SWD (Pico sonda → Pico target)

| Sonda | Target | Señal |
|---|---|---|
| GP2 | GP2 | SWDIO |
| GP3 | GP3 | SWCLK |
| GND | GND | GND |
| 3V3 | 3V3 | 3.3V (opcional) |

> El target NO debe estar en modo BOOTSEL (USB mostrando `RPI-RP2`) al usar SWD.

## Documentos clave

- [docs/DEBUGPROBE_LEARNINGS.md](docs/DEBUGPROBE_LEARNINGS.md) — diferencia entre
  `debugprobe.uf2` y `debugprobe_on_pico.uf2`, por qué fallaba la conexión, receta
  completa de instalación en otra PC y paso a paso de la regla udev.
- [docs/PICO_DEBUGGER_GUIDE.md](docs/PICO_DEBUGGER_GUIDE.md) — guía de depuración
  (VS Code, OpenOCD, GDB, rescue, serial).
- [docs/REPORT.md](docs/REPORT.md) — bitácora de la sesión de trabajo.

## Licencia

MIT — ver [LICENSE](LICENSE). Los archivos UF2 de `firmware/` provienen del proyecto
opensource `raspberrypi/debugprobe` (licencia MIT/BSD) y la documentación derivada de
*Getting Started with Raspberry Pi Pico-series* es propiedad de Raspberry Pi Ltd.