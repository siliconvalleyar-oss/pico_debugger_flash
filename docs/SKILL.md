# SKILL · Programar Raspberry Pi Pico por SWD con un Pico como Debug Probe

**Skill consolidada** del proyecto `pico_debugger_flash`.
Nivel: apto para personas y para inteligencias artificiales (receta reproducible).

**Resultado final (lema del skill):**

```bash
./scripts/flash_nosudo.sh               # Pico 1   (LED GPIO25)
BOARD=pico_w ./scripts/flash_nosudo.sh  # Pico W   (LED via CYW43)
```

---

## 0. Objetivo del skill

Entender por qué, en hardware real, "flashear" y "que funcione" son dos cosas
distintas, y dominar el flujo completo:

1. Un **Pico se convierte en sonda de depuración (Debug Probe)** con firmware CMSIS-DAP.
2. La sonda se conecta a **otro Pico (target)** por 4 cables (SWD + GND + 3V3).
3. OpenOCD (compilado con soporte CMSIS-DAP + hidapi) programa el target.
4. Una **regla udev** permite programar **sin sudo**.
5. El firmware debe compilarse para la placa correcta (`BOARD=pico|pico_w`), porque
   **cada placa tiene LEDs y pines distintos** (ej.: Pico W usa el LED del chip WiFi).

---

## 1. Comandos claves (cheat sheet)

| Tarea | Comando |
|---|---|
| Compilar + programar **Pico 1** (sin sudo) | `./scripts/flash_nosudo.sh` |
| Compilar + programar **Pico W** (sin sudo) | `BOARD=pico_w ./scripts/flash_nosudo.sh` |
| Compilar + programar **Pico 2 W** (sin sudo) | `BOARD=pico2_w ./scripts/flash_nosudo.sh` |
| Solo compilar | `BOARD=<board> ./scripts/build.sh` |
| Compilar + programar (con sudo) | `./scripts/build_and_program.sh` |
| Programar un build ya hecho (con sudo) | `./scripts/program.sh` |
| Modo RESCUE (target colgado) | `./scripts/flash_rescue.sh [proyecto]` (rescue + reprogramar; `--rescue-only` para solo rescue) |
| Instalar regla udev (una vez, con sudo) | `sudo ./scripts/install_udev.sh` |
| Ver qué firmware tiene la sonda | `lsusb -v -d 2e8a:000c \| grep iProduct` |
| Ver salida serial del target | `minicom -D /dev/ttyACM1 -b 115200` (con USB del target) o `/dev/ttyACM0` (UART del Debug Probe, sin cable extra — §8.5) |
| Estado BTstack por SWD (advertising) | `ptype /o hci_stack_t` en gdb batch + `mdb` por OpenOCD — ver §8.4 |

> Todos los scripts detectan automáticamente `PICO_SDK_PATH`, `OPENOCD_BIN`,
> `OPENOCD_SCRIPTS` y `HIDAPI_LIB`. Cualquiera se sobre-escribe con variables de
> entorno, p. ej.:
> `PICO_SDK_PATH=/x OPENOCD_BIN=/x/openocd BOARD=pico_w ./scripts/flash_nosudo.sh`

---

## 2. Arquitectura del sistema

```
[PC Linux]  --USB-->  [Pico sonda: debugprobe_on_pico.uf2]  --SWD+3V3+GND-->  [Pico target]
   │                          │                                                     │
   └─ OpenOCD (CMSIS-DAP)     └─ chip RP2040 corre el firmware "Debug Probe"         └─ recibe blink.elf
      + libhidapi                  que habla CMSIS-DAP por USB                        por SWD y lo ejecuta
                                  y SWD por GPIO2 (SWDIO) / GPIO3 (SWCLK)
```

| Componente | Rol | Ruta típica del proyecto |
|---|---|---|
| Pico sonda | Habla CMSIS-DAP por USB, SWD por GPIO | conectado al PC por USB |
| Pico target | Es programado por SWD | conectado a la sonda por 4 cables |
| Pico SDK | Toolchain de compilación | `../pico-sdk` (variable `PICO_SDK_PATH`) |
| OpenOCD | Cliente CMSIS-DAP + protocolo SWD | `../openocd-src/src/openocd` |
| hidapi | Librería HID requerida por CMSIS-DAP | `../hidapi-install/lib` |
| Config OpenOCD | Driver + definición del target RP2040 | `debugprobe-openocd.cfg` |

### Firmwares de la sonda

