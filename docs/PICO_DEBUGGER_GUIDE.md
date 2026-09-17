# Raspberry Pi Pico Series - Guía de Depuración con Pico Probe Debugger

**Documento derivado de:** *Getting Started with Raspberry Pi Pico-series* (RP-008276-DS-2) - Versión 03/07/2026

---

## 1. Requisitos Previos

### Hardware Necesario
- **Raspberry Pi Pico-series device** (Pico, Pico W, Pico 2, Pico H, Pico WH, etc.)
- **Cable Micro USB** para alimentación y programación
- **Raspberry Pi Debug Probe** (recomendado) **O** un segundo Pico/Pico 2 con firmware `debugprobe`

### Software Necesario
- **Visual Studio Code** + Extensión "Raspberry Pi Pico" (método recomendado)
- **OpenOCD** (para depuración desde línea de comandos)
- **GDB** / **gdb-multiarch** / **lldb** (según plataforma)

---

## 2. Métodos de Depuración Disponibles

### Opción A: Raspberry Pi Debug Probe (Hardware Oficial)
El método más simple y recomendado. Proporciona:
- **Serial Wire Debug (SWD)** - para depuración de bajo nivel
- **USB-to-Serial bridge (UART)** - para consola serial genérica

### Opción B: Segundo Pico/Pico 2 con firmware `debugprobe`
Convierte un Pico en un adaptador SWD + UART. Requiere flashear el firmware `debugprobe_on_pico.uf2` (o `debugprobe_on_pico2.uf2` para Pico 2).

---

## 3. Conexiones de Cableado (Wiring)

### 3.1 Debug Probe oficial → Pico H / Pico WH (con header JST-SH)

| Debug Probe | Pico H / WH |
|-------------|-------------|
| Puerto **"D"** (SWD) | Conector **"DEBUG"** JST-SH |
| Puerto **"U"** RX | Pin **TX** (GP0/UART0_TX) |
| Puerto **"U"** TX | Pin **RX** (GP1/UART0_RX) |
| Puerto **"U"** GND | Pin **GND** |

> **Nota:** Conectar **dos cables USB**: uno al Debug Probe y otro al Pico (el Debug Probe **no alimenta** al Pico).

### 3.2 Debug Probe oficial → Pico / Pico W / Pico 2 (sin header JST-SH)

Requiere **soldar header macho** en pines: `SWCLK`, `GND`, `SWDIO`

| Debug Probe "D" | Pico / Pico W / Pico 2 |
|-----------------|------------------------|
| **SC** (SWCLK)  | **SWCLK**              |
| **GND**         | **GND**                |
| **SD** (SWDIO)  | **SWDIO**              |

Para UART (opcional), usar el cable JST-SH hembra a header 0.1" incluido:
| Debug Probe "U" | Pico |
|-----------------|------|
| RX              | GP0 (UART0_TX) |
| TX              | GP1 (UART0_RX) |
| GND             | GND |

### 3.3 Pico A (debugger) → Pico B (target) con firmware `debugprobe`

**Conexiones mínimas (SWD):**
```
Pico A GND   → Pico B GND
Pico A GP2   → Pico B SWCLK
Pico A GP3   → Pico B SWDIO
```

**Conexiones UART (opcional, para consola serial):**
```
Pico A GP4 (UART1 TX) → Pico B GP1 (UART0 RX)
Pico A GP5 (UART1 RX) → Pico B GP0 (UART0 TX)
```

**Alimentación compartida (opcional):**
- Modo device USB o sin USB: `VSYS` → `VSYS`
- Modo USB Host: `VBUS` → `VBUS` (provee 5V en conector USB)

---

## 4. Instalación del Firmware debugprobe (Opción B)

