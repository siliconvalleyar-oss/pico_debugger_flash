# Build Instructions

## Requisitos previos

- Pico SDK con BTstack, p. ej. en `../../pico-sdk` relativo a la raíz del
  proyecto (o definiendo `PICO_SDK_PATH`).
- Toolchain `arm-none-eabi-gcc` 13.x o superior.
- `cmake` >= 3.13 y `make`.
- Python 3 (lo usa el SDK).

El build localiza/descarga `picotool` automáticamente (o puedes instalarlo).

> IMPORTANTE: este firware usa la radio Bluetooth del **Pico W / Pico 2 W**
> (CYW43439). No es compatible con un Pico RP2040 estándar sin radio BT.

## Configurar el build

```bash
cd bluetooth_device
mkdir -p build
cd build
cmake .. -DPICO_BOARD=pico_w
```

Si el SDK no está en la ubicación por defecto:

```bash
cmake -DPICO_SDK_PATH=/ruta/a/pico-sdk -DPICO_BOARD=pico_w ..
# o via entorno:
export PICO_SDK_PATH=/ruta/a/pico-sdk
```

## Compilar

```bash
make -j$(nproc)
```

## Salidas

```
build/src/bluetooth_device.uf2   # firmware para flashear (BOOTSEL)
build/src/bluetooth_device.bin   # binario plano
build/src/bluetooth_device.elf   # binario con símbolos (debug)
```

Tamaño de referencia (v0.1.0): `.bin` ≈ 542 KB, `.uf2` ≈ 1084 KB. La flash del
Pico W es de 2 MB; el firmware ocupa menos de un tercio.

## Flashear

1. Conecta el Pico W manteniendo pulsado **BOOTSEL**, luego suelta.
2. Aparece el volumen `RPI-RP2`.
3. Copia `bluetooth_device.uf2` al volumen:

   ```bash
   cp build/src/bluetooth_device.uf2 /media/$(whoami)/RPI-RP2/
   ```

4. El Pico se reinicia y ejecuta el firmware automáticamente.

## Consola UART

El firmware imprime logs por **UART0**:

| Señal | GPIO |
|-------|------|
| TX    | GPIO0 |
| RX    | GPIO1 |
| GND   | GND   |

Conecta TX->RX, RX->TX y GND, y abre el puerto a **115200 baud, 8N1**. Al
arrancar deberías ver el escaneo y la conexión al primer dispositivo A2DP Sink.

## Orden de conexión (STDIN)

Con `HAVE_BTSTACK_STDIN` la demo acepta comandos por UART (además del auto-scan):

- `a` – escanear de nuevo
- `b`/`B` – conectar/desconectar
- `x` – fuente: tono seno
- `z` – fuente: módulo MOD
- `p` – pausa
- `w`/`e` – reconfigurar a 44100/48000 Hz
- `t`/`T`/`v`/`V` – volumen AVRCP (bajar/subir)