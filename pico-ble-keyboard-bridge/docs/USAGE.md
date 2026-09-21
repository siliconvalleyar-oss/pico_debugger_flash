# Guía de uso

## Tabla de contenidos
- [Primer emparejamiento (pairing)](#primer-emparejamiento-pairing)
- [Comandos especiales soportados](#comandos-especiales-soportados)
- [Ejemplos de payloads](#ejemplos-de-payloads)
- [Probar sin teléfono (script de escritorio)](#probar-sin-teléfono-script-de-escritorio)
- [Errores comunes](#errores-comunes)

## Primer emparejamiento (pairing)

1. Flasheá la Pico W con `pico_ble_keyboard_bridge.uf2` (ver README.md).
2. Conectala por USB a la Raspberry Pi destino. El LED debe quedar apagado
   (todavía no hay conexión BLE).
3. En tu teléfono, abrí una app BLE genérica, por ejemplo **nRF Connect
   for Mobile** (Android/iOS, gratuita).
4. Escaneá dispositivos BLE cercanos y buscá **`Pico-KB-Bridge`**.
5. Tocá **Connect**. La primera vez, el sistema operativo del teléfono
   va a pedir confirmar el emparejamiento ("Pair with Pico-KB-Bridge?").
   Confirmá.
6. Una vez conectado, el LED de la Pico se pone **fijo** (bonding +
   encriptación activos).
7. Dentro de la app, expandí el servicio con UUID `0000FFE0-...` y
   ubicá la característica `0000FFE1-...`. Tocá el ícono de escritura
   (lápiz o "Write").

De acá en adelante, ese teléfono queda **bondeado**: las próximas veces
se reconecta automáticamente sin pedir confirmación de nuevo.

## Comandos especiales soportados

| Comando | Efecto |
|---|---|
| `\ENTER` | Pulsa Enter |
| `\TAB` | Pulsa Tab |
| `\ESC` | Pulsa Escape |
| `\DEL` | Pulsa Backspace |
| `\SLEEP` | Espera 500ms antes del siguiente evento |

Se pueden combinar varios comandos y texto plano en un mismo paquete,
separados por espacios:

```
usuario\TABcontraseña123\ENTER
```

## Ejemplos de payloads

Desde el campo "Write value" de nRF Connect (como texto UTF-8):

```
echo hola mundo\ENTER
```

```
cd /home/pi\ENTER
```

```
\SLEEP\SLEEPls -la\ENTER
```
(dos `\SLEEP` seguidos = 1 segundo de espera antes de tipear, útil si la
Raspberry Pi todavía está terminando de bootear cuando conectás la Pico)

## Probar sin teléfono (script de escritorio)

Si tu computadora de desarrollo tiene Bluetooth LE, podés probar el
servicio con `bluetoothctl` en Linux en vez de un teléfono:

```bash
bluetoothctl
[bluetooth]# scan on
# ... esperar a ver Pico-KB-Bridge y anotar su MAC ...
[bluetooth]# scan off
[bluetooth]# pair AA:BB:CC:DD:EE:FF
[bluetooth]# connect AA:BB:CC:DD:EE:FF
[bluetooth]# menu gatt
[bluetooth]# select-attribute 0000ffe1-0000-1000-8000-00805f9b34fb
[bluetooth]# write "hola\ENTER"
```

## Errores comunes

| Síntoma | Causa | Solución |
|---|---|---|
| La app BLE no encuentra `Pico-KB-Bridge` | Advertising no arrancó (radio no llegó a `HCI_STATE_WORKING`) | Revisar consola UART; reflashear; confirmar que es una Pico **W** |
| Pairing falla o se cae a mitad de camino | Interferencia BLE, o el teléfono tenía un bonding viejo corrupto | "Olvidar" el dispositivo en el teléfono y reintentar desde cero |
| La escritura no produce ninguna tecla | El payload no es texto ASCII válido, o el link no completó bonding | Revisar que la app mande como "UTF-8 string" y no como "hex" |
| El comando `\ENTER` se tipea literalmente como texto | Typo en el comando (falta la barra invertida, mayúsculas incorrectas) | Los comandos son *case-sensitive*: `\ENTER`, no `\enter` |
| Las teclas salen en el orden incorrecto o repetidas | Buffer circular lleno (`KeyQueue::kCapacity = 256`) por mandar texto muy largo de una sola vez | Mandar paquetes BLE más cortos, con pausas entre ellos |
