# Skills del Repositorio pico_debugger_flash

Este documento describe los **skills** (habilidades especializadas) disponibles en el repositorio para tareas específicas.

---

## Tabla de Contenidos

1. [flash-pico-swd](#1-flash-pico-swd) - Flashear Pico por SWD
2. [keyboard-oled-bridge](#2-keyboard-oled-bridge) - Puente BLE → USB HID

---

## 1. flash-pico-swd

**Archivo:** `.opencode/skills/flash-pico-swd/SKILL.md`

### Descripción
Programar/grabar firmware de Raspberry Pi Pico (RP2040/RP2350) por SWD usando un Pico como Debug Probe (CMSIS-DAP), **sin sudo**.

### Cuándo usar
- Tareas que mencionen: `flash`, `grabar`, `programar`, `flashear`, `SWD`, `debugprobe`, `debug probe`, `openocd`, `uf2`, `pico_w`, `RP2040`, `RP2350`
- Scripts del repo: `flash_nosudo_multi.sh`, `build.sh`, `config.sh`

### Uso Básico

```bash
# Target: Pico (RP2040)
./scripts/flash_nosudo_multi.sh <proyecto>

# Target: Pico W
BOARD=pico_w ./scripts/flash_nosudo_multi.sh <proyecto>

# Target: Pico 2 W
BOARD=pico2_w ./scripts/flash_nosudo_multi.sh <proyecto>

# Solo compilar
BOARD=pico ./scripts/build.sh <proyecto>
```

### Flujo de Build/Programación

```
config.sh ──▶ build.sh ──cmake+ninja──▶ build/<proyecto>/<target>.elf
    ▲                                                          │
    └── heredado por flash_nosudo_multi.sh ──▶ openocd ──SWD──▶ target
```

### Componentes Clave

| Script | Función |
|--------|---------|
| `config.sh` | Detecta `PICO_SDK_PATH`, `OPENOCD_BIN`, `OPENOCD_SCRIPTS`, `HIDAPI_LIB` |
| `build.sh` | cmake + ninja; genera ELF/UF2 en `build/src/<target>` |
| `flash_nosudo_multi.sh` | Compila y programa por SWD **sin sudo** |

### Firmware de la Sonda (CRÍTICO)

| Variante | Firmware | Pines SWD | iProduct |
|----------|----------|-----------|----------|
| Debug Probe oficial | `debugprobe.uf2` | SWCLK=GP12, SWDI=GP13, SWDIO=GP14 | `Debug Probe (CMSIS-DAP)` |
| **Pico normal** ✅ | `debugprobe_on_pico.uf2` | SWCLK=GP2, SWDIO=GP3, nRESET=GP1 | `Debugprobe on Pico (CMSIS-DAP)` |

> **Regla de oro:** Si la sonda es un Pico normal → usar `debugprobe_on_pico.uf2` (bandera `DEBUG_ON_PICO=ON`). El firmware oficial usa GP12/13/14 y **nunca conecta** con cableado estándar GP2/GP3.

### Cableado (solo con `debugprobe_on_pico.uf2`)

```
Sonda GP2  → Target GP2  (SWDIO)
Sonda GP3  → Target GP3  (SWCLK)
Sonda GND  → Target GND
Sonda 3V3  → Target 3V3 (opcional)
```

**Reglas:**
1. Target **NO** en modo BOOTSEL (SWD deshabilitado) — desconectar USB del target
2. Target alimentado y GND compartido con la sonda

### Sin sudo: Regla udev

```udev
# /etc/udev/rules.d/99-pico-debugprobe.rules
SUBSYSTEM=="usb", ATTR{idVendor}=="2e8a", MODE="0666", GROUP="plugdev"
```

Instalar: `sudo udevadm control --reload-rules && sudo udevadm trigger`
→ **Desconectar/reconectar la sonda** (las reglas no son retroactivas)

### Verificación de Conexión

```bash
lsusb | grep 2e8a
# 2e8a:000c CMSIS-DAP
```

### Señales de Éxito OpenOCD

```
Info : SWD DPIDR 0x0bc12477, DLPIDR 0x00000001
** Programming Started ** / ** Verify Started ** / ** Verified OK **
```

### Solución de Problemas

| Síntoma | Causa | Arreglo |
|---------|-------|---------|
| `Failed to connect multidrop rp2040.dap0` | Firmware sonda pines equivocados / target sin alimentar / GND suelto / BOOTSEL | Ver iProduct, cableado GP2/GP3, quitar USB target |
| Sonda visible pero no conecta | Mismatch pinout firmware vs cableado | Flashear `debugprobe_on_pico.uf2` |
| `libhidapi-hidraw.so: cannot open` | hidapi en prefijo local | `HIDAPI_LIB=.../hidapi-install/lib` (config.sh lo hace) |
| `Permission denied` | udev no aplicada | Instalar regla y reconectar sonda |
| Build OK pero LED no prende | `BOARD` equivocado (Pico W: LED por CYW43) | `BOARD=pico_w` y `rm -rf build/` |

### Trampas Conocidas en este Repo

- `keyboard_oled` declara target en `src/CMakeLists.txt` (no raíz) — `flash_nosudo_multi.sh` ya maneja esto
- Cambiar `BOARD` → **limpiar `build/`** (`rm -rf build`) para evitar caché CMake

---

## 2. keyboard-oled-bridge

**Archivo:** `.opencode/skills/keyboard-oled-bridge/SKILL.md`

### Descripción
Compilar y flashear el puente **BLE → USB HID** `keyboard_oled` (target `pico_keyboard_bridge`) para **Pico W**.

Recibe texto por **BLE** (GATT doorbell, FFE0/FFE1) y lo escribe como teclado **USB HID** (TinyUSB boot keyboard 6KRO).

### Cuándo usar
- Tareas que mencionen: `keyboard_oled`, `pico_keyboard_bridge`, `teclado BLE`, `keyboard bridge`, `doorbell`, `portapapeles->teclado`, `BLE->USB HID`
- Archivos: `ascii_to_hid.cpp`, `ble_server.c`, `usb_kbd.c`, `doorbell.gatt`

### Uso

```bash
# Build + flash SWD (sin sudo)
BOARD=pico_w ./scripts/flash_nosudo_multi.sh keyboard_oled

# Solo build
BOARD=pico_w ./scripts/build.sh
```

> **Requisito:** ELF generado por `[100%] Built target pico_keyboard_bridge` + sonda `2e8a:000c` conectada

### Arquitectura

| Archivo | Rol |
|---------|-----|
| `src/main.c` | Init CYW43 + BTstack + TinyUSB; bucle BLE→HID |
| `src/ble_server.[ch]` | BTstack: GATT doorbell, recibe texto por FFE0/FFE1 en ring buffer 64B |
| `src/ascii_to_hid.cpp` | Tabla ASCII (0x20..0x7E) → HID usage + shift |
| `src/usb_kbd.[ch]` | Descriptores TinyUSB + boot keyboard |
| `src/doorbell.gatt` | GATT idéntico al `ble_doorbell` del Pico SDK |
| `src/CMakeLists.txt` | Un único `add_executable(pico_keyboard_bridge)` |

### Gotchas (Lecciones Pagadas)

1. **`add_executable` en `src/CMakeLists.txt`** — la raíz solo hace `add_subdirectory(src)`
2. **`pico_cyw43_arch_none` en el executable** — NO en lib STATIC aparte; si no, `pico/cyw43_arch.h` no llega a `main.c`
3. **GATT generado con `pico_btstack_make_gatt_header`** — `.gatt` en misma dir que CMakeLists; si falla `doorbell.h`, copiar 1:1 del `ble_doorbell` del SDK
4. **OLED SSD1306 integrado** — I2C0 SDA=GP4, SCL=GP5, addr 0x3C (sin conflicto SWD GP2/GP3). MAC BLE mostrado en OLED
5. **README.md y docs/BUILD_INSTRUCTIONS.md DESACTUALIZADOS** — describen A2DP, no este proyecto. Doc fiel: `docs/INFO.md` y `docs/SKILL.md`

### Solución de Problemas

| Error | Causa | Fix |
|-------|-------|-----|
| `pico/cyw43_arch.h: No such file` | Falta link en executable | Gotcha 2: `target_link_libraries(bridge pico_cyw43_arch_none)` |
| Error `doorbell.h` | GATT no generado | Gotcha 3: `pico_btstack_make_gatt_header` + copiar de SDK |
| No conecta SWD | Sonda/target | Ver skill `flash-pico-swd` |

### Referencias

- `keyboard_oled/docs/INFO.md`, `keyboard_oled/docs/SKILL.md`
- `keyboard_oled/src/CMakeLists.txt`
- Ejemplo SDK: `pico-examples/bluetooth/ble_doorbell`

---

## Resumen Rápido

| Skill | Target | Hardware | Comando Principal |
|-------|--------|----------|-------------------|
| `flash-pico-swd` | Cualquier Pico (RP2040/2350) | Pico sonda + target | `./scripts/flash_nosudo_multi.sh <proj>` |
| `keyboard-oled-bridge` | Pico W | Pico W + OLED + BLE | `BOARD=pico_w ./scripts/flash_nosudo_multi.sh keyboard_oled` |

---

## Notas Generales

- **Scripts compartidos:** Ambos skills usan `scripts/config.sh`, `build.sh`, `flash_nosudo_multi.sh`
- **Regla udev:** Necesaria para flash sin sudo (ver skill 1)
- **Sonda:** Pico con `debugprobe_on_pico.uf2` para cableado GP2/GP3 estándar
- **Limpieza:** Al cambiar `BOARD` → `rm -rf build/`

---

*Documentación generada para el proyecto `debbug_serial_floppy` — Herramienta de depuración serie USB para Raspberry Pi Pico*