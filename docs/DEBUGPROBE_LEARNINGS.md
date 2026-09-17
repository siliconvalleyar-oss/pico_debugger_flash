# Debug Probe en Pico: Firmwares, Cableado y Programación sin sudo

**Fecha:** 2026-09-17
**Autor:** sesión de OpenOCD / pruebas con el usuario (usuario `optimus`)
**Repos:** `/home/optimus/src/rpico` (alias `/mnt/disk/src/rpico`)

> Este documento está escrito para que **cualquier IA o persona pueda reproducir el
> procedimiento en otra PC**, entienda por qué un firmware no funcionó, y configure el
> sistema para programar un RP2040 por SWD **sin contraseña sudo**.

---

## 1. Resumen ejecutivo

- Se usa **un Raspberry Pi Pico como sonda de depuración (Debug Probe, protocolo CMSIS-DAP)** para programar otro Pico (target) por SWD.
- OpenOCD fue compilado desde fuente y depende de `libhidapi` (también compilada desde fuente).
- **El firmware correcto para flashear en un Pico-sonda es `debugprobe_on_pico.uf2`, NO `debugprobe.uf2`.**
- El fallo del firmware equivocado era esperable: cada variante usa **pines GPIO distintos**.
- La programación sin sudo se logra con **una regla udev** que da acceso al dispositivo USB al grupo `plugdev` (grupo al que pertenece `optimus`).

---

## 2. Mapa físico / de software

| Componente | Ruta | Rol |
|---|---|---|
| Pico sonda (probe) | en el USB como `2e8a:000c` | Habla CMSIS-DAP por USB y SWD por GPIO |
| Pico target | conectado por 4 cables al probe | Recibe blink.elf por SWD |
| Firmware del probe | origen en `/home/optimus/src/rpico/debugprobe/` | `debugprobe_on_pico.uf2` |
| Pico SDK | `/home/optimus/src/rpico/pico-sdk` | Toolchain de los firmwares |
| OpenOCD | `/home/optimus/src/rpico/openocd-src/src/openocd` | Cliente CMSIS-DAP + SWD |
| hidapi | `/home/optimus/src/rpico/hidapi-install/lib` | Librería HID para CMSIS-DAP |
| Config OpenOCD | `/home/optimus/src/rpico/debugprobe-openocd.cfg` | Driver CMSIS-DAP + target RP2040 |
| Proyecto blink | `/home/optimus/src/rpico/pico_src/blink` | Firmware de prueba del target |

**Cableado (solo válido con `debugprobe_on_pico.uf2`):**

| Pico-sonda (probe) | Pico target | Señal |
|---|---|---|
| Pin 4  (GP2) | Pin 4  (GP2) | SWDIO (datos) |
| Pin 5  (GP3) | Pin 5  (GP3) | SWCLK (reloj) |
| GND      | GND      | Referencia común |
| Pin 36 (3V3) | Pin 36 (3V3) | Alimentación del target |

**Reglas de oro:**
1. El target **NO debe estar en modo BOOTSEL** (si tiene USB conectado parece `RPI-RP2` y el SWD está deshabilitado).
2. La sonda-sonda y el target deben **compartir GND** y el target debe estar **alimentado** (3V3 o su propio USB).

---

## 3. `debugprobe.uf2` vs `debugprobe_on_pico.uf2`

Ambos son el mismo código fuente del repo Rasperry Pi `raspberrypi/debugprobe`,
compilado con una bandera de configuración distinta. La bandera se llama `DEBUG_ON_PICO`.

### 3.1 Definiciones en el código fuente

`probe_config.h` (líneas 68-72):

```c
#ifdef DEBUG_ON_PICO
#include "board_pico_config.h"
#else
#include "board_debug_probe_config.h"
#endif
```

**`board_debug_probe_config.h`** → se usa cuando `DEBUG_ON_PICO=OFF` →
firmware para la **placa Debug Probe oficial** (el accesorio de hardware que vende Raspberry Pi).
Características:
- `PROBE_IO_SWDI`: SWDIO y SWDI separados porque la placa oficial trae **level-shifter**.
- SWCLK = GPIO **12**, SWDI = GPIO **13**, SWDIO = GPIO **14**.
- Serial UART: TX=GP4, RX=GP5 (con flags de hardware, CTS/RTS).
- No expone pin nRESET.
- `iProduct = "Debug Probe (CMSIS-DAP)"`.