| Archivo | Para qué hardware | Pines SWD | String USB |
|---|---|---|---|
| `debugprobe.uf2` | **Debug Probe oficial** (accesorio RPi) | SWCLK=GP12, SWDI=GP13, SWDIO=GP14 | `Debug Probe (CMSIS-DAP)` |
| `debugprobe_on_pico.uf2` | **Pico / Pico 2 normal** ✅ | SWCLK=GP2, SWDIO=GP3, RESET=GP1 | `Debugprobe on Pico (CMSIS-DAP)` |

> **Regla de oro:** si la sonda es un Pico, usar `debugprobe_on_pico.uf2`.
> `debugprobe.uf2` en un Pico normal **NO funciona**: los pines GPIO12/13/14 no
> están cableados hacia el target → OpenOCD ve la sonda por USB pero el target
> nunca responde (`Failed to connect multidrop rp2040.dap0`).

### Cableado (sonda → target) — solo con `debugprobe_on_pico.uf2`

| Sonda | Target | Señal |
|---|---|---|
| GP2 (Pin 4) | GP2 (Pin 4) | SWDIO |
| GP3 (Pin 5) | GP3 (Pin 5) | SWCLK |
| GND | GND | Referencia común |
| 3V3 (Pin 36) | 3V3 (Pin 36) | Alimentación del target (opcional) |

> - El target **NO** debe estar en modo BOOTSEL (USB mostrando `RPI-RP2`).
> - Sonda y target deben **compartir GND** y el target debe estar **alimentado**.
> - Para el Pico W (sin header debug), el punto exacto de conexión son los
>   **pads inferiores SWCLK/SWDIO/GND** (los 3 agujeros de debug).

---

## 3. Secuencia decisión-flujo (cómo pensar el fallo)

```
¿La sonda aparece en USB?  (lsusb → 2e8a:000c)
   ├─ NO  → ¿firmware correcto? ¿cable USB? ¿puerto?
   │
   └─ SÍ  → ¿qué iProduct reporta?
        ├─ "Debug Probe (CMSIS-DAP)"        → firmare de placa oficial → pines 12/13/14 → revisar hardware o flashear on_pico
        └─ "Debugprobe on Pico (CMSIS-DAP)" → ✅ firmware correcto
              ↓
        OpenOCD: ¿conecta el DAP? (SWD DPIDR 0x0bc12477)
           ├─ SÍ → Programming Started → Verify → OK ✅
           └─ NO → "Failed to connect multidrop rp2040.dap0"
                  → target no alimentado / BOOTSEL / cableado / pines equivocados
```

### Señales de éxito (output de OpenOCD)

```
Info : SWD DPIDR 0x0bc12477, DLPIDR 0x00000001
Info : [rp2040.core0] Cortex-M0+ r0p1 processor detected
** Programming Started **
** Verify Started **
** Verified OK **
** Resetting Target **
```

---

## 4. Diferencias de placa que rompen el firmware (lección clave)

| Placa | LED onboard | GPIO del LED | ¿Qué pasa si uso la compilación equivocada? |
|---|---|---|---|
| Pico 1 / Pico 2 | LED PWM directo | `PICO_DEFAULT_LED_PIN = 25` | En Pico 1 sin LED físico, GPIO25 queda "flotante", sin efecto visible |
| **Pico W** | LED del chip WiFi CYW43439 | `CYW43_WL_GPIO_LED_PIN = 0` (por SPI) | En Pico W, GPIO25 = **CS SPI del WiFi**, no es el LED → nada parpadea |

**Por qué el Pico W es traicionero:** el LED del Pico W no es un GPIO directo.
Está dentro del chip de WiFi (CYW43439) y hay que hablarle por SPI a través del
driver `cyw43` del SDK. Compilar para otro `BOARD` **programa bien y verifica bien**
pero el LED no prende — exactamente lo que le pasó en la sesión real.

**Solución en código** (`blink/blink.c`): autodetección en tiempo de compilación.

```c
#ifdef CYW43_WL_GPIO_LED_PIN
    #include "pico/cyw43_arch.h"
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);   // Pico W
#elif defined(PICO_DEFAULT_LED_PIN)
    gpio_put(PICO_DEFAULT_LED_PIN, on);               // Pico 1
#endif
```

**Solución en build** (`blink/CMakeLists.txt`):

```cmake
if (PICO_CYW43_SUPPORTED)
    target_link_libraries(blink pico_cyw43_arch_none)   # driver mínimo del chip WiFi
endif()
```

`pico_cyw43_arch_none` inicializa el chip por SPI (para el LED) **sin** incluir la
pila de red lwIP → binario pequeño.