1. **Descargar** UF2 desde [GitHub debugprobe releases](https://github.com/raspberrypi/debugprobe/releases)
   - `debugprobe_on_pico.uf2` para Pico original
   - `debugprobe_on_pico2.uf2` para Pico 2
2. **Poner Pico A en modo BOOTSEL**: Mantener botón BOOTSEL presionado al conectar USB
3. **Arrastrar/copiar** el archivo `.uf2` a la unidad montada (RPI-RP2)
4. El Pico se reiniciará automáticamente como adaptador de depuración

---

## 5. Depuración con VS Code (Método Recomendado)

### 5.1 Configuración Inicial
1. Instalar extensión **"Raspberry Pi Pico"** desde VS Code Marketplace
2. Crear proyecto: Sidebar Pico → "New Project from Examples" → `blink`
3. Compilar: Botón "Compile Project" en sidebar o status bar

### 5.2 Iniciar Depuración
1. Conectar Debug Probe (o Pico con debugprobe) al PC vía USB
2. Alimentar Pico target por separado (cable USB independiente)
3. Sidebar Pico → **"Debug Project"** o tecla **F5**
4. Si se solicita, seleccionar **"Pico Debug (Cortex-Debug)"**

### 5.3 Flujo de Depuración en VS Code

| Acción | Tecla / Botón |
|--------|---------------|
| **Continuar** (ejecutar hasta siguiente breakpoint) | **F5** |
| **Paso sobre** (step over) | **F10** |
| **Paso dentro** (step into) | **F11** |
| **Paso fuera** (step out) | **Shift+F11** |
| **Reiniciar** (volver a main) | **Ctrl+Shift+F5** |
| **Toggle Breakpoint** | **F9** (en línea de código) |

**Paneles útiles durante depuración:**
- **Variables** - Inspeccionar variables locales (ej. `rc = PICO_OK`)
- **Watch** - Expresiones personalizadas
- **Call Stack** - Pila de llamadas
- **Breakpoints** - Lista de puntos de interrupción
- **Registers** - Registros del procesador (Cortex-M)

---

## 6. Depuración con OpenOCD + GDB (Línea de Comandos)

### 6.1 Instalar OpenOCD
```bash
# Opción 1: Binarios precompilados (recomendado)
# Descargar desde: https://github.com/raspberrypi/openocd/releases (branch sdk-2.2.0)

# Opción 2: Compilar desde fuente
git clone https://github.com/raspberrypi/openocd.git --branch sdk-2.2.0
cd openocd
./bootstrap
./configure --disable-werror
make -j4
sudo make install
```

### 6.2 Cargar Binario vía OpenOCD (Flash directo)

**Para RP2040 (Pico, Pico W, Pico H, Pico WH):**
```bash
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
  -c "adapter speed 5000" \
  -c "program blink.elf verify reset exit"
```

**Para RP2350 (Pico 2):**
```bash
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg \
  -c "adapter speed 5000" \
  -c "program blink.elf verify reset exit"
```

### 6.3 Depuración Interactiva (Servidor OpenOCD + GDB)

**Terminal 1 - Servidor OpenOCD:**
```bash
# RP2040
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000"

# RP2350 (Pico 2)
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000"
```

**Terminal 2 - Cliente GDB:**
```bash
# Linux (x86_64)
gdb-multiarch blink.elf

# Linux (ARM/Raspberry Pi OS) / Windows
gdb blink.elf

# macOS ARM
lldb blink.elf
```

**Comandos GDB:**
```gdb
(gdb) target remote localhost:3333
(gdb) monitor reset init
(gdb) load
(gdb) continue
```

> **Tip:** Compilar con `-DCMAKE_BUILD_TYPE=Debug` para mejor experiencia:
> ```bash
> cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DPICO_BOARD=pico
> cmake --build build
> ```

### 6.4 Puerto de Rescate (Rescue Debug Port)

Útil cuando el firmware bloquea el acceso SWD normal (ej. deshabilitó reloj del sistema):

**RP2040:**
```bash
# 1. Activar Rescue DP
openocd -f interface/cmsis-dap.cfg -f target/rp2040-rescue.cfg

# 2. En otra terminal, OpenOCD normal
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg
```

**RP2350:**
```bash
openocd -f interface/cmsis-dap.cfg -f target/rp2350-rescue.cfg
```

> **Advertencia RP2350:** Las flags OTP `CRIT1.DEBUG_DISABLE` y `CRIT1.SECURE_DEBUG_DISABLE` **no** pueden ser anuladas por Rescue DP.

---

## 7. Consola Serial (UART) via Debug Probe

El Debug Probe / debugprobe expone un **CDC UART class-compliant** (funciona sin drivers en Windows).

### 7.1 Linux
```bash
sudo minicom -D /dev/ttyACM0 -b 115200
```

### 7.2 Windows
1. **Administrador de dispositivos** → Identificar puerto COM (ej. `COM7`)
2. **PuTTY** → Connection type: **Serial** → Serial line: `COM7` → Speed: `115200` → **Open**

### 7.3 macOS
```bash
brew install minicom
minicom -D /dev/tty.usbmodemXXXXXX -b 115200
```

### 7.4 VS Code Serial Monitor
1. **View** → **Terminal** → Pestaña **"Serial Monitor"**
2. Seleccionar puerto serial → Baud rate: **115200** → **Start Monitoring**

---

## 8. Configuración STDIO en Proyectos

Al crear proyecto en VS Code (sección "STDIO support"):

| Opción | Descripción | Requiere Cableado |
|--------|-------------|-------------------|
| **UART** | Serial por pines GP0/GP1 | Sí (Debug Probe UART o conversor USB-UART) |
| **USB CDC** | Serial por USB nativo | No (cable USB directo al PC) |
| **Ambos** | Dual console | Solo para UART |

**Nota:** USB CDC puede perder salida inicial (1-2 seg para enumerar USB tras reset). UART captura todo desde el boot.

---

## 9. Archivos de Compilación Generados

| Extensión | Descripción |
|-----------|-------------|
| `.uf2` | Formato para drag-and-drop en modo BOOTSEL |
| `.elf` | Binario completo con información de debug (para GDB) |
| `.dis` | Desensamblado |
| `.hex` | Hex dump |
| `.map` | Mapa de memoria del linker |

**Para binario en SRAM (sin flash):**
```bash
cmake -DPICO_NO_FLASH=1 ...
# O en CMakeLists.txt:
pico_set_binary_type(TARGET_NAME no_flash)
```

---

## 10. Referencias y Recursos

- **Extensión VS Code:** https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico
- **Código fuente extensión:** https://github.com/raspberrypi/pico-vscode
- **Ejemplos oficiales:** https://github.com/raspberrypi/pico-examples
- **Debug Probe docs:** https://github.com/raspberrypi/debugprobe
- **OpenOCD para Pico:** https://github.com/raspberrypi/openocd (branch sdk-2.2.0)
- **picotool:** https://github.com/raspberrypi/picotool
- **SDK C/C++:** https://github.com/raspberrypi/pico-sdk
- **Documentación oficial:** https://www.raspberrypi.com/documentation/microcontrollers/

---

## 11. Resumen Rápido (Cheat Sheet)

```bash
# 1. Compilar proyecto (ejemplo blink)
cd ~/pico/pico-examples
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DPICO_BOARD=pico
cmake --build build

# 2. Flashear via OpenOCD (RP2040)
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
  -c "adapter speed 5000" -c "program build/blink.elf verify reset exit"

# 3. Depurar interactivo
# Terminal 1:
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000"
# Terminal 2:
gdb-multiarch build/blink.elf
# (gdb) target remote localhost:3333
# (gdb) monitor reset init
# (gdb) load
# (gdb) break main
# (gdb) continue

# 4. Consola serial
sudo minicom -D /dev/ttyACM0 -b 115200
```

---

*Documento generado automáticamente desde la guía oficial RP-008276-DS-2 (Julio 2026)*