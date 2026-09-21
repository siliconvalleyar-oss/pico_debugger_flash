# Script `flash_nosudo_multi.sh`

Compila y programa el firmware de un proyecto Pico por **SWD sin sudo**.

**Fecha:** 2026-09-18
**Ubicación:** `scripts/flash_nosudo_multi.sh`
**Depende de:** `scripts/config.sh`, `scripts/build.sh`, `debugprobe-openocd.cfg`

---

## 1. Qué hace

Es un script **interactivo** que:

1. Detecta automáticamente los proyectos del repositorio (carpetas con `CMakeLists.txt`).
2. **Muestra un menú y pide la ruta** del proyecto a compilar y programar
   (número de la lista o una ruta directa pegada por el usuario).
3. Compila el firmware con `cmake` + `make` (script `build.sh`).
4. Programa el `.elf` resultante en el Pico target vía **OpenOCD + Debug Probe**
   (protocolo CMSIS-DAP por SWD) **sin `sudo`**.

> Requiere la regla udev ya instalada:
> `sudo ./scripts/install_udev.sh` (una vez) y **reconectar** la sonda.

---

## 2. Uso

```bash
./scripts/flash_nosudo_multi.sh                # modo menú (interactivo)
./scripts/flash_nosudo_multi.sh /ruta/proyecto # modo directo, sin menú
```

### Modo menú (recomendado)

```text
=== Pico Debugger Flash - Build & Program (no sudo) ===
Board: pico    (use BOARD=pico_w para Pico W)

Proyectos detectados en .../pico_debugger_flash:
   1) blink
   2) oled_ssd1306

Ingrese el número del proyecto, pegue una ruta, o 'q' para salir:
```

- Escriba el **número** (p. ej. `1`) para elegir de la lista, o
- **pegue una ruta** (relativa o absoluta) de un proyecto cualquiera, p. ej.
  `../pico_src/blink` o `/home/user/proyectos/oled_ssd1306`.
- `q` sale sin hacer nada.

El menú valida que la ruta exista y que contenga `CMakeLists.txt`; si no,
vuelve a pedir entrada hasta obtener una válida.

### Modo directo

```bash
./scripts/flash_nosudo_multi.sh ./blink
./scripts/flash_nosudo_multi.sh ../pico_src/blink
```

Con un argumento se omite el menú y se usa ese directorio directamente.

---

## 3. Variables de entorno

| Variable | Función | Ejemplo |
|---|---|---|
| `BOARD` | Placa target (afecta la config Pico SDK). | `BOARD=pico_w` (Pico W), `BOARD=pico2_w` |
| `PICO_SDK_PATH` | Ruta al Pico SDK si no está en `../pico-sdk`. | `PICO_SDK_PATH=/x/pico-sdk` |
| `OPENOCD_BIN` | Binario de OpenOCD (si no está en `$PATH`). | `OPENOCD_BIN=/x/openocd` |
| `OPENOCD_SCRIPTS` | Carpeta `tcl` de OpenOCD (detectada por `config.sh`). | `OPENOCD_SCRIPTS=/x/openocd/tcl` |
| `HIDAPI_LIB` | Carpeta con `libhidapi-hidraw.so` (necesaria para CMSIS-DAP). | `HIDAPI_LIB=/x/hidapi-install/lib` |

Ejemplo para Pico W:

```bash
BOARD=pico_w ./scripts/flash_nosudo_multi.sh
```

---

## 4. Flujo paso a paso

1. **Menú / selección** — `flash_nosudo_multi.sh` descubre los proyectos del repo
   y obtiene la ruta elegida por el usuario (o el argumento directo).
2. **Resolución** — `PROJECT` = nombre de la carpeta; `BUILD_DIR=<proyecto>/build`;
   `ELF_FILE=<build>/<proyecto>.elf`.
3. **Validaciones** — comprueba `CMakeLists.txt`, `debugprobe-openocd.cfg` y el
   binario de OpenOCD antes de tocar nada.
