# SKILL.md - bluetooth_device (Host Bluetooth A2DP Source para Pico W)

Referencia técnica completa del proyecto **bluetooth_device**. Diseñado para
que un agente de IA o desarrollador pueda comprender, modificar y mantener el
proyecto de forma autónoma.

---

## 1. Resumen del Proyecto

Convierte una **Raspberry Pi Pico W / Pico 2 W** en un **host Bluetooth de
audio**: escanea dispositivos Bluetooth cercanos, se empareja con auriculares
o altavoces (**A2DP Sink**) y transmite audio en streaming. Cumple el rol de
**A2DP Source** (emisor de audio) y **AVRCP Target** (expone play/pause del
auricular para controlar al host).

- **Plataforma:** RP2040 (Pico W) / RP2350 (Pico 2 W) con radio CYW43439
- **Lenguaje:** C
- **SDK:** Pico SDK + **BTstack** (incluido en el SDK, `lib/btstack`)
- **Stack de audio:** A2DP Source + AVRCP Target, codificador **SBC**
- **Banda:** Classic Bluetooth (BR/EDR); se linkea BLE pero A2DP es BT clásico
- **Versión actual:** v0.1.0
- **Licencia:** MIT (partes BTstack © BlueKitchen GmbH)

### Funcionalidades clave

- **Auto-scan al arrancar**: la demo de BTstack escanea por defecto solo si
  NO hay stdin; este proyecto elimina ese guard para que el Pico escanee siempre
  en el arranque (host headless).
- **Auto-conexión**: conecta con el primer sink A2DP descubierto.
- **Auto-play**: al establecerse `A2DP_SUBEVENT_STREAM_ESTABLISHED` inicia el
  stream automáticamente.
- **Fuentes de audio**: tono seno (`STREAM_SINE`) y módulo ProTracker
  (`STREAM_MOD`, via painter hxcmod). Por defecto la demo arranca en `MOD`;
  este proyecto mantiene la fuente seleccionable por UART (`x`/`z`).
- **AVRCP**: play/pause/stop controlables desde el auricular, status query.
- **Logs por UART0** a 115200 baud con toda la traza BTstack.
- **Led onboard** como indicador de actividad (`hal_led_toggle`, cyw43 GPIO 0).

---

## 2. Arquitectura del Firmware

```
                    ┌──────────────────────────────────────┐
                    │              main.c                  │
                    │  stdio_init_all                      │
                    │  cyw43_arch_init (radio CYW43439)    │
                    │  btstack_main(0, NULL)  → setup      │
                    │  btstack_run_loop_execute()  (∞)     │
                    └───────────────┬──────────────────────┘
                                    │ (idle loop / eventos)
                    ┌───────────────▼──────────────────────┐
                    │      a2dp_source_host.c              │
                    │  (adaptado de BTstack a2dp_source_demo)│
                    │                                      │
                    │  - SDP: A2DP Source + AVRCP Target   │
                    │  - hci_packet_handler: scan/connect  │
                    │  - a2dp_packet_handler: stream       │
                    │  - avrcp_target_packet_handler: ctl  │
                    │  - media_tracker: estado stream      │
                    │  - sin_generator / hxcmod (MOD)      │
                    └───────────────┬──────────────────────┘
                                    │ SBC encode + L2CAP
                    ┌───────────────▼──────────────────────┐
                    │    pico_btstack library de pico-sdk  │
                    │  cyw43 BT → pico_btstack_cyw43       │
                    │  classic  → pico_btstack_classic     │
                    │  sbc      → pico_btstack_sbc_encoder │
                    │  mod      → pico_btstack_hxcmod_player│
                    └───────────────┬──────────────────────┘
                                    │ UART HCI over SPI (cyw43)
                              radio CYW43439 ⇄ auricular BT
```

### Módulos

| Archivo | Rol |
|---|---|
| `src/main.c` | init placa + radio, lanza run loop BTstack |
| `src/a2dp_source_host.c` | host A2DP/AVRCP completo (funcionamiento real) |
| `mods/mod.h` | declara `mod_data[]`, `mod_len`, `mod_name` |
| `mods/nao-deceased_by_disease.c` | datos del módulo ProTracker |
| `btstack_config.h` | config BTstack del proyecto (incluye el común) |
| `btstack_config_common.h` | tuning común (oficial pico-examples) |
| `CMakeLists.txt` / `src/CMakeLists.txt` | build |

