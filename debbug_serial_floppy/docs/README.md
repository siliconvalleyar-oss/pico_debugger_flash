# Pico Serial Debugger

Herramienta en C++ para depurar la comunicación serie USB entre PC (Linux) y Raspberry Pi Pico.

## Características

- **Comunicación serie raw** (8N1, sin control de flujo) via termios
- **Dos modos de operación:**
  - Comando único: `./pico_serial_debug /dev/ttyACM0 sdtest`
  - Modo interactivo: `./pico_serial_debug /dev/ttyACM0`
- **Comandos predefinidos** para el firmware floppydisk_usb
- **Detección automática de puertos** serie disponibles
- **Configuración flexible**: baudrate, timeout, prompt personalizado

## Estructura del Proyecto

```
debbug_serial_floppy/
├── Makefile              # Build: obj/*.o -> bin/pico_serial_debug
├── include/
│   └── serial.h          # Clase SerialPort (API pública)
├── src/
│   ├── main.cpp          # CLI principal
│   └── serial.cpp        # Implementación termios
├── obj/                  # Archivos objeto (gitignored)
├── bin/                  # Binario final (gitignored)
└── docs/                 # Documentación
```

## Compilación

```bash
cd debbug_serial_floppy
make          # Compila en bin/pico_serial_debug
make clean    # Limpia obj/ y bin/
make install  # Instala en /usr/local/bin/
```

## Uso

### Modo Comando Único

```bash
# Sintaxis: pico_serial_debug [opciones] <puerto> <comando>
./bin/pico_serial_debug /dev/ttyACM0 version
./bin/pico_serial_debug /dev/ttyACM1 sdtest
./bin/pico_serial_debug -b 115200 /dev/ttyACM0 "help"
```

### Modo Interactivo

```bash
./bin/pico_serial_debug /dev/ttyACM1
```

Dentro del modo interactivo:
```
pico> sdtest          # Test microSD completo
pico> version         # Versión firmware
pico> fat             # Info sistema archivos
pico> ls              # Listar imágenes
pico> help            # Ayuda shell Pico
pico> exit            # Salir
```

**Comandos especiales (prefijo `!`):**
```
pico> !list           # Listar puertos serie disponibles
pico> !baud 115200    # Cambiar baudrate
pico> !timeout 5000   # Cambiar timeout (ms)
pico> !prompt "floppydisk> "  # Cambiar prompt esperado
```

### Opciones CLI

| Opción | Descripción | Default |
|--------|-------------|---------|
| `-b, --baud <rate>` | Baudrate | 115200 |
| `-t, --timeout <ms>` | Timeout lectura | 3000 |
| `-p, --prompt <str>` | Prompt esperado | "floppydisk> " |
| `-l, --list` | Listar puertos disponibles | - |
| `-h, --help` | Mostrar ayuda | - |

### Listar Puertos

```bash
./bin/pico_serial_debug --list
# o en modo interactivo:
pico> !list
```

Salida ejemplo:
```
Available serial ports:
  /dev/ttyACM0
  /dev/ttyACM1
```

## Arquitectura

### Clase SerialPort (include/serial.h)

```cpp
class SerialPort {
public:
    bool open(const std::string& device, int baudrate = 115200);
    void close();
    bool isOpen() const;

    ssize_t write(const uint8_t* data, size_t len);
    ssize_t write(const std::string& str);

    // Lectura con timeout (ms)
    // Retorna: bytes leídos, 0 = timeout, -1 = error
    ssize_t read(uint8_t* buffer, size_t max_len, int timeout_ms = 1000);

    std::string readLine(int timeout_ms = 1000);

    // Envía comando + \r\n, lee líneas hasta encontrar prompt
    std::vector<std::string> sendCommand(const std::string& cmd,
                                          const std::string& prompt = "floppydisk> ",
                                          int timeout_ms = 3000);
    
    void flush();
    int getFd() const;  // Para select/poll
};
```

### Flujo de Comunicación

```
PC                                    Pico
 |                                      |
 |--- "sdtest\r\n" (write) ------------>|
 |                                      | (procesa comando)
 |<-- "SD SPI Test...\nCMD0[0]...\n" <--| (read lines)
 |                                      |
 |--- "help\r\n" ---------------------->|
 |<-- "Comandos disponibles:\n..." <----|
```

### Configuración Puerto Serie (termios)

- **8N1**: 8 bits, sin paridad, 1 bit stop
- **Raw mode**: sin procesamiento de entrada/salida
- **No flow control**: sin XON/XOFF, sin RTS/CTS
- **Non-blocking reads** con timeout via `select()`
- **VMIN=0, VTIME=0** para control manual de timeout

## Integración con Firmware Pico

El debugger está diseñado para trabajar con el firmware `floppydisk_usb` que expone un shell UART/USB con estos comandos:

| Comando | Descripción |
|---------|-------------|
| `help` | Lista comandos disponibles |
| `version` | Versión, build, commit git |
| `echo on/off` | Activar/desactivar eco |
| `reboot` | Reiniciar Pico |
| `fat` | Info FAT (libre/total KB) |
| `ls` | Listar imágenes en /IMG |
| `dump <base> <ext> [blk] [cnt]` | Dump bloques imagen |
| `hex <base> <ext> <offset> [len]` | Hex dump |
| `sdtest` | Test completo microSD SPI |

El prompt por defecto es `floppydisk> ` (configurable con `-p`).

## Puertos Serie Típicos

| Dispositivo | Puerto |
|-------------|--------|
| Pico (USB CDC) | `/dev/ttyACM0`, `/dev/ttyACM1` |
| PicoProbe (CMSIS-DAP) | `/dev/ttyACM0` (si solo uno) |
| USB-Serial genérico | `/dev/ttyUSB0`, `/dev/ttyUSB1` |
| UART GPIO | `/dev/ttyS0`..`/dev/ttyS4` |

> **Nota**: En la Raspberry Pi remota, `/dev/ttyACM0` suele ser el PicoProbe (para flashear SWD) y `/dev/ttyACM1` el Pico target (shell UART).

## Permisos

El usuario debe estar en el grupo `dialout`:

```bash
sudo usermod -a -G dialout $USER
newgrp dialout
# o reiniciar sesión
```

## Solución de Problemas

| Problema | Solución |
|----------|----------|
| "Permission denied" | Agregar usuario a grupo `dialout` |
| "No such file" | Verificar puerto con `ls /dev/ttyACM*` |
| Timeout en comandos | Aumentar `-t 5000` o verificar baudrate |
| Prompt no detectado | Verificar prompt con `-p "otro> "` |
| Caracteres extraños | Verificar baudrate coincida (115200 default) |

## Ejemplos Avanzados

### Script de Test Automatizado

```bash
#!/bin/bash
PORT="/dev/ttyACM1"
TOOL="./bin/pico_serial_debug"

echo "=== Test suite ==="
$TOOL $PORT version
$TOOL $PORT fat
$TOOL $PORT ls
$TOOL $PORT sdtest
echo "=== Done ==="
```

### Monitor Continuo (via bash wrapper)

```bash
# Usar el script bash incluido para monitor continuo
./scripts/serial_debug.sh /dev/ttyACM1
# Seleccionar opción 'm' para monitor
```

## Licencia

MIT - Parte del proyecto pico_debugger_flash