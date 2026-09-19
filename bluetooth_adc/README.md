# Pico ADC Monitor — OLED + Bluetooth

Firmware para convertir una **Raspberry Pi Pico (RP2040)** en un medidor ADC que:

- Lee canales ADC con detección de cambios (hysteresis configurable en LSB).
- Muestra valores en un display OLED SSD1306 128x64 por I2C.
- Envía notificaciones por Bluetooth (UART, módulo HC-05/HC-06).
- Acepta comandos AT para configuración en tiempo real.
- Se compila y flashea con OpenOCD vía Debug Probe (SWD).

## Hardware requerido

| Componente | Conexión |
|---|---|
| **Pico (RP2040)** | Target principal |
| **OLED SSD1306 128x64** | I2C0: SDA=GP4, SCL=GP5, addr=0x3C |
| **HC-05/HC-06** | UART0: TX=GP0, RX=GP1, 9600 baud |
| **Entrada analógica** | ADC0=GPIO26 (por defecto) |
| **Debug Probe** | SWD: SWDIO=GP2, SWCLK=GP3, GND |

### Pinout resumen

```
Pico GPIO  Función
─────────────────────
GP0        UART0 TX → HC-05/06 RX
GP1        UART0 RX ← HC-05/06 TX
GP2        SWDIO (Debug Probe)
GP3        SWCLK (Debug Probe)
GP4        I2C0 SDA → OLED SDA
GP5        I2C0 SCL → OLED SCL
GP26       ADC0 (canal por defecto)
GP27       ADC1
GP28       ADC2
GP29       ADC3 (sensor interno de temperatura)
```

## Comandos AT

Todos los comandos terminan con `\r\n` y se reciben por USB serial o Bluetooth.

### Tabla de comandos

| Comando | Descripción | Respuesta ejemplo |
|---|---|---|
| `AT` | Test de conexión | `OK` |
| `AT+VERSION?` | Versión del firmware | `+VERSION:1.0.0 (Sep 18 2026 12:00:00)` |
| `AT+ADC?` | Leer todos los canales ADC | `ADC0=2048\r\nADC1=1024\r\nOK` |
| `AT+ADC=<canal>` | Leer canal específico (0-3) | `ADC0=2048\r\n+ADC0:1.650V,raw=2048\r\nOK` |
| `AT+ADCR` | Alias: leer todos los canales | Igual que `AT+ADC?` |
| `AT+ADCC` | Listar canales configurados | `+CHANNELS:4\r\n+CH0:GPIO26,thresh=16,...\r\nOK` |
| `AT+ADCCFG=<ch>,<lo>,<hi>` | Configurar umbrales de voltaje | `+ADCCFG:0,0.50,2.50\r\nOK` |
| `AT+THRESH=<n>` | Umbral de cambio en LSB (0-4095) | `+THRESH:0,16\r\nOK` |
| `AT+RATE=<ms>` | Periodo de muestreo (10-60000 ms) | `+RATE:100\r\nOK` |
| `AT+OLED=ON\|OFF` | Habilitar/deshabilitar OLED | `+OLED:OFF\r\nOK` |
| `AT+DISP=<page>` | Cambiar página del OLED (0-3) | `+DISP:2\r\nOK` |
| `AT+GRAPH=0\|1` | Toggle vista de gráfico | `+GRAPH:1\r\nOK` |
| `AT+STATUS` | Estado completo del sistema | Ver abajo |
| `AT+RST` | Reset a valores por defecto | `OK` (reinicia) |
| `AT+HELP` | Lista de comandos | Lista completa |

### Respuesta de AT+STATUS

```
+STATUS:Vref=3.30,Channels=4,Rate=100ms,OLED=ON,Uptime=3600s
+CH0:V=1.650,R=2048,Thresh=16,Min=0.100,Max=3.200,Avg=1.500,
+CH1:V=0.800,R=992,Thresh=16,Min=0.050,Max=2.800,Avg=1.200,
OK
```

## Formato de tramas Bluetooth (para app Android)

### Notificación de cambio (enviada cuando el ADC supera el umbral)

```
CHANGED:ADC0=2048,1.650V,delta=32
ADC0=2048
```

Líneas separadas:
1. `CHANGED:ADC<n>=<raw>,<voltage>V,delta=<lsb>` — detalle del cambio
2. `ADC<n>=<raw>` — valor simplificado para parsing fácil

### Datos periódicos (cada 5 segundos)

```
ADC,3600,1.650,0.800,2.100,0.450
```

