# Guía de solución de problemas - bluetooth_device

## No aparece el volumen RPI-RP2 al conectar

- Asegúrate de mantener pulsado **BOOTSEL** mientras conectas el USB y
  soltarlo después.
- Prueba otro cable USB (solo datos) y otro puerto.
- `dmesg | tail` debería mostrar un dispositivo `2e8a:0003 Raspberry Pi RP2 Boot`.

## El .uf2 no escribe (el volumen desaparece al copiar)

- Es normal: al copiar el .uf2 el Pico se reinicia y el volumen se desmonta.
  Verifica el estado tras unos segundos: la placa debe ejecutar el firmware.
- Si el volumen reaparece en BOOTSEL, el firmware se rechazó o no arrancó.

## El Pico no escanea / no conecta auriculares

- El `/dev/tty*` del adaptador UART: abre la consola a **115200** y observa los
  logs. Deberías ver `GAP: start inquiry` y el descubrimiento de dispositivos.
- Verifica que el auricular esté en **modo pairing/descubrible**.
- BT classic (A2DP) NO usa BLE: el auricular debe soportar BT clásico.
- El `gap_inquiry` busca sinks A2DP con clase `0x200408`; si tu dispositivo no
  envía esa clase durante la inquiry, no aparecerá.

## El stream se establece pero no se oye audio

- Confirma en logs que `A2DP Source: Stream started` apareció.
- Ajusta el volumen del auricular con sus botones (o `t`/`T` por UART).
- Si/no hay sonido con el MOD, prueba la fuente seno con `x`.
- Algunos chips CYW43439 con diferencia de versión de firmware requieren
  `HCI_ACL_PAYLOAD_SIZE` mayor; usa la config común oficial (ya incluida).

## Errores de compilación comunes

### `HCI_ACL_PAYLOAD_SIZE too small ...`
BTstack no encontró la config: asegúrate de que `btstack_config.h` y
`btstack_config_common.h` estén en el include path del target y que
`btstack_config.h` **no defina el guard** `_PICO_BTSTACK_BTSTACK_CONFIG_H`
antes de incluir el común (ambos archivos usan ese mismo guard).

### `error: 'HCI_ACL_PAYLOAD_SIZE' undeclared`
Igual que el anterior: la config no se está incluyendo. Revisa `target_include_directories`.

### Linker: undefined reference a `btstack_*`
Falta linkear una librería BTstack. Este proyecto necesita al menos:
`pico_btstack_cyw43`, `pico_btstack_classic`, `pico_btstack_sbc_encoder`.
Si usas MOD, también `pico_btstack_hxcmod_player` (o el core hxcmod manual).

## La Pico W se queda en bootloader (led piscando en bucle)

- El firmware no cabe en flash o el `PICO_BOARD` no es `pico_w`. Rebuild con
  `-DPICO_BOARD=pico_w`.

## ¿Por qué no funciona en un Pico RP2040 normal?

El RP2040 estándar **no tiene radio Bluetooth**. La radio vive en el chip
**CYW43439** soldado solo en el Pico W / Pico 2 W.