**`board_pico_config.h`** → se usa cuando `DEBUG_ON_PICO=ON` →
firmware para correr en un **Pico o Pico 2 normal**.
Características:
- `PROBE_IO_RAW`: usa SWDIO bidireccional directo (sin level-shifter, la propia placa Pico es 3V3).
- SWCLK = GPIO **2**, SWDIO = GPIO **3**.
- nRESET = GPIO **1** (puede resetear el target).
- Serial UART: TX=GP4, RX=GP5.
- `iProduct = "Debugprobe on Pico (CMSIS-DAP)"`.

### 3.2 Tabla comparativa

| Aspecto | `debugprobe.uf2` | `debugprobe_on_pico.uf2` |
|---|---|---|
| Hardware objetivo | Placa Debug Probe oficial | Pico / Pico 2 |
| Flag de build | `DEBUG_ON_PICO=OFF` | `DEBUG_ON_PICO=ON` |
| Pin SWCLK | GPIO 12 | GPIO 2 |
| Pin SWDIO | GPIO 14 (SWDI por GPIO 13) | GPIO 3 (modo RAW) |
| Pin nRESET | — | GPIO 1 |
| UART | GP4 TX / GP5 RX (+ CTS/RTS) | GP4 TX / GP5 RX |
| Level shifter | Sí (implícito por SWDI) | No |
| String USB | `Debug Probe (CMSIS-DAP)` | `Debugprobe on Pico (CMSIS-DAP)` |
| ¿Sirve en un Pico normal? | No (pines equivocados) | **Sí (este es el correcto)** |

### 3.3 Cómo saber qué firmware tiene la sonda (sin desmontar nada)

```bash
lsusb
# Bus 001 Device 005: ID 2e8a:000c Raspberry Pi Debugprobe on Pico (CMSIS-DAP)

lsusb -v -d 2e8a:000c | grep iProduct
# iProduct                2 Debugprobe on Pico (CMSIS-DAP)   ← variante Pico
# iProduct                2 Debug Probe (CMSIS-DAP)          ← variante Debug Probe oficial
```

---

## 4. Por qué `debugprobe.uf2` NO funcionó

### 4.1 Síntoma

OpenOCD detectaba la sonda **perfectamente** por USB:

```
Info : Using CMSIS-DAPv2 interface with VID:PID=0x2e8a:0x000c, serial=E660C0D1C7309C30
Info : CMSIS-DAP: Interface ready
Error: Failed to connect multidrop rp2040.dap0
```

### 4.2 Causa raíz

La conexión USB es idéntica en ambos firmwares (el RP2040 siempre usa los **mismos pines
dedicados de USB**, independiente de la configuración de la placa). Por eso la sonda
"aparecía". El problema está **en el lado SWD**:

- El cableado instalado seguía el pinout **de la variante Pico**: Pin 4=GP2 (SWDIO) y Pin 5=GP3 (SWCLK).
- El firmware `debugprobe.uf2` (variante Debug Probe oficial) estaba **manejando los GPIO 12/13/14**.
- Resultado: los pines físicos que tocan el target estaban **inactivos** → ningún reloj SWD llegaba al RP2040 → el DAP del target jamás respondía → `Failed to connect multidrop rp2040.dap0`.

En resumen: **mismatch entre el pinout del firmware y el cableado físico**. No era un
problema de OpenOCD, ni de udev, ni de electrónica (al final sí funcionó al usar el
firmware coherente con el cableado).

### 4.3 Confirmación

Tras flashear `debugprobe_on_pico.uf2` (mismo cableado GP2/GP3 intacto), el mismo
comando funcionó:

```
Info : SWD DPIDR 0x0bc12477, DLPIDR 0x00000001
Info : [rp2040.core0] Cortex-M0+ r0p1 processor detected
** Programming Started ** ... ** Verified OK ** ... shutdown command invoked
```

**Lección general:** cuando una sonda CMSIS-DAP "se ve" en USB pero no conecta por SWD,
verificar SIEMPRE el **pinout del firmware** frente al **cableado físico** antes de tocar
configuraciones, drivers o permisos.

---

## 5. Pasos para que la programación funcione sin sudo

La causa del sudo es que el nodo USB queda con permisos `root:root`:

```
crw-rw-r-- 1 root root 189, 4 ... /dev/bus/usb/001/005
```