---

## 3. Configuración BTstack (tuning clave)

La config vive en `btstack_config.h` + `btstack_config_common.h`. Puntos
críticos:

- **Sin guard propio** en `btstack_config.h`: debe `#include
  "btstack_config_common.h"` y NO definir `_PICO_BTSTACK_BTSTACK_CONFIG_H`
  antes, porque el archivo común lo define. Si se define el guard primero, el
  contenido del común se omite y BTstack compila con valores por defecto
  (rompe con `HCI_ACL_PAYLOAD_SIZE too small`).
- `ENABLE_CLASSIC` / `ENABLE_BLE`: se activan **automáticamente** por las
  librerías `pico_btstack_classic` / `pico_btstack_ble` que el SDK inyecta
  como `-DENABLE_CLASSIC` / `-DENABLE_BLE` a través de la config común.
- `HCI_ACL_PAYLOAD_SIZE (1691 + 4)` y `HCI_ACL_CHUNK_SIZE_ALIGNMENT 4`:
  necesario para A2DP (SBC en paquetes L2CAP grandes) y para el bus cyw43.
- `ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL`: evita overrun del bus SPI
  compartido del CYW43439.
- `ENABLE_SCO_OVER_HCI`: habilita eSCO sobre HCI (audio de voz; no usado por
  A2DP pero inofensivo).
- `MAX_NR_AVDTP_CONNECTIONS 1`, `MAX_NR_AVDTP_STREAM_ENDPOINTS 1`,
  `MAX_NR_AVRCP_CONNECTIONS 2`: config 1-a-1 para este caso de uso.
- `BTSTACK_MEMORY_ALLOCATOR_*`: usa el pool del SDK (ver el común).

Referencia de la doc oficial: `pico-sdk/src/rp2_common/pico_btstack/doc.h`.

---

## 4. Flujo de Funcionamiento (runtime)

1. **Boot**: `main()` → `cyw43_arch_init()` (enciende radio), `btstack_main()`
   registra servicios SDP (A2DP Source + AVRCP Target), setea nombre local
   `"Pico W BT Host 00:00:00:00:00:00"`, clase de dispositivo `0x200408`,
   y `data_source = STREAM_MOD`. Luego corre el run loop de BTstack.
2. **`hci_power_control(HCI_POWER_ON)`** → el chip inicia el stack.
3. **`BTSTACK_EVENT_STATE` → `HCI_STATE_WORKING`**: el handler llama
   `a2dp_source_demo_start_scanning()` (gap_inquiry) **siempre** (este
   proyecto quitó el `#ifndef HAVE_BTSTACK_STDIN` para que sea headless).
4. En `GAP_EVENT_INQUIRY_RESULT` la demo filtra por clase de dispositivo
   `bluetooth_speaker_cod = 0x200000 | 0x040000 | 0x000400` (Audio | Rendering).
   Si coincide, copia la dirección a `device_addr`, detiene la inquiry y llama
   `a2dp_source_establish_stream(device_addr, &media_tracker.a2dp_cid)`.
   Los dispositivos que requieren pairing legacy hacen `HCI_EVENT_PIN_CODE_REQUEST`
   → `gap_pin_code_response(address, "0000")`.
5. Si la inquiry termina sin encontrar sinks (`GAP_EVENT_INQUIRY_COMPLETE` y
   `scan_active`), relanza la inquiry en bucle (búsqueda continua).
6. `A2DP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED` → `avdtp_connect_source`
   + configurar SEID → `a2dp_source_start_stream`.
7. `A2DP_SUBEVENT_STREAM_ESTABLISHED` → marca `stream_opened = 1` y llama
   `a2dp_source_start_stream()` (auto-play, en local_seid remoto).
8. `A2DP_SUBEVENT_STREAM_STARTED` → `play_info.status = PLAYING`, notifica
   AVRCP (now playing + playback status), arranca `a2dp_demo_timer_start`.
   A partir de ahí el callable de media del source suministra PCM (seno o
   hxcmod-decoded) y BTstack lo codifica a SBC para el aire.
9. Control remoto: eventos `AVRCP_SUBEVENT_OPERATION` con
   `AVRCP_OPERATION_ID_PLAY/PAUSE/STOP` mapean a start/pause/disconnect del
   stream.

