---
name: keyboard-oled-bridge
description: Compilar y flashear el puente BLE->USB HID keyboard_oled (target pico_keyboard_bridge) para Pico W. USAR cuando la tarea mencione keyboard_oled, pico_keyboard_bridge, teclado BLE, keyboard bridge, doorbell, portapapeles->teclado, BLE->USB HID o los archivos ascii_to_hid.cpp, ble_server.c, usb_kbd.c, doorbell.gatt de este repo.
---

# Skill · Puente BLE → USB HID: `keyboard_oled` (pico_keyboard_bridge)

Firmware para **Pico W** que recibe texto por **BLE** (GATT doorbell, FFE0/FFE1)
y lo escribe como teclado **USB HID** (TinyUSB boot keyboard 6KRO).

> Si la tarea es tocar/compilar/flashear este puente, carga este skill para no
> romper la cadena de build/flash que comparten `scripts/config.sh` y
> `scripts/build.sh`.

## Comandos (lo que manda)

```bash
BOARD=pico_w ./scripts/flash_nosudo_multi.sh keyboard_oled   # build + SWD flash, sin sudo
BOARD=pico_w ./scripts/build.sh                               # solo build
```

Requisito de flasheo (regla honesta del repo): ELF generado por `[100%] Built
target pico_keyboard_bridge` + sonda `2e8a:000c` conectada. **Jamás inventar un
`[100%]` ni flashear sin ELF.**

## Arquitectura del código

| Archivo | Rol |
|---|---|
| `src/main.c` | Init CYW43 + BTstack + TinyUSB; bucle: BLE→HID |
| `src/ble_server.[ch]` | BTstack: GATT doorbell, recibe texto por Digital Output (FFE0/FFE1) en un ring buffer de 64 B |
| `src/ascii_to_hid.cpp` | Tabla ASCII (0x20..0x7E) → HID usage + shift (`HidKey{usage, shift}`) |
| `src/usb_kbd.[ch]` | Descriptores TinyUSB + boot keyboard |
| `src/doorbell.gatt` | GATT idéntico (1:1 Verified) al `ble_doorbell` del Pico SDK |
| `src/CMakeLists.txt` | Un único `add_executable(pico_keyboard_bridge)` |

## Gotchas (lecciones pagadas)

1. **`add_executable` vive en `src/CMakeLists.txt`, no en la raíz.** La raíz solo
   hace `add_subdirectory(src)`.
2. **`pico_cyw43_arch_none` va en el executable** (`target_link_libraries` del
   bridge), NO en una lib STATIC aparte; si no, `pico/cyw43_arch.h` no llega a
   `main.c`.
3. **El GATT se genera con `pico_btstack_make_gatt_header`**; el `.gatt` debe
   estar en la misma dir que apunta el CMakeLists. Si falla `doorbell.h`, no
   lo reescribas a mano: copia el bloque 1:1 del `ble_doorbell` del SDK.
4. **OLED SSD1306 integrado en el build** (`ssd1306.c` + `ssd1306_font.h`, driver
   Verified de `/mnt/disk/src/rpico/pico_src/tmp/oled_ssd1306`). I2C0 SDA=GP4,
   SCL=GP5, addr 0x3C (sin conflicto con SWD GP2/GP3). `main.c` muestra nombre
   BLE + MAC (el MAC se captura en `HCI_STATE_WORKING` vía `gap_local_bd_addr`
   en `ble_server.c`). Ya NO hay duplicado `oled_ssd1306.h` (eliminado).
5. **README.md y docs/BUILD_INSTRUCTIONS.md están DESACTUALIZADOS**: describen
   un reproductor A2DP (`a2dp_source_host.c`, `bluetooth_device`) que no es este
   proyecto. El doc fiel es `docs/INFO.md` y `docs/SKILL.md`.

## Si algo falla

- `pico/cyw43_arch.h: No such file` → ver gotcha 2 (enlazar en el executable).
- Error `doorbell.h` → ver gotcha 3 (`pico_btstack_make_gatt_header`).
- No conecta por SWD → sonda/target: ver skill `flash-pico-swd`.

## Referencias

- `keyboard_oled/docs/INFO.md`, `keyboard_oled/docs/SKILL.md`, `keyboard_oled/src/CMakeLists.txt`
- Ejemplo Verified del SDK: `pico-examples/bluetooth/ble_doorbell`