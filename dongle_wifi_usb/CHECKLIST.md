# CHECKLIST - Pico 1W USB -> WiFi Dongle

## Compilacion y Flasheo
- [ ] cmake -DPICO_BOARD=pico_w ..  (sin errores)
- [ ] make -j$(nproc)  (compila completo)
- [ ] Genera dongle_wifi_usb.uf2
- [ ] Flasheo modo BOOTSEL funciona
- [ ] Pico arranca y muestra logo en OLED

## USB RNDIS/CDC-ECM
- [ ] Host detecta dispositivo USB (lsusb)
- [ ] Interfaz de red usb0/enp0s20f0u1 aparece en host
- [ ] Host obtiene IP via DHCP en interfaz USB
- [ ] Ping desde host a IP de la Pico (USB) funciona
- [ ] Reconexion automatica al desenchufar/enchufar USB

## WiFi Access Point
- [ ] AP visible en escaneo WiFi (SSID: PicoDongle)
- [ ] Cliente conecta con password (pico1234)
- [ ] Cliente obtiene IP via DHCP (192.168.4.2 - 192.168.4.20)
- [ ] Gateway configurable: 192.168.4.1
- [ ] DNS configurable: 192.168.4.1 (o DNS del host USB)

## NAT / Forwarding
- [ ] Ping 192.168.4.1 desde cliente WiFi -> OK
- [ ] Ping 8.8.8.8 desde cliente WiFi -> OK
- [ ] Ping google.com desde cliente WiFi -> OK (DNS)
- [ ] Navegacion HTTP (curl http://example.com) -> OK
- [ ] Navegacion HTTPS (curl https://google.com) -> OK
- [ ] Multiples clientes simultaneos (2-4) -> OK
- [ ] Tabla NAT se limpia entradas expiradas

## OLED SSD1306 (I2C GP4/GP5)
- [ ] Inicializa correctamente
- [ ] Muestra "Pico WiFi Dongle" al arrancar
- [ ] Estado USB: CONECTADO/DESCONECTADO
- [ ] Estado WiFi AP: ACTIVO/INACTIVO
- [ ] Contador de clientes actualizado
- [ ] IP USB mostrada correctamente
- [ ] Refresh ~1Hz sin parpadeo

## Watchdog y Robustez
- [ ] Watchdog habilitado (8s timeout)
- [ ] Watchdog se refresca en loop principal
- [ ] Reboot por watchdog detectado al reinicio
- [ ] Reconexion WiFi si se cae
- [ ] Reconexion USB si host reinicia

## Rendimiento
- [ ] Throughput > 5 Mbps (USB FS limit ~12 Mbps)
- [ ] Latencia < 50ms extra vs conexion directa
- [ ] CPU usage < 80% en core1
- [ ] Memoria lwIP estable (sin leaks)

## Pruebas de Estrés
- [ ] 8 clientes concurrentes descargando
- [ ] Transferencia continua 10 min sin errores
- [ ] Desconexion/reconexion rapida USB
- [ ] Cambio de canal WiFi en caliente

## Documentacion
- [ ] README.md completo
- [ ] Configuracion en config.h documentada
- [ ] Comandos de compilacion/flasheo claros
- [ ] Troubleshooting basico