Solo root puede escribir. La solución es **una regla udev** que asigne mode `0666` y/o
grupo `plugdev` (grupo donde está el usuario habitual).

### 5.1 Regla udev (archivo)

`/etc/udev/rules.d/99-pico-debugprobe.rules`:

```udev
# Raspberry Pi Pico / Debug Probe (CMSIS-DAP) - acceso sin sudo
SUBSYSTEM=="usb", ATTR{idVendor}=="2e8a", MODE="0666", GROUP="plugdev"
```

### 5.2 Instalación (una sola vez, requiere contraseña)

```bash
sudo cp /tmp/opencode/99-pico-debugprobe.rules /etc/udev/rules.d/99-pico-debugprobe.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

> IMPORTANTE: las reglas nuevas no se aplican retroactivamente al dispositivo ya
> conectado. **Desconectar y reconectar el Pico sonda** tras instalar la regla.

### 5.3 Verificación (sin sudo)

```bash
lsusb | grep 2e8a
# Bus 001 Device 006: ID 2e8a:000c Raspberry Pi Debugprobe on Pico (CMSIS-DAP)
ls -la /dev/bus/usb/001/006     # ahora debe aparecer rw-rw-rw- o plugdev
```

Si ya no pide sudo, ejecutar el script de flasheo:

```bash
cd /home/optimus/src/rpico/pico_src/blink
./flash_nosudo.sh
```

### 5.4 Por qué esta vía es la recomendable

- No modifica permisos manuales que se pierden al reconectar.
- Es declarativa y reproducible: se copia un archivo de reglas y se recgana el udev.
- Cualquier usuario del grupo `plugdev` (o cualquiera, con MODE 0666) puede usar la sonda.
- En otra PC, basta repetir la Sección 5.2.

---

## 6. Instalación completa desde cero en otra PC (receta para IA)

Todo lo que sigue es lo que se hizo y funcionó en esta máquina. Orden recomendado.

### 6.1 Dependencias del sistema

```bash
sudo apt update
sudo apt install -y git cmake gcc-arm-none-eabi libnewlib-arm-none-eabi \
    build-essential autoconf automake libtool pkg-config libusb-1.0-0-dev \
    libusb-dev texinfo gcc make
```

### 6.2 Pico SDK

```bash
export PICO_SDK_PATH=/home/optimus/src/rpico/pico-sdk
# (o usar la ruta que corresponda; git clone https://github.com/raspberrypi/pico-sdk)
```

### 6.3 hidapi (necesario para CMSIS-DAP de OpenOCD)

```bash
cd /home/optimus/src/rpico
git clone https://github.com/libusb/hidapi.git
cd hidapi
./bootstrap
./configure --prefix=/home/optimus/src/rpico/hidapi-install
make -j"$(nproc)"
make install
# Resultado: /home/optimus/src/rpico/hidapi-install/lib/libhidapi-hidraw.so.0
```

### 6.4 OpenOCD desde fuente (con soporte CMSIS-DAP)

```bash
cd /home/optimus/src/rpico
git clone https://github.com/openocd-org/openocd.git openocd-src
cd openocd-src
git submodule update --init --recursive
./bootstrap
./configure --enable-cmsis-dap --enable-internal-jimtcl --disable-werror
make -j"$(nproc)"
# Binario: src/openocd — probar:
LD_LIBRARY_PATH=/home/optimus/src/rpico/hidapi-install/lib src/openocd --version
```

> OpenOCD necesita encontrar `libhidapi-hidraw.so.0`. Como no está en `/usr/local/lib`,
> se pasa con `LD_LIBRARY_PATH`. Alternativa: `sudo make install` para OpenOCD y copiar
> libhidapi a `/usr/local/lib` + `sudo ldconfig`.

### 6.5 Firmware de la sonda (Pico como Debug Probe)

1. Descargar o compilar `debugprobe_on_pico.uf2`.
   - Descarga oficial: `https://github.com/raspberrypi/debugprobe/releases/latest`.
   - Compilar (variante Pico):
     ```bash
     cd /home/optimus/src/rpico/debugprobe
     git submodule update --init --recursive
     mkdir -p build-pico && cd build-pico
     cmake -DDEBUG_ON_PICO=ON -DPICO_BOARD=pico ..   # PICO_SDK_PATH definido
     make -j"$(nproc)"
     cp debugprobe_on_pico.uf2 /home/optimus/src/rpico/debugprobe_on_pico.uf2
     ```