**Atención con el caché de CMake:** al cambiar de `BOARD`, limpiar `build/`:

```bash
rm -rf build && BOARD=pico_w ./scripts/build.sh
```

> Toolkit para validar: `cmake -DPICO_BOARD=<board> ...` deja `PICO_BOARD:STRING=<board>`
> en `build/CMakeCache.txt`; comprobarlo evita flashear la placa equivocada.

---

## 5. Sin sudo: regla udev

El nodo USB sale con permisos `root:root`. Para no usar sudo en programación:

```udev
# /etc/udev/rules.d/99-pico-debugprobe.rules
SUBSYSTEM=="usb", ATTR{idVendor}=="2e8a", MODE="0666", GROUP="plugdev"
```

Instalar (una vez, con sudo):

```bash
sudo ./scripts/install_udev.sh
# luego DESCONECTAR y RECONECTAR la sonda (las reglas no se aplican retroactivamente)
```

Verificar:

```bash
lsusb | grep 2e8a
ls -la /dev/bus/usb/$(lsusb | grep 2e8a | awk '{print $2}')/$(lsusb | grep 2e8a | awk '{print $4}' | tr -d ':')
# debe verse rw-rw-rw- (o grupo plugdev)
```

---

## 6. Preguntas frecuentes (FAQ)

**P: Se compila y verifica OK pero mi LED no parpadea.**
R: 1) Revisa el `BOARD` usado (`grep PICO_BOARD build/CMakeCache.txt`). 2) En Pico W el
LED no es GPIO25, usa `BOARD=pico_w`. 3) Chequea el cableado y que el target esté alimentado.

**P: La sonda aparece en USB pero dice "Failed to connect multidrop rp2040.dap0".**
R: Verificar en orden: (a) `iProduct` = `Debugprobe on Pico` (firmware correcto);
(b) target alimentado; (c) target fuera de BOOTSEL; (d) GND compartido; (e) pines
GP2/GP3 → SWDIO/SWCLK correctos; (f) si cifra muy corta, bajar `adapter speed`.

**P: ¿Por qué el `debugprobe.uf2` "aparece" pero no conecta?**
R: Porque el USB usa los pines dedicados del RP2040 (iguales en ambos firmwares),
pero el SWD usa GPIO12/13/14 (placa oficial) en vez de GPIO2/GP3 (Pico). El cableado
a GPIO2/GP3 queda inactivo → el target no recibe reloj SWD.

**P: Verifico OK, ¿realmente se grabó en flash?**
R: Sí. `Verify` relee la flash por SWD tras `Program`. No es "ágil" ni provisional:
el target quedó programado hasta que se reescriba.

**P: ¿`pico_cyw43_arch_none` carga la pila de red?**
R: No. Es la variante mínima: solo inicializa el chip CYW43439 por SPI para controlar
el LED (o GPIO del chip). Si se quisiera WiFi se usaría `pico_cyw43_arch_lwip_*`.

**P: Sudo sigue pidiendo contraseña pese a la regla udev.**
R: La regla se aplica al conectar el dispositivo: **desconecta y vuelve a conectar**
la sonda; de lo contrario el nodo viejo conserva root:root.

---

## 7. Recursos

- Debug Probe oficial y firmware: https://github.com/raspberrypi/debugprobe
- OpenOCD (upstream): https://github.com/openocd-org/openocd
- Pico SDK: https://github.com/raspberrypi/pico-sdk
- Guía oficial "Getting Started with Raspberry Pi Pico-series" (RP-008276-DS-2)
- Documentos hermanos de este repo: `docs/DEBUGPROBE_LEARNINGS.md`,
  `docs/PICO_DEBUGGER_GUIDE.md`, `docs/REPORT.md`

---

## 8. Lecciones de la sesión 2026-09-21: build multi-proyecto, target/ELF, BTstack, BLE y debug UART

Lecciones pagadas con fallos reales al compilar y flashear dos proyectos
convivientes (`keyboard_oled` y `pico-ble-keyboard-bridge`) con la misma
cadena de scripts.

### 8.1 Un solo script, muchos proyectos: inferir target y layout, NUNCA heredarlos

- `config.sh` define `PROJECT_CMAKE_TARGET` para el `PROJECT` default. Si el
  script de flash acepta una **carpeta por argumento** (distinta del default),
  ese valor heredado **miente**: cada proyecto tiene su propio target
  (`keyboard_oled → pico_keyboard_bridge`,
  `pico-ble-keyboard-bridge → pico_ble_keyboard_bridge`).
