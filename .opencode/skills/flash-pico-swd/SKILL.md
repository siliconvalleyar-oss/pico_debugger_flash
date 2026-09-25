---
name: flash-pico-swd
description: Programar/graber firmware de Raspberry Pi Pico (RP2040/RP2350) por SWD usando un Pico como Debug Probe (CMSIS-DAP), sin sudo. USAR cuando la tarea mencione flash, grabar, programar, flashear, SWD, debugprobe, debug probe, openocd, uf2, pico_w, RP2040, RP2350 o los scripts flash_nosudo_multi.sh / build.sh / config.sh de este repo.
---

# Skill · Flashear Pico por SWD con un Pico como Debug Probe

Receta reproducible para compilar y grabar firmware en una Raspberry Pi Pico o
Pico W por **SWD sin sudo**, con un segundo Pico como sonda CMSIS-DAP.

**Resultado final (lema):**

```bash
./scripts/flash_nosudo_multi.sh <proyecto>          # target Pico
BOARD=pico_w ./scripts/flash_nosudo_multi.sh <proyecto>  # target Pico W
```

## 1. Cadena de build/programación (lo que YA existe en disco)

```
config.sh ──BUILD──▶ build.sh ──cmake+ninja──▶ build/<proyecto>/src/<target>.elf
    ▲  red                                                                  │
    └── heredado por flash_nosudo_multi.sh ──▶ openocd (CMSIS-DAP) ──SWD──▶ target
```

- `scripts/config.sh` — detecta `PICO_SDK_PATH` (`../pico-sdk`), `OPENOCD_BIN`,
  `OPENOCD_SCRIPTS` y `HIDAPI_LIB`. Todo sobre-escribible por entorno.
- `scripts/build.sh` — cmake + ninja; deja el ELF/UF2 en `build/src/<target>`.
- `scripts/flash_nosudo_multi.sh <dir>` — modo directo: compila y programa por
  SWD **sin sudo** (requiere la regla udev, §4).

## 2. Cheat sheet

| Tarea | Comando |
|---|---|
| Compilar + programar Pico (target Pico 1) | `./scripts/flash_nosudo_multi.sh <proyecto>` |
| Compilar + programar Pico W | `BOARD=pico_w ./scripts/flash_nosudo_multi.sh <proyecto>` |
| Pico 2 W | `BOARD=pico2_w ./scripts/flash_nosudo_multi.sh <proyecto>` |
| Solo compilar | `BOARD=<board> ./scripts/build.sh` |
| Ver sonda en USB | `lsusb \| grep 2e8a` → `2e8a:000c CMSIS-DAP` |
| Ver firmware de la sonda | `lsusb -v -d 2e8a:000c \| grep iProduct` |

> El argumento del script puede ser una ruta de carpeta con `CMakeLists.txt`
> (p. ej. `keyboard_oled`). El target CMake se infiere del `add_executable` del
> proyecto (raíz o `src/`).

## 3. Firmware de la sonda y cableado (lección clave)

**Regla de oro:** si la sonda es un Pico normal, usar **`debugprobe_on_pico.uf2`**
(bandera `DEBUG_ON_PICO=ON`). El `debugprobe.uf2` (Debug Probe oficial) usa
GPIO **12/13/14**; en un Pico normal cableado a GP2/GP3 **NUNCA conecta**
(`Failed to connect multidrop rp2040.dap0`) aunque se vea en USB — el mismatch
es de pines SWD, no de USB ni de OpenOCD.

| Variante | Firmware | Pines SWD | iProduct |
|---|---|---|---|
| Debug Probe oficial | `debugprobe.uf2` | SWCLK=GP12, SWDI=GP13, SWDIO=GP14 | `Debug Probe (CMSIS-DAP)` |
| Pico normal | `debugprobe_on_pico.uf2` ✅ | SWCLK=GP2, SWDIO=GP3, nRESET=GP1 | `Debugprobe on Pico (CMSIS-DAP)` |

**Cableado sonda → target** (solo con `debugprobe_on_pico.uf2`):
GP2→GP2 (SWDIO), GP3→GP3 (SWCLK), GND→GND, 3V3→3V3 (opcional).

Reglas:
1. Target **NO** en modo BOOTSEL (SWD deshabilitado). Desconectar USB del target.
2. Target alimentado y GND compartido con la sonda.

## 4. Sin sudo: regla udev

El nodo USB sale `root:root`. Para grabar sin sudo:

```udev
# /etc/udev/rules.d/99-pico-debugprobe.rules
SUBSYSTEM=="usb", ATTR{idVendor}=="2e8a", MODE="0666", GROUP="plugdev"
```

Instalar (una vez): `sudo udevadm control --reload-rules && sudo udevadm trigger`
y **desconectar/reconectar la sonda** (las reglas no se aplican retroactivamente).

## 5. Señales de éxito de OpenOCD

```
Info : SWD DPIDR 0x0bc12477, DLPIDR 0x00000001
** Programming Started ** / ** Verify Started ** / ** Verified OK **
```

`Verify` re-lee la flash por SWD: si dice OK, el target quedó programado.

## 6. Solución de problemas

| Síntoma | Causa | Arreglo |
|---|---|---|
| `Failed to connect multidrop rp2040.dap0` | Firmware de sonda con pines equivocados / target sin alimentar / GND suelto / target en BOOTSEL | Verificar iProduct, cableado GP2/GP3, quitar USB del target |
| Sonda se ve pero no conecta | Mismatch pinout firmware vs cableado (GP12/13/14 vs GP2/GP3) | Flashear `debugprobe_on_pico.uf2` |
| `libhidapi-hidraw.so: cannot open` | hidapi en prefijo local | `HIDAPI_LIB=.../hidapi-install/lib` (lo hace `config.sh`) |
| `Permission denied` | udev no aplicada | Instalar regla y reconectar la sonda |
| Build `[100%]` pero LED no prende | `BOARD` equivocado (Pico W: LED por CYW43, no GPIO25) | `BOARD=pico_w` y limpiar `build/` |

## 7. Trampas conocidas en este repo

- **`keyboard_oled` declara el target en `src/CMakeLists.txt`** (no en la raíz);
  `flash_nosudo_multi.sh` ya cae al `src/` cuando la raíz no tiene
  `add_executable`. No "arreglar" el CMake moviendo el target.
- Al cambiar `BOARD`, **limpiar `build/`** (`rm -rf build`) para evitar caché CMake
  de la otra placa (`grep PICO_BOARD build/CMakeCache.txt` para confirmar).

## 8. Referencias

- `docs/SKILL.md`, `docs/DEBUGPROBE_LEARNINGS.md`, `docs/PICO_DEBUGGER_GUIDE.md`,
  `docs/FLASH_NOSUDO_MULTI.md`
- Formato de sonda: https://github.com/raspberrypi/debugprobe