### Fuente de audio (data_source)

```c
typedef enum { STREAM_SINE = 0, STREAM_MOD } audio_data_source_t;
int current_track_index;   // índice en tracks[]
media_source_t media_source; // seno | mod (gestionan el callable)
```

- `STREAM_SINE`: genera onda seno a 8 kHz (test rápido, muy barato).
- `STREAM_MOD`: decodifica `mod_data` con hxcmod a `current_sample_rate`
  (44100/48000 Hz). `a2dp_demo_hexcmod_configure_sample_rate()` reconfigura.

---

## 5. Comandos UART (STDIN)

Solo si `HAVE_BTSTACK_STDIN` está definido (viene en la config común). Funciona
además del auto-scan:

| Tecla | Acción |
|-------|--------|
| `a` | Escanear de nuevo |
| `b` / `B` | Conectar / desconectar stream |
| `x` | Fuente: tono seno |
| `z` | Fuente: módulo MOD |
| `p` | Pausar stream |
| `w` / `e` | Reconfigurar 44100 Hz / 48000 Hz |
| `t` / `T` | Volume down / up (AVRCP) |
| `v` / `V` | Volume down / up (AVRCP) alternativo |

> En la demo original `z`/MOD solo funciona si `mod_data[]` está presente; este
> proyecto compila el asset `nao-deceased_by_disease.c`.

---

## 6. Build

```bash
mkdir -p build && cd build
cmake .. -DPICO_BOARD=pico_w
make -j$(nproc)
```

- El `project()` debe declarar **C CXX ASM** (BTstack y cyw43 usan libs C++; la
  demo original es C pura pero este proyecto linkea pico libs).
- Requiere la **config BTstack en el include path de `bluetooth_device`**
  (raíz del proyecto, donde vive `btstack_config.h`).
- Linkea: `pico_stdlib`, `pico_cyw43_arch_none`, `pico_btstack_cyw43`,
  `pico_btstack_classic`, `pico_btstack_ble`, `pico_btstack_sbc_encoder`,
  y `bluetooth_hxcmod` (interfaz que compila `hxcmod.c` del SDK + `mods/...c`).
- Salidas: `build/src/bluetooth_device.uf2` (+ `.bin`, `.elf`).

### Tamaños de referencia (v0.1.0)

- `.bin` = 542124 B (0x845AC)
- `.uf2` = 1084416 B
- flash total Pico W = 2 MB → margen amplio

---

## 7. Conectar / Probar

1. Flashea `.uf2` en modo BOOTSEL (ver BUILD_INSTRUCTIONS).
2. Conecta un USB-UART a GPIO0 (TX) / GPIO1 (RX), 115200 baud 8N1.
3. Pon el auricular en modo emparejamiento y enciende el Pico.
4. Espera los logs: `inquiry` → conexión A2DP → `Stream started` → audio.
5. Desde el auricular: play/pause (AVRCP) debe pausar/reanudar el stream.

---

## 8. Troubleshooting (resumen)

| Síntoma | Causa / Solución |
|---|---|
| No compila con `HCI_ACL_PAYLOAD_SIZE too small` | config BTstack no incluida o con guard propio que oculta el común |
| No escanea al arrancar | falta el auto-scan; este proyecto lo fuerza quitando `#ifndef HAVE_BTSTACK_STDIN` |
| No conecta | auricular no en modo pairing, o el inquiry no lo ve (clase COD) |
| Se establece stream pero sin audio | probar fuente seno (`x`), subir volumen del auricular |
| Un Pico RP2040 normal no hace nada | no tiene radio BT; solo Pico W / Pico 2 W |

Detalle ampliado en `docs/TROUBLESHOOTING.md`.

---

## 9. Referencias

- BTstack demo original: `pico-sdk/lib/btstack/example/a2dp_source_demo.c`
- Pico BTstack docs: `pico-sdk/src/rp2_common/pico_btstack/doc.h`
- Config común: `pico-examples/bluetooth/config/btstack_config_common.h`
- Pico examples A2DP: `pico-examples/bluetooth/btstack_examples/a2dp_source_demo/`
- hxcmod player: `pico-sdk/lib/btstack/3rd-party/hxcmod-player/`