- Inferir SIEMPRE del `add_executable` del `CMakeLists.txt` del proyecto
  seleccionado. Cuidado con dónde declara el target:
  - `pico-ble-keyboard-bridge` lo declara en la **raíz**.
  - `keyboard_oled` lo declara en **`src/CMakeLists.txt`** (vía
    `add_subdirectory(src)`) → el ELF sale en `build/src/`, no en `build/`.
- **Trampa `set -euo pipefail` + grep:** `VAR=$(grep ... | grep ...)` con
  cero matches devuelve exit 1 y **mata el script en silencio** (sin mensaje,
  EXIT=1). Los grep de inferencia van con `|| true`. Síntoma típico: el
  script imprime el banner y muere sin error aparente.
- Red de seguridad post-build: si el ELF esperado no existe, `find` en
  `build/` antes de declarar "Build failed".

### 8.2 BOARD del default vs BOARD del proyecto (pico_w obligatorio)

- Los scripts con `BOARD=${BOARD:-pico}` **sobreescriben** el `set(PICO_BOARD
  pico_w ...)` del CMakeLists del proyecto (el `-DPICO_BOARD=` de build.sh
  gana sobre el cache). Para Pico W: **siempre `BOARD=pico_w` explícito**.
- Síntoma de board equivocada: `fatal error: pico/cyw43_arch.h: No such file`
  (la placa genérica no trae el radio CYW43).

### 8.3 BTstack exige btstack_config.h y el header GATT se llama <nombre>.h

- `btstack_config.h` **no es opcional** y se incluye por nombre exacto. Si el
  proyecto no lo tiene: `fatal error: btstack_config.h: No such file`.
  Receta: copiar `config/btstack_config.h` + `btstack_config_common.h` de un
  proyecto conocido-bueno del mismo SDK (aquí: de `keyboard_oled`) y agregar
  `config/` a `target_include_directories`. Recortar opciones no usadas
  (sin SCO/Classic si el firmware es BLE-only).
- `pico_btstack_make_gatt_header` genera `<nombre-sin-ext>.h`
  (ej. `pico_kb_bridge.h`), **NO** `<nombre>.gatt.h`. Verificado compilando
  `btstack/tool/compile_gatt.py <archivo>.gatt <salida>.h` a mano: además
  confirma que el `.gatt` es válido y muestra los `*_HANDLE` reales que usa
  el código.

### 8.4 "El Pico no aparece en el scan BLE": verificar el firmware ANTES que el radio

Orden de diagnóstico que funcionó (todo por SWD, sin tocar nada físico):

1. ¿El firmware correcto está grabado? **Dos proyectos con el mismo banco
   se pisan**: el último `flash_nosudo_multi.sh <otro>` deja el firmware
   equivocado. Confirmar con el banner UART o el nombre del ELF flasheado
   en el log del script.
2. Estado BTstack leyendo RAM por OpenOCD (ELF con DWARF → `ptype /o` en
   gdb batch para offsets, sin conexión gdb remota):
   - `hci_stack_static.state == HCI_STATE_WORKING (2)`
   - `le_advertisements_enabled_for_current_roles == 1`
   - `le_advertisements_data_len == 19` y el buffer contiene
     `02 01 06 | 0f 09 "Pico-KB-Bridge"`
   - `le_advertisements_todo == 0` (sin tareas pendientes)
   - BD_ADDR no nula/unicast (`x x x x x CD`)
   → con eso, **el Pico está anunciando**: el problema está en el host.
3. El host de la sesión tenía un dongle **CSR HCI/LMP 2.0 sin LE**
   (`hciconfig hci0 version`; features octeto 6 sin bit LE): `hcitool lescan`
   da "Operation not permitted" y `gatttool connect` da
   "Operation not supported (95)". Un adaptador BR/EDR-only **jamás** verá
   el advertising LE. Probar con un teléfono (BLE) o dongle BT 4.0+.

### 8.5 Debug por UART a través del Debug Probe (CDC-ACM)

- El Debug Probe expone **dos interfaces USB**: CMSIS-DAP (SWD) y un
  **CDC-ACM `/dev/ttyACM0`** que puentea su header UART (3v3) — no hace
  falta un USB-TTL aparte.