Formato: `ADC,<timestamp_s>,<voltage_ch0>,<voltage_ch1>,...`

### Notificación de status

```
STATUS,System Ready
```

## Detección de cambios

El firmware compara cada lectura ADC con la anterior. Si la diferencia absoluta (`delta`) supera el umbral configurado (`change_threshold` en LSB), se:

1. Marca `changed=true` en el canal.
2. Envía notificación por Bluetooth.
3. El delta se calcula sobre 12 bits (0-4095).

**Umbral por defecto:** 16 LSB ≈ 13 mV (con Vref=3.3V)

Para reducir ruido, incrementar el umbral:
```
AT+THRESH=32    // ~26 mV
AT+THRESH=64    // ~52 mV
```

## Compilar

```bash
# Asegurar PICO_SDK_PATH
export PICO_SDK_PATH=/ruta/a/pico-sdk

# Compilar (default: pico_w)
cd pico_debugger_flash/bluetooth_adc
./build.sh

# O directamente con cmake:
mkdir -p build && cd build
cmake -DPICO_BOARD=pico_w -DPICO_SDK_PATH=$PICO_SDK_PATH ..
make -j$(nproc)
```

### Compilar para Pico 1 (sin WiFi/BLE)

```bash
BOARD=pico ./build.sh
# O:
cmake -DPICO_BOARD=pico ..
```

## Flashear

### Vía Debug Probe (SWD) — recomendado

```bash
./flash_nosudo.sh
# O manualmente:
cd build
openocd -f interface/cmsis-dap.cfg \
        -f target/rp2040.cfg \
        -c "program bluetooth_adc.elf verify reset exit"
```

### Vía BOOTSEL (drag & drop)

```bash
./flash_bootsel.sh
```

## Sesión de prueba Bluetooth

Conectar al módulo HC-05/HC-06 desde una terminal Bluetooth SPP (baud: 9600).

```
> AT
OK

> AT+VERSION?
+VERSION:1.0.0 (Sep 18 2026 12:00:00)
OK

> AT+ADC?
ADC0=2048
ADC1=1024
ADC2=512
ADC3=800
OK

> AT+ADC=0
ADC0=2048
+ADC0:1.650V,raw=2048
OK

> AT+THRESH=32
+THRESH:0,32
OK

> AT+RATE=500
+RATE:500
OK

> AT+OLED=OFF
+OLED:OFF
OK

> AT+STATUS
+STATUS:Vref=3.30,Channels=4,Rate=500ms,OLED=OFF,Uptime=45s
+CH0:V=1.650,R=2048,Thresh=32,Min=0.100,Max=3.200,Avg=1.500,
OK

> AT+HELP
+HELP:AT Commands v1.0.0
AT                  - Test connection
AT+VERSION?         - Firmware version
...
OK

> AT+RST
OK
```

### Formato de datos recibidos (cuando hay cambio)

Si el voltaje en ADC0 cambia más de 32 LSB:

```
CHANGED:ADC0=2080,1.676V,delta=32
ADC0=2080
```

## App Android

El firmware está diseñado para funcionar con una **app Android genérica de terminal Bluetooth SPP** (como "Serial Bluetooth Terminal"). La app puede:

1. **Enviar comandos AT** y ver respuestas en tiempo real.
2. **Recibir notificaciones** de cambio de ADC (`CHANGED:...`).
3. **Recibir datos periódicos** cada 5 segundos.

Para una app custom, parsear las líneas que empiezan con:
- `CHANGED:` — evento de cambio de ADC
- `ADC,` — datos periódicos
- `STATUS,` — notificaciones de estado

## Árbol de archivos

```
bluetooth_adc/
├── CMakeLists.txt        # Build configuration
├── main.c                # Entry point: init + loop principal
├── adc_monitor.c/.h      # Lectura ADC + detección de cambios
├── at_commands.c/.h      # Parser de comandos AT
├── bluetooth.c/.h        # Driver UART Bluetooth (HC-05/06)
├── oled_display.c/.h     # Páginas de display OLED
├── ssd1306.c/.h          # Driver SSD1306 I2C (incluido)
├── tusb_config.h         # Configuración TinyUSB
├── pico_sdk_import.cmake # Importador del Pico SDK
├── config.sh             # Configuración compartida de scripts
├── build.sh              # Script de compilación
├── flash_nosudo.sh       # Build + flash SWD sin sudo
├── flash_bootsel.sh      # Flash vía BOOTSEL
└── README.md             # Este archivo
```

## Licencia

MIT — ver LICENSE en el directorio padre.
