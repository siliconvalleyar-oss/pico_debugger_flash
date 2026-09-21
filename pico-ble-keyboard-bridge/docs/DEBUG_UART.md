# Debug por UART (consola serie del firmware)

Cómo ver los logs de diagnóstico del firmware usando el **UART del propio
Raspberry Pi Debug Probe** (la misma sonda que flashea por SWD), sin
herramientas extra ni segundas conexiones.

```
┌─────────────────────┐  USB (CMSIS-DAP + CDC-ACM)   ┌──────────────┐
│  PC de desarrollo   │ ◀──────────────────────────▶ │ Debug Probe  │
│  /dev/ttyACM0 (CDC) │      + SWD (flasheo)         │  (2e8a:000c) │
└─────────────────────┘                              └──────┬───────┘
                                                            │ UART 3v3
                                                            │ TX/RX/GND
                                                     ┌──────▼───────┐
                                                     │   Pico W     │
                                                     │ GP0=TX GP1=RX│
                                                     └──────────────┘
```

## Cableado (sonda ↔ Pico W target)

El firmware usa **UART0 del RP2040** como consola `stdio`:

| Señal | GPIO del Pico W (target) | Pin físico | Va al Debug Probe |
|---|---|---|---|
| UART0 **TX** | **GP0** | pin 1 | **UART RX** de la sonda |
| UART0 **RX** | **GP1** | pin 2 | **UART TX** de la sonda |
| GND          | GND     | pin 3 | **GND** de la sonda |

Cruzar TX↔RX (el TX de uno va al RX del otro) y **compartir GND**.
La sonda alimenta la UART a 3,3 V — no conectar a RS-232 (±12 V lo daña).

> El conector UART del Debug Probe es el header de 3 pines etiquetado
> **UART** (no confundir con el de SWD: 3 pines con SWDIO/SWCLK/GND).

## Configuración del firmware

Ambos proyectos del repo quedaron configurados igual: **UART0,
GP0=TX, GP1=RX, 115200-8N1**.

`pico-ble-keyboard-bridge/CMakeLists.txt` (y ya heredado igual en
`keyboard_oled/src/CMakeLists.txt`):

```cmake
target_compile_definitions(<target> PRIVATE
    PICO_DEFAULT_UART=0
    PICO_DEFAULT_UART_TX_PIN=0
    PICO_DEFAULT_UART_RX_PIN=1
    PICO_DEFAULT_UART_BAUD_RATE=115200
)
pico_enable_stdio_uart(<target> 1)
```

Los `printf()` del código salen por esa UART (BTstack además loguea
`ENABLE_LOG_INFO`/`ENABLE_LOG_ERROR` de `btstack_config_common.h`).

## Captura en la PC de desarrollo

El Debug Probe expone **dos interfaces USB**: la CMSIS-DAP (flasheo SWD)
y un **CDC-ACM** (`/dev/ttyACM0`) que es el puente de la UART.

```bash
# 115200, sin echo, en raw (una sola vez por sesión)
stty -F /dev/ttyACM0 115200 raw -echo

# Ver el log en vivo
picocom -b 115200 /dev/ttyACM0        # o: cat /dev/ttyACM0

# Captura con reinicio sincronizado (reset por SWD y log desde el boot)
stty -F /dev/ttyACM0 115200 raw -echo
(cat /dev/ttyACM0 > /tmp/boot.log &) 
./scripts/flash_nosudo_multi.sh keyboard_oled     # reprograma + reset
strings /tmp/boot.log
```

Para capturar desde el boot sin reflashear:

```bash
(cat /dev/ttyACM0 > /tmp/boot.log &)
openocd -f debugprobe-openocd.cfg -c "init; reset run; shutdown"
```

## Qué esperar en el log

`pico-ble-keyboard-bridge` (después de un reset):

```
=== Pico-KB-Bridge boot (uart0 GP0/GP1 @115200) ===
[main] usb_kbd + ble_gatt_server inicializados
[BLE] HCI_STATE_WORKING: radio listo, inicio advertising
```

Eventos posteriores (al conectar/emparejar desde el teléfono):

```
[BLE] conexion LE entrante (handle=0x0000)
[SM] JUST_WORKS_REQUEST: auto-aceptando
[SM] PAIRING_COMPLETE OK: bondido+encriptado, LED ON
[ATT] write en FFE1, 10 bytes
[BLE] desconexion (handle=0x0000)
```

`keyboard_oled`:

```
Pico KB Bridge started
```

## Solución de problemas

| Síntoma | Causa probable | Solución |
|---|---|---|
| `cat /dev/ttyACM0` cuelga sin datos | No hay cable UART entre sonda y target, o TX/RX sin cruzar | Revisar GP0→RX de sonda, GP1←TX de sonda, GND compartido |
| Basura ilegible | Baud rate distinto | `stty -F /dev/ttyACM0 115200` (los dos lados a 115200) |
| Aparece `/dev/ttyACM1` en vez de 0 | Otra CDC en el sistema | Probar `ls /dev/ttyACM*` y usar el correcto |
| `Permission denied` en ttyACM0 | Usuario fuera del grupo `dialout` | `sudo usermod -aG dialout $USER` y re-login |
| Log solo hasta "boot" y se corta | El `cat` murió al reiniciar el USB | Relanzar `cat` después del reset; o capturar con `timeout` |
| No hay stderr de BTstack | `WANT_HCI_DUMP` no definido | Es opcional; los `[BLE]/[SM]/[ATT]` del firmware bastan para el flujo del puente |

## Alternativa sin sonda: USB-TTL externo

Cualquier adaptador USB-serial 3,3 V (CP2102, FT232, CH340) sirve igual:
su RX a GP0, su TX a GP1, GND compartido, y abrir el puerto que cree
(`/dev/ttyUSB0`) a 115200. La sonda simplemente ahorra ese cable extra
porque ya está en el banco para flashear.