- Cableado target: **GP0=TX, GP1=RX, GND** (cruzar con TX/RX de la sonda).
- Firmware: `PICO_DEFAULT_UART=0`, `PICO_DEFAULT_UART_TX_PIN=0`,
  `PICO_DEFAULT_UART_RX_PIN=1`, `PICO_DEFAULT_UART_BAUD_RATE=115200` +
  `pico_enable_stdio_uart(1)`. Detalle: `keyboard_oled` tenía
  `stdio_uart 0 / stdio_usb 1` — sin `stdio_uart 1` no hay logs.
- Captura sincronizada al reset: `cat /dev/ttyACM0` en background +
  OpenOCD `reset run` + `strings` del log.
- Trampa de shell: `pkill -f "cat /dev/ttyACM0"` se mata a sí mismo si el
  patrón está en la propia línea de comando (el `-f` matchea el cmdline del
  shell lanzador). Usar el PID capturado con `$!`.
- Cómo quedar los logs: boot + `[BLE] conexion LE entrante` +
  `[SM] JUST_WORKS_REQUEST` + `[SM] PAIRING_COMPLETE OK` + `[ATT] write en
  FFE1` — el ciclo BLE completo observable en vivo. Documento completo:
  `pico-ble-keyboard-bridge/docs/DEBUG_UART.md`.

### 8.6 Modo RESCUE oficial de OpenOCD (target "colgado")

- La implementación manual del `debugprobe-openocd-rescue.cfg` del repo
  estaba rota con OpenOCD moderno (`dap create -tap` sin tap creado →
  "-tap is invalid"). **Corregido (2026-09-21):** hoy es un wrapper fino
  que solo hace `set RESCUE 1` + `source [find target/rp2040.cfg]` (con
  `vid_pid` de la sonda), porque el rescue ya vive nativo en
  `target/rp2040.cfg`.
- El rescue nativo (`swd newdap` + `rescue_dap instance-id 0xf` + clear
  DBGPWRUPREQ → reset del PSM) responde `SWD DPIDR 0x10212927,
  DLPIDR 0xf0000001`, deja el core halted en bootrom y termina con
  `shutdown`. Cuando imprime "Now restart OpenOCD without RESCUE flag",
  reprogramar normal (`debugprobe-openocd.cfg` o `flash_nosudo_multi.sh`)
  — relevante si el firmware vivo rompe el SWD.
- Uso desde el repo: `./scripts/flash_rescue.sh [proyecto]` (creado el
  2026-09-21: fase 1 rescue con `CONFIG_RESCUE_FILE`, fase 2 delega en
  `flash_nosudo_multi.sh`; `--rescue-only` omite la fase 2). A mano:
  `source scripts/config.sh && "$OPENOCD_BIN" -s
  "$OPENOCD_SCRIPTS" -f "$CONFIG_RESCUE_FILE"`.
- Nota: el patrón roto original quedó como `test_rescue.cfg` (artefacto
  histórico citado en `docs/REPORT.md`); ningún script lo consume.

### 8.7 "El teléfono conecta pero nunca paira": falta el Security Request

- Síntoma real: el teléfono ve al periférico, conecta, pero el pairing no
  arranca (FFE1 inaccesible sin encriptación) — y ni `SM_AUTHREQ_BONDING`
  ni "Just Works" disparan nada por sí solos.
- Causa: **muchos teléfonos no inician el emparejamiento por su cuenta**
  al conectar; esperan que el periférico lo pida. Sin petición, quedan
  conectados sin bond para siempre.
- Fix (validado en el banco, 2026-09-21): al recibir
  `HCI_SUBEVENT_LE_CONNECTION_COMPLETE` llamar
  `sm_send_security_request(g_con_handle)` — el periférico pide pairing
  apenas conectan. En `ble_gatt_server.cpp` de `pico-ble-keyboard-bridge`.
- Acompañante: **no exigir `SM_AUTHREQ_SECURE_CONNECTION`** duro en
  `sm_set_authentication_requirements` — solo `SM_AUTHREQ_BONDING`. El SM
  de BTstack igual negocia Secure Connections si el peer lo soporta;
  exigirlo rechaza intentos que podrían completar (fallback legacy muerto).
- Diagnóstico cuando la UART calla (la CDC del probe pierde ráfagas):
  leer `g_con_handle` por SWD. `0xFFFF` (HCI_CON_HANDLE_INVALID) = sin
  conexión; **`0x0000` es un handle válido** (¡no es "sin conexión"!) —
  con conexión viva y sin eventos `[SM]` en el log, el pairing es el
  problema, no el link.
- Resultado verificado: `PAIRING_COMPLETE OK` (LED fijo) + write en FFE1
  tipeado por USB — la cadena completa end-to-end funcionando.