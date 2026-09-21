# Pico BLE-to-USB Keyboard Bridge

## Tabla de contenidos
- [Qué es esto](#qué-es-esto)
- [Por qué NO es "cualquier PC, cualquier dispositivo"](#seguridad)
- [Hardware necesario](#hardware-necesario)
- [Compilación](#compilación)
- [Uso](#uso)
- [Estructura del proyecto](#estructura-del-proyecto)
- [Errores comunes](#errores-comunes)
- [Documentación adicional](#documentación-adicional)

## Qué es esto

Firmware para **Raspberry Pi Pico W** que resuelve un problema concreto:
tenés una Raspberry Pi (u otro equipo) sin teclado físico conectado, y
querés poder tipear en ella usando tu **teléfono como control remoto**,
sin fabricar un teclado matricial.

La Pico W:
1. Se conecta por **cable USB** a la Raspberry Pi (o cualquier PC) y se
   enumera como **teclado HID estándar**.
2. Expone un **servicio BLE GATT** llamado `Pico-KB-Bridge`.
3. Tu teléfono (con una app BLE genérica tipo *nRF Connect* o *BLE
   Terminal*) se **empareja** una vez con la Pico y le manda texto.
4. Ese texto se reenvía por USB como pulsaciones de teclado reales.

```
┌────────────┐   BLE (bonded)   ┌──────────────┐   USB HID    ┌────────────────┐
│  Teléfono  │ ───────────────▶ │   Pico W     │ ───────────▶ │ Raspberry Pi /  │
│ (BLE app)  │                  │  (firmware)  │              │  PC destino     │
└────────────┘                  └──────────────┘              └────────────────┘
```

## Seguridad

Este proyecto usa **bonding BLE obligatorio** ("Just Works" + LE Secure
Connections). Eso significa:

- La primera vez que tu teléfono se conecta, se hace un emparejamiento
  (pairing) que queda guardado tanto en la Pico como en el teléfono.
- **Dispositivos que no hicieron ese primer pairing NO pueden escribir**
  en la característica BLE: BTstack rechaza la escritura si el enlace
  no está encriptado y bonded.
- El LED onboard de la Pico W se enciende solo cuando hay una conexión
  BLE bonded y encriptada activa, y se apaga al desconectar.

Esto **no** es una herramienta pensada para conectarse a "cualquier PC"
de forma encubierta: el USB solo enumera como teclado ante el host al
que decidas conectarla físicamente (tu propia Raspberry Pi), y el canal
BLE de control solo acepta al dispositivo que vos mismo emparejaste.

> Si necesitás que varios teléfonos/PCs distintos puedan controlarla,
> tenés que repetir el proceso de pairing con cada uno explícitamente;
> el firmware no acepta conexiones BLE anónimas por diseño.

## Hardware necesario

- 1x **Raspberry Pi Pico W** (necesita el chip CYW43439 para BLE nativo;
  una Pico normal sin radio **no puede** correr este proyecto tal cual).
- 1x cable USB micro-B a USB-A/C (según tu Raspberry Pi de destino).
- Un teléfono con Bluetooth LE y una app tipo *nRF Connect for Mobile*
  (Android/iOS) o *LightBlue*.

## Compilación

Requiere el [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
(que ya incluye TinyUSB y BTstack) y el toolchain `arm-none-eabi-gcc`.

**`PICO_BOARD=pico_w` es obligatorio**: el radio BLE vive en el chip
CYW43439 de la Pico W. Compilar para la placa genérica `pico` falla con
`fatal error: pico/cyw43_arch.h: No such file or directory`.

### Opción A: scripts del repo (compila y graba por SWD, sin sudo)

```bash
# Un solo paso: build + programación vía Debug Probe (CMSIS-DAP 2e8a:000c).
# IMPORTANTE: pasar BOARD=pico_w. El default de los scripts es BOARD=pico
# y ese flag sobreescribe el PICO_BOARD fijado en el CMakeLists.txt.
BOARD=pico_w ./scripts/flash_nosudo_multi.sh pico-ble-keyboard-bridge
```

El ELF/UF2 quedan en la raíz de `build/`:
`build/pico_ble_keyboard_bridge.{elf,uf2}`.

### Opción B: CMake manual

```bash
# 1. Exportar la ruta al SDK (ajustar a tu instalación)
export PICO_SDK_PATH=/ruta/a/pico-sdk

# 2. Configurar y compilar (siempre con pico_w)
mkdir build && cd build
cmake -DPICO_BOARD=pico_w ..
make -j4

# 3. El binario final queda en:
#    build/pico_ble_keyboard_bridge.uf2
```

### btstack_config.h (incluido en este repo)

BTstack **exige** un header `btstack_config.h` accesible por include path;
sin él la compilación falla con
`fatal error: btstack_config.h: No such file or directory`.

Este proyecto lo provee en `config/`:

- `config/btstack_config.h` — el header que BTstack incluye por nombre.
- `config/btstack_config_common.h` — las opciones reales: periférico LE,
  Secure Connections y flow-control HCI↔host para no saturar el bus
  compartido del CYW43. Derivado de las defaults del SDK
  (`pico-sdk/lib/btstack/src/btstack_config.h`), recortado a lo que este
  firmware usa (sin SCO/Classic/HID-host/stdin).

El `CMakeLists.txt` ya agrega `config/` a `target_include_directories`.
Si reusás este código en otro proyecto, copiá ambos archivos junto con
ese include dir.

> **Nota sobre el header GATT:** `pico_btstack_make_gatt_header` genera
> `pico_kb_bridge.h` (nombre del `.gatt` sin extensión + `.h`), *no*
> `pico_kb_bridge.gatt.h`. El código fuente incluye `pico_kb_bridge.h`.

## Uso

1. Mantené presionado el botón **BOOTSEL** de la Pico W y conectala por
   USB a tu computadora de desarrollo (no la Raspberry Pi destino todavía).
2. Copiá `pico_ble_keyboard_bridge.uf2` a la unidad `RPI-RP2` que aparece.
   La Pico se reinicia sola con el firmware nuevo.
3. Desconectala y conectala ahora al **puerto USB de la Raspberry Pi
   destino** (la que no tiene teclado).
4. En tu teléfono, abrí la app BLE, escaneá dispositivos y conectate a
   `Pico-KB-Bridge`. Confirmá el pairing si te lo pide.
5. Buscá la característica con UUID `0000FFE1-...` y mandale texto plano
   (Write / Write Without Response). El LED de la Pico se pone fijo
   cuando el bonding está activo.
6. Ejemplos de payloads a mandar desde la app BLE:
   ```
   hola mundo\ENTER
   ls -la\ENTER
   \SLEEP\TABsegunda linea\ENTER
   ```

## Estructura del proyecto

```
pico-ble-keyboard-bridge/
├── CMakeLists.txt
├── pico_sdk_import.cmake
├── config/
│   ├── btstack_config.h          # Header que exige BTstack (por nombre)
│   └── btstack_config_common.h   # Opciones del stack (LE peripheral)
├── gatt/
│   └── pico_kb_bridge.gatt      # Definición del servicio BLE
├── src/
│   ├── main.cpp                 # Loop principal
│   ├── usb_hid_keyboard.{h,cpp} # Emulación de teclado USB HID
│   ├── usb_descriptors.cpp      # Descriptores USB (device/config/HID)
│   ├── tusb_config.h            # Configuración de TinyUSB
│   ├── ascii_to_hid.{h,cpp}     # Tabla ASCII -> keycode HID (US QWERTY)
│   ├── key_queue.{h,cpp}        # Buffer circular de eventos de teclado
│   └── ble_gatt_server.{h,cpp}  # Servidor GATT BLE + bonding
└── docs/
    ├── ARCHITECTURE.md
    └── USAGE.md
```

## Errores comunes

| Síntoma | Causa probable | Solución |
|---|---|---|
| `PICO_SDK_PATH no está definido` al correr `cmake` | Variable de entorno no exportada | `export PICO_SDK_PATH=/ruta/al/sdk` antes de `cmake` |
| `btstack_config.h: No such file or directory` | Falta la config de BTstack o `config/` no está en los include dirs | Copiar `config/btstack_config*.h` y agregar `${CMAKE_CURRENT_LIST_DIR}/config` a `target_include_directories` |
| `pico/cyw43_arch.h: No such file or directory` | Compilado para placa `pico` (sin radio CYW43) | Compilar con `-DPICO_BOARD=pico_w`; con los scripts, `BOARD=pico_w` (el default es `pico`) |
| `pico_kb_bridge.gatt.h: No such file or directory` | Include viejo: el SDK genera `<nombre>.h`, no `<nombre>.gatt.h` | Incluir `pico_kb_bridge.h` (ver nota en [Compilación](#compilación)) |
| Build OK pero el script dice `... .elf no encontrado` | Target CMake heredado de otro proyecto o layout de salida distinto (`build/` vs `build/src/`) | `scripts/flash_nosudo_multi.sh` infiere el target del `add_executable`; actualizalo si usás tu propio script |
| La Pico no aparece como teclado en la Raspberry Pi | Firmware compilado para `pico` en vez de `pico_w`, o cable solo de carga (sin datos) | Verificar `-DPICO_BOARD=pico_w` y usar un cable USB de datos |
| El teléfono no ve `Pico-KB-Bridge` al escanear | BLE no llegó a `HCI_STATE_WORKING` (radio no inicializado) | Revisar log por UART (`pico_enable_stdio_uart`); reflashear |
| El teléfono se conecta pero la escritura falla / no pasa nada | El link no completó el bonding (pairing rechazado o interrumpido) | Olvidar el dispositivo en el teléfono y volver a emparejar desde cero |
| Los caracteres con tilde/ñ no se tipean | La tabla `ascii_to_hid` solo cubre el layout US QWERTY | Enviar el texto sin tildes, o extender la tabla para tu layout |
| El texto llega cortado | La cadena superó el buffer interno (`kMaxCmdLen` o `KeyQueue::kCapacity`) y se trocea | Mandar la cadena en paquetes BLE más chicos |

## Documentación adicional

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): diagramas de arquitectura y flujo de datos.
- [`docs/USAGE.md`](docs/USAGE.md): guía paso a paso de emparejamiento y comandos soportados.
