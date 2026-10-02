# Pico 1W USB -> WiFi Dongle (USB Tethering)

Convierte una Raspberry Pi Pico 1W en un dongle WiFi que:
- Se conecta por USB a un host (PC, router, telefono) y obtiene Internet via RNDIS/CDC-ECM
- Crea un Access Point WiFi (SoftAP) con SSID/password propio
- Hace NAT/forwarding para que clientes WiFi naveguen a traves del USB del host

## Arquitectura

```
[ Cliente WiFi ] --(802.11)--> [ Pico 1W SoftAP ] --(lwIP NAT) -->
[ USB RNDIS/CDC-ECM ] --(USB)--> [ Host con Internet ]
```

## Requisitos

- Raspberry Pi Pico 1W (RP2040 + CYW43439)
- pico-sdk
- TinyUSB (incluido en pico-sdk)
- lwIP (incluido en pico-sdk)
- OLED SSD1306 128x64 en I2C (GP4=SDA, GP5=SCL) - opcional

## Compilacion

```bash
cd dongle_wifi_usb
./build.sh
```

Genera: `build/dongle_wifi_usb.uf2`

## Flasheo

### Opcion A: Directo desde esta PC (modo BOOTSEL)
1. Mantener BOOTSEL pulsado en la Pico
2. Conectar USB a la PC
3. Copiar UF2:
   ```bash
   cp build/dongle_wifi_usb.uf2 /media/$USER/RPI-RP2/
   ```

### Opcion B: Via Raspberry Pi remota
```bash
# En la PC: compilar y copiar UF2 a la Pi
scp build/dongle_wifi_usb.uf2 joy@raspberry.local:/home/joy/

# En la Pi: flashear
ssh joy@raspberry.local "cd /home/joy/src/pico/pico_debugger_flash && git pull"
# Luego copiar UF2 a la Pico en modo BOOTSEL desde la Pi
```

## Configuracion (config.h)

```c
#define WIFI_SSID              "PicoDongle"
#define WIFI_PASSWORD          "pico1234"
#define WIFI_CHANNEL           6
#define AP_IP_ADDR             "192.168.4.1"
#define AP_DHCP_START          "192.168.4.2"
#define AP_DHCP_END            "192.168.4.20"
```

## Pruebas

1. Conectar Pico 1W por USB a host con Internet
2. En host: verificar interfaz USB RNDIS (ej: `usb0` en Linux, `RNDIS` en Windows)
3. Conectar dispositivo WiFi a SSID "PicoDongle" / password "pico1234"
4. Verificar:
   - Ping a gateway: `ping 192.168.4.1`
   - Ping externo: `ping 8.8.8.8`
   - Navegacion HTTP/HTTPS

## OLED SSD1306 (GP4/GP5 I2C)

Muestra estado en tiempo real:
- Estado USB (CONECTADO/DESCONECTADO)
- Estado WiFi AP (ACTIVO/INACTIVO)
- Numero de clientes conectados
- IP obtenida por USB

## Limitaciones

- Ancho de banda max: USB FS ~12 Mbps, WiFi ~varios Mbps
- Latencia: ~10-50ms extra
- Clientes max recomendados: 4-8
- HTTPS funciona sin problemas (no MITM, solo forwarding)
- IPv6: no implementado (MVP)

## Estructura del proyecto

```
dongle_wifi_usb/
├── CMakeLists.txt
├── build.sh
├── pico_sdk_import.cmake
├── include/
│   ├── config.h
│   ├── lwipopts.h
│   ├── nat.h
│   ├── dhcp_server.h
│   ├── usb_netif.h
│   ├── wifi_ap.h
│   ├── ssd1306.h
│   └── tusb_config.h
└── src/
    ├── main.c
    ├── nat.c
    ├── dhcp_server.c
    ├── usb_netif.c
    ├── wifi_ap.c
    └── ssd1306.c
```

## Checklist de verificacion

- [ ] Compila sin errores
- [ ] Flashea correctamente (UF2)
- [ ] Pico aparece como dispositivo RNDIS/CDC-ECM en host
- [ ] Host obtiene IP via DHCP en interfaz USB
- [ ] WiFi AP visible con SSID configurado
- [ ] Cliente WiFi conecta y obtiene IP via DHCP (192.168.4.x)
- [ ] Ping 192.168.4.1 desde cliente WiFi
- [ ] Ping 8.8.8.8 desde cliente WiFi
- [ ] Navegacion web HTTP/HTTPS desde cliente WiFi
- [ ] OLED muestra estado correcto
- [ ] Watchdog funciona (reboot si se cuelga)
- [ ] Reconexion automatica si host USB se desconecta/conecta