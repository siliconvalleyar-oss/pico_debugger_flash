# INFO.md — puente `keyboard` (pico_keyboard_bridge)

## Qué es
Firmware para **Raspberry Pi Pico W** que actúa de puente:
- **Entrada BLE:** un "doorbell" (el GATT `doorbell.gatt` del SDK, 106 B, 1:1 con
  `pico-examples/bluetooth/ble_doorbell`) que recibe texto por la característica
  FFE0/FFE1 estilo Smart button.
- **Salida USB HID:** convierte el texto recibido en pulsaciones de teclado de
  clase HID (TinyUSB `tinyusb_device`), de modo que la Pico W funciona como un
  teclado USB inalámbrico controlado por BLE.

## Árbol del código
- `src/doorbell.gatt` — GATT (FFE0/FFE1), idéntico (Verified) al del
  `ble_doorbell` del SDK.
- `src/ascii_to_hid.cpp` + `ascii_to_hid.h` — tabla ASCII→HID (95 caracteres
  imprimibles, 2987 B, Verified) y rutina de conversión con manejo de SHIFT/uso.
- `src/ascii_hid.h` / `src/usb_kbd.[ch]` — envío HID por TinyUSB (USB HID 6KRO).
- `src/ble_server.[ch]` — servidor BLE (BTstack) que recibe el texto y dispara
  la escritura HID.
- `src/main.c` — punto de entrada: arranca BTstack, espera característica doorbell,
  envía por HID cada carácter ASCII recibido.
- `config/` — `btstack_config.h` (106 B) + `btstack_config_common.h` (2876 B),
  copiados del SDK (Verified).
- `CMakeLists` de `src/` (doorbell.gatt + doorbell.h Verified doorbell 1:1):
  `target_link_libraries` **con `pico_cyw43_arch_none` en el add_executable**
  (igual que el doorbell Verified; por eso `pico/cyw43_arch.h` llega a main.c).

## Dependencias (la que faltaba: NO era apt)
- **No** hace falta `ninja` ni paquetes adicionales: ninja ya está instalado.
- **La dependencia REAL que faltaba era `scripts/build.sh`** — `flash_nosudo_multi.sh`
  lo invocaba en su línea ~135 y **no existía en disco**. Se creó y hereda
  `config.sh` (`PROJECT=keyboard`, `keyboard/`).
- `config.sh` → `PROJECT=keyboard`, dir `keyboard/`, target CMake
  `pico_keyboard_bridge`, heredado por los 4 `scripts/flash_nosudo*.sh`.
- Headers BTstack: `pico/cyw43_arch.h` viene de enlazar
  `pico_cyw43_arch_none` en el **executable** (no en lib estática), como el
  `ble_doorbell` del SDK.

## Estado de la build (honesto)
Hasta el cierre de esta sesión, la build del puente **no** había alcanzado
`[100%] Built target pico_keyboard_bridge` ni generado ELF. **Regla de la rama:
no se flashea sin ELF (`>150 KB`) + `[100%]`** — el firmware pendiente de build.
La rama `keyboard` (push a `origin/keyboard`) contiene: doorbell.gatt + doorbell.h
(Verified 1:1), ascii_to_hid (95 chars Verified), scripts/build.sh + config.sh→keyboard.

## Flasheo
Después de `[100%] Built target` con ELF:
- SWD local (probe `2e8a:000c`, Pico W en disco) vía `scripts/flash_nosudo_multi.sh keyboard/`.
- Solo si el ELF existe y `>150 KB`.
