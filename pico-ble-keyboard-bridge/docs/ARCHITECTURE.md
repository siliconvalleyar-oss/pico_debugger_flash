# Arquitectura

## Tabla de contenidos
- [Vista general](#vista-general)
- [Flujo de datos](#flujo-de-datos)
- [Módulos](#módulos)
- [Máquina de estados BLE](#máquina-de-estados-ble)

## Vista general

```
                          ┌───────────────────────────────────────┐
                          │              Raspberry Pi Pico W        │
                          │                                          │
   BLE (bonded) ────────▶ │  ble_gatt_server.cpp                    │
                          │       │  parsea texto / comandos \XXX    │
                          │       ▼                                  │
                          │  key_queue.h  (buffer circular)          │
                          │       │                                  │
                          │       ▼                                  │
                          │  usb_hid_keyboard.cpp                    │
                          │       │  arma reportes HID de 8 bytes    │
                          │       ▼                                  │
                          │  TinyUSB (tud_hid_keyboard_report)       │
                          └───────────────┬──────────────────────────┘
                                          │ USB HID Boot Keyboard
                                          ▼
                          ┌───────────────────────────────────────┐
                          │      Raspberry Pi destino (sin teclado) │
                          └───────────────────────────────────────┘
```

## Flujo de datos

```
Teléfono (app BLE)
   │  escribe "hola\ENTER" en característica 0000FFE1
   ▼
att_write_callback()               [ble_gatt_server.cpp]
   │
   ▼
process_incoming_payload()         separa texto plano de comandos \XXX
   │
   ├─▶ usb_kbd_type_string("hola") ──▶ ascii_to_hid() por cada letra ──▶ KeyQueue.push()
   │
   └─▶ handle_special_command("\ENTER") ──▶ usb_kbd_press(kEnter) ──▶ KeyQueue.push()

Loop principal (main.cpp)
   │
   ▼
usb_kbd_task()                     [usb_hid_keyboard.cpp]
   │  cada iteración, si tud_hid_ready():
   │    - si hay tecla "presionada" pendiente de soltar -> release
   │    - si no, pop() de la cola -> send_report()
   ▼
tud_hid_keyboard_report()          TinyUSB arma el reporte de 8 bytes
   │
   ▼
Host USB (Raspberry Pi destino) recibe el reporte como si fuera
un teclado físico presionando/soltando teclas.
```

## Módulos

| Módulo | Responsabilidad |
|---|---|
| `usb_descriptors.cpp` | Descriptores USB estándar (device, config, HID report) para que el host reconozca el dispositivo como teclado boot-protocol. |
| `usb_hid_keyboard.{h,cpp}` | Cola de reportes HID y su envío no bloqueante, respetando el polling interval del host. |
| `ascii_to_hid.{h,cpp}` | Tabla de conversión de caracteres ASCII imprimibles a Usage IDs HID (layout US QWERTY). |
| `key_queue.{h,cpp}` | Buffer circular genérico (single-producer/single-consumer) para desacoplar la llegada BLE del envío USB. |
| `ble_gatt_server.{h,cpp}` | Inicialización de BTstack, definición del servicio GATT, manejo de bonding y parseo de comandos especiales. |
| `main.cpp` | Orquesta la inicialización y el loop cooperativo. |

## Máquina de estados BLE

```
        ┌─────────────┐   hci_power_control(ON)   ┌─────────────┐
        │   BOOTING   │ ────────────────────────▶ │ ADVERTISING │
        └─────────────┘                            └──────┬──────┘
                                                            │ conexión entrante
                                                            ▼
                                                    ┌───────────────┐
                                                    │  CONNECTED    │
                                                    │ (sin bonding) │
                                                    └──────┬────────┘
                                        SM_EVENT_JUST_WORKS_REQUEST
                                                            │ sm_just_works_confirm()
                                                            ▼
                                                    ┌───────────────┐
                                          LED ON ◀──│ BONDED /      │
                                                    │ ENCRYPTED     │──▶ acepta writes en 0000FFE1
                                                    └──────┬────────┘
                                                            │ desconexión
                                                            ▼
                                                    ┌───────────────┐
                                          LED OFF ◀─│ DISCONNECTED  │──▶ vuelve a ADVERTISING
                                                    └───────────────┘
```