2. Flashear: mantener **BOOTSEL** en el Pico-sonda, conectar USB, soltar, aparece
   `RPI-RP2`:
   ```bash
   cp debugprobe_on_pico.uf2 /media/$USER/RPI-RP2/
   ```
3. Verificar:
   ```bash
   lsusb            # ID 2e8a:000c Raspberry Pi Debugprobe on Pico
   sudo dmesg | tail -20
   ```

### 6.6 Regla udev (para no usar sudo) — Sección 5

```bash
sudo tee /etc/udev/rules.d/99-pico-debugprobe.rules >/dev/null <<'EOF'
SUBSYSTEM=="usb", ATTR{idVendor}=="2e8a", MODE="0666", GROUP="plugdev"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
# IMPORTANTE: desconectar y reconectar la sonda
```

### 6.7 Config de OpenOCD

`/home/optimus/src/rpico/debugprobe-openocd.cfg`:

```tcl
adapter driver cmsis-dap
adapter usb vid_pid 0x2e8a 0x000c
adapter speed 1000
source [find target/rp2040.cfg]
init
```

### 6.8 Script de programación sin sudo

`/home/optimus/src/rpico/pico_src/blink/flash_nosudo.sh` (resumen):

```bash
#!/bin/bash
set -e
ELF=/home/optimus/src/rpico/pico_src/blink/build/blink.elf
LD_LIBRARY_PATH=/home/optimus/src/rpico/hidapi-install/lib \
  /home/optimus/src/rpico/openocd-src/src/openocd \
  -s /home/optimus/src/rpico/openocd-src/tcl \
  -f /home/optimus/src/rpico/debugprobe-openocd.cfg \
  -c "program ${ELF} verify reset exit"
```

### 6.9 Prueba

```bash
cd /home/optimus/src/rpico/pico_src/blink
./flash_nosudo.sh
# Debe terminar con:
# ** Programming Finished ** / ** Verified OK ** / shutdown command invoked
```

---

## 7. Autotest del target (blink modificado)

El `blink.c` actual habilita salida serial por USB CDC (`pico_enable_stdio_usb 1`) e
imprime por cada parpadeo: reloj del CPU, versión del SDK y contador `blink #N`.

```bash
sudo apt install -y minicom
minicom -D /dev/ttyACM0 -b 115200   # o: screen /dev/ttyACM0 115200
```

Debería verse:

```
=== RP2040 SELF-TEST ===
CPU clock: 125 MHz
SDK version: 2.1.0
LED blink test started...
blink #1
blink #2
...
```

Paralelamente, el LED onboard (GPIO 25) parpadea a 4 Hz.

---

## 8. Checklist final

- [ ] Sonda enumera como `Debugprobe on Pico (CMSIS-DAP)`.
- [ ] Cableado GP2/GP3 + GND + 3V3 entre sonda y target.
- [ ] Target fuera de BOOTSEL (sin USB que lo ponga en modo RPI-RP2).
- [ ] Regla udev instalada y sonda reconectada (nodo accesible sin sudo).
- [ ] `LD_LIBRARY_PATH` apunta a hidapi-install/lib.
- [ ] `./flash_nosudo.sh` hace `Programming Finished / Verified OK`.
- [ ] `minicom /dev/ttyACM0` muestra el autotest del target.

---

## 9. Errores vistos y su causa (registro histórico)

| Error | Causa real | Solución |
|---|---|---|
| `Failed to connect multidrop rp2040.dap0` | Firmware `debugprobe.uf2` con cableado de variante Pico (mismatch de pines) | Flashear `debugprobe_on_pico.uf2` |
| `libhidapi-hidraw.so.0: cannot open` | hidapi instalada en prefijo local | `LD_LIBRARY_PATH=hidapi-install/lib` o instalarla en /usr/local |
| `can't find interface/cmsis-dap.cfg` | se omitió `-s <tcl>` | pasar `-s /home/optimus/src/rpico/openocd-src/tcl` |
| `could not open device: Access denied` | permisos root:root del USB | regla udev (Sección 5) |
| Trigger en BOOTSEL: no conecta | RP2040 en modo RPI-RP2 desactiva SWD | quitar USB del target/no entrar en BOOTSEL |