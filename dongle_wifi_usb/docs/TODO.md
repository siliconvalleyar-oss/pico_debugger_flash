# TODO - dongle_wifi_usb

## Estado General
- [x] Proyecto base creado y compilando
- [x] USB RNDIS/ECM funcionando
- [x] WiFi AP (cyw43_arch) funcionando
- [x] NAT bidireccional implementado
- [x] DHCP server en AP
- [x] OLED SSD1306 (I2C GP4/GP5) funcionando
- [x] Multicore (Core0: status/LED, Core1: network loop)
- [x] Watchdog habilitado
- [x] Build system CMake funcional
- [x] UF2 generado correctamente

---

## Pendientes por Prioridad

### Alta Prioridad (Core Functionality)
- [ ] **Verificacion hardware**: Flashear UF2 en Pico W fisica y validar
  - [ ] USB RNDIS detectado en host Linux/Windows/macOS
  - [ ] Host obtiene IP via DHCP en interfaz USB
  - [ ] WiFi AP visible con SSID "PicoDongle"
  - [ ] Cliente WiFi conecta y obtiene IP (192.168.4.x)
  - [ ] Ping 192.168.4.1 desde cliente WiFi
  - [ ] Ping 8.8.8.8 desde cliente WiFi (NAT working)
  - [ ] Navegacion HTTP/HTTPS desde cliente WiFi
  - [ ] OLED muestra estado correcto

- [ ] **Stress testing**
  - [ ] 4 clientes concurrentes descargando
  - [ ] Transferencia continua 30 min sin memory leaks
  - [ ] Desconexion/reconexion rapida USB
  - [ ] Reconexion automatica si host USB reinicia

- [ ] **DHCP Server robustez**
  - [ ] Lease renewal antes de expiracion
  - [ ] DHCP RELEASE handling
  - [ ] DHCP INFORM para clientes con IP estatica
  - [ ] Colisiones de IP (duplicate detection)

### Media Prioridad (Features)
- [ ] **DNS Forwarder**
  - [ ] Interceptar queries DNS en AP (puerto 53)
  - [ ] Forward a DNS del host USB
  - [ ] Cache basico (TTL respect)

- [ ] **Configuracion persistente (Flash)**
  - [ ] Guardar SSID/password en flash
  - [ ] Menu configuracion via USB CDC (serial)
  - [ ] Factory reset (BOOTSEL + boton)

- [ ] **Estadisticas y Monitoring**
  - [ ] Contadores bytes/packets RX/TX por interfaz
  - [ ] Tabla NAT visible via serial/OLED
  - [ ] Uptime, clientes historicos

- [ ] **Power Management**
  - [ ] Sleep mode cuando no hay clientes
  - [ ] Wake on USB activity
  - [ ] Medir consumo real (mA)

### Baja Prioridad (Nice to Have)
- [ ] **IPv6 Support**
  - [ ] lwIP IPv6 habilitado
  - [ ] NDP/Router Advertisement
  - [ ] DHCPv6 en AP

- [ ] **Web UI Configuracion**
  - [ ] HTTP server en AP (puerto 80)
  - [ ] Formulario SSID/password/canal
  - [ ] Status page con graficos

- [ ] **Multiple USB Gadget Modes**
  - [ ] CDC-ECM + RNDIS simultaneo
  - [ ] MSC (mass storage) para logs
  - [ ] HID para configuracion via teclas

- [ ] **Bandwidth Shaping**
  - [ ] Token bucket por cliente
  - [ ] Prioridad ICMP/DNS sobre bulk

---

## Bugs Conocidos / Investigar

- [ ] **NAT ICMP**: Verificar que ping responses vuelven correctamente
- [ ] **TCP window scaling**: No implementado, limita throughput en alta latencia
- [ ] **Fragmentation**: IP_FRAG=1 pero no probado con paquetes > MTU
- [ ] **ARP cache**: No hay aging configurado, posible memory leak
- [ ] **DHCP race condition**: Si 2 clientes piden IP simultaneamente
- [ ] **USB enumeration fix**: PICO_FIX_RP2040_USB_DEVICE_ENUMERATION=1 necesario?

---

## Testing Checklist (Pre-Release)

### Funcionalidad Basica
- [ ] Compila sin warnings (`-Wall -Wextra -Werror`)
- [ ] Flashea via UF2 (BOOTSEL mode)
- [ ] Arranca y muestra "Pico WiFi Dongle" en OLED
- [ ] LED parpadena: rapido=sin USB, lento=USB conectado

### USB Device
- [ ] `lsusb` muestra "Raspberry Pi Pico WiFi Dongle"
- [ ] `dmesg` muestra interfaz RNDIS/CDC-ECM
- [ ] Host Linux: `ip addr show usb0` -> IP asignada
- [ ] Host Windows: "RNDIS" en Administrador de dispositivos
- [ ] Host macOS: "RNDIS/Ethernet Gadget" en Network preferences

### WiFi AP
- [ ] Escaneo muestra "PicoDongle" (canal 6)
- [ ] Conexion WPA2-PSK "pico1234"
- [ ] Cliente obtiene IP 192.168.4.2-20
- [ ] Gateway 192.168.4.1 responde ping
- [ ] DNS 192.168.4.1 resuelve nombres

### NAT/Forwarding
- [ ] `ping 8.8.8.8` desde cliente -> OK
- [ ] `ping google.com` -> OK (DNS)
- [ ] `curl http://httpbin.org/get` -> OK
- [ ] `curl https://google.com` -> OK (HTTPS passthrough)
- [ ] Múltiples pestañas navegador -> OK

### Multiples Clientes
- [ ] 2 smartphones conectados simultaneamente
- [ ] 1 laptop + 1 phone
- [ ] Descarga paralela (speedtest)
- [ ] Reconexion despues de sleep telefono

### Edge Cases
- [ ] Desconectar USB host -> AP sigue? Reconecta al volver?
- [ ] Apagar WiFi telefono -> DHCP lease expira -> limpieza NAT?
- [ ] Reiniciar host PC -> Pico detecta link down/up?
- [ ] Watchdog reboot -> recuperacion limpia?
- [ ] Cambio canal WiFi en caliente (requiere reinicio AP)

---

## Documentacion Pendiente
- [ ] **API.md**: Documentar funciones publicas de cada modulo
- [ ] **ARCHITECTURE.md**: Diagramas de flujo de datos
- [ ] **BUILD.md**: Instrucciones detalladas de compilacion
- [ ] **TESTING.md**: Procedimientos de prueba paso a paso
- [ ] **TROUBLESHOOTING.md**: Problemas comunes y soluciones

---

## Refactoring Tecnico
- [ ] Mover NAT a modulo separado con tests unitarios
- [ ] DHCP server usar lwIP netif API en lugar de raw UDP
- [ ] Unificar usb_netif y wifi_ap en netif_generic
- [ ] Eliminar globals (g_usb_netif, g_wifi_ap) -> pasar contexto
- [ ] Configurar lwIP stats para debugging

---

## Versioning
- [ ] v0.1: MVP funcional (actual)
- [ ] v0.2: DNS forwarder + config persistente
- [ ] v0.3: Web UI + stats + multi-client stress tested
- [ ] v1.0: Release estable con documentacion completa

---

*Ultima actualizacion: 2026-10-04*