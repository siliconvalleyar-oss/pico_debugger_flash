# bluetooth_device - Bluetooth Audio Host (A2DP Source) para Pico W

Reproductor Bluetooth de bolsillo basado en **Raspberry Pi Pico W / Pico 2 W**:
convierte el Pico en un **host Bluetooth** que escanea, se empareja y transmite
audio (A2DP Source + AVRCP) hacia auriculares o altavoces Bluetooth.

- **Plataforma:** RP2040 (Pico W) / RP2350 (Pico 2 W), radio CYW43439
- **Lenguaje:** C (BTstack), firmware para Pico SDK
- **Role:** A2DP Source (emisor de audio) + AVRCP Target (control remoto)
- **Versión actual:** v0.1.0

## Funcionalidades

- **Auto-scan al arrancar**: busca dispositivos Bluetooth cercanos y conecta con
  el primer **A2DP Sink** (auriculares / altavoces / barras de sonido).
- **Playback automático**: una vez establecido el stream, reproduce audio
  directamente (sin necesidad de comandos).
- **Dos fuentes de audio**: tono seno (test rápido) y reproductor de módulos
  ProTracker (MOD), conmutables por UART.
- **AVRCP Target**: permite play/pause/stop y consulta de estado desde el
  auricular (si lo soporta).
- **Emparejamiento legacy**: PIN `0000` automático al emparejar.
- **Logs por UART0**: visibles con un adaptador USB-UART a 115200 baud.

## Estructura

```
bluetooth_device/
├── CMakeLists.txt              # proyecto (Pico SDK, config BTstack)
├── btstack_config.h            # config BTstack del proyecto
├── btstack_config_common.h     # config común de pico-examples (tuning)
├── mods/                       # módulo de audio (MOD) + mod.h
│   ├── mod.h
│   └── nao-deceased_by_disease.c
└── src/
    ├── CMakeLists.txt
    ├── main.c                  # init cyw43 + run loop BTstack
    └── a2dp_source_host.c      # host A2DP/AVRCP (adaptado de BTstack demo)
```

## Versiones

| Versión | Descripción               |
|---------|---------------------------|
| v0.1.0  | Primer firmware funcional |

---

Licencia MIT (el código base A2DP proviene de BlueKitchen GmbH / BTstack).