4. **Paso 1: build** — `build.sh` ejecuta
   `cmake -DPICO_BOARD=<board> -DPICO_SDK_PATH=<sdk> <proyecto> && make -j$(nproc)`.
5. **Paso 2: programar** — `config.sh` ejecuta OpenOCD:
   `openocd -s <tcl> -f debugprobe-openocd.cfg -c "program <proyecto>.elf verify reset exit"`
   con `LD_LIBRARY_PATH` apuntando a `libhidapi` (**sin `sudo`**).

Salida esperada al final de la grabación:

```text
** Programming Finished **
** Verify Started **
** Verified OK **
=== Programación completa: <proyecto> grabado sin sudo ===
```

---

## 5. Requisitos

- Linux con `cmake`, `arm-none-eabi-gcc`, `build-essential`.
- Pico SDK disponible (`../pico-sdk` relativo al repo o `PICO_SDK_PATH`).
- OpenOCD con driver CMSIS-DAP + acceso a `libhidapi` (ver `DEBUGPROBE_LEARNINGS.md` §6).
- Regla udev instalada (`sudo ./scripts/install_udev.sh`) y sonda reconectada.
- Cableado SWD correcto entre sonda y target:

| Sonda | Target | Señal |
|---|---|---|
| GP2 | GP2 | SWDIO |
| GP3 | GP3 | SWCLK |
| GND | GND | GND |
| 3V3 | 3V3 | 3.3 V (opcional) |

> El target **no** debe estar en modo BOOTSEL (`RPI-RP2`) al programar por SWD.

---

## 6. Diferencias con otros scripts

| Script | Sudo | Interactivo | Multi-proyecto |
|---|---|---|---|
| `flash_nosudo_multi.sh` | No | Sí (menú/ruta) | Menú con todos los proyectos + ruta manual |
| `flash_nosudo.sh` | No | No (`PROJECT` fijo) | No |
| `flash_simple.sh` | Sí | No | No |
| `build_and_program.sh` | Sí | No (`PROJECT` fijo) | No |
| `flash_rescue.sh` | No | No (fase 2 delega en `flash_nosudo_multi.sh`: menú/ruta) | Sí (vía `flash_nosudo_multi.sh`) |

El valor por defecto de otros scripts sale de `config.sh` (`PROJECT=blink`).
Este script **ignora `PROJECT`** y usa siempre la ruta indicada en el menú/argumento.

---

## 7. Solución de problemas

| Síntoma | Causa probable | Solución |
|---|---|---|
| `Error: openocd no encontrado` | OpenOCD no está en `$PATH` ni en `../openocd-src` | `OPENOCD_BIN=/ruta/openocd ./scripts/flash_nosudo_multi.sh` |
| `PICO_SDK_PATH no está definido` | No se halló `../pico-sdk` | Exportar `PICO_SDK_PATH` |
| `Error: libhidapi...` / DAP no se abre | Falta `libhidapi-hidraw.so` en `$LD_LIBRARY_PATH` | `HIDAPI_LIB=/ruta/hidapi-install/lib` |
| `Error accessing the DAP` | Cableado SWD mal, sonda no reconectada o target en BOOTSEL | Verificar cables / `lsusb` (debe verse `2e8a:000c`) / sacar target de BOOTSEL |
| `Permission denied` en `/dev/bus/usb` | Regla udev no aplicada | `sudo ./scripts/install_udev.sh` y reconectar la sonda |
| Menú atascado | Entrada vacía o ruta que no existe | El script re-pregunta hasta recibir un número válido o una ruta existente |

---

## 8. Verificación rápida

```bash
# 1) Confirmar que la sonda se ve por USB
lsusb | grep 2e8a        # => ... Raspberry Pi Debugprobe on Pico (CMSIS-DAP)

# 2) Ejecutar el menú
./scripts/flash_nosudo_multi.sh

# 3) Elegir 'blink' y comprobar que termina con "Verified OK"
```

---