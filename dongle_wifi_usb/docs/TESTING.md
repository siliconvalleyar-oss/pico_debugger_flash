# Testing Guide - dongle_wifi_usb

## Resumen de Pruebas

| Categoria | Pruebas | Estado |
|-----------|---------|--------|
| Unit Tests | Modulos individuales | Pendiente |
| Integration | USB + WiFi + NAT | Pendiente |
| Stress | Carga continua | Pendiente |
| Regression | Despues de cambios | Pendiente |
| Hardware | Pico W fisica | Pendiente |

---

## 1. Pruebas Unitarias (Por Modulo)

### 1.1 NAT Module (`nat.c`)
```bash
# Compilar test standalone (requiere mock lwIP)
# Verificar:
# - nat_init() inicializa free list 256 entries
# - nat_get_mapped_port() retorna puertos 1024-65535 sin repetir
# - nat_translate_outbound() crea entrada y traduce IP/puerto
# - nat_translate_inbound() encuentra entrada y revierte traduccion
# - Checksums IP/TCP/UDP/ICMP se recalculan correctamente
# - nat_cleanup_expired() elimina entradas > 5 min
# - Tabla maneja colisiones (hash chain)
```

**Casos de Prueba:**
| ID | Entrada | Esperado |
|----|---------|----------|
| NAT-01 | Paquete TCP 192.168.4.10:54321 -> 8.8.8.8:80 | src_ip=USB_IP, src_port=mapped |
| NAT-02 | Paquete UDP 192.168.4.10:1234 -> 1.1.1.1:53 | src_ip=USB_IP, src_port=mapped |
| NAT-03 | Paquete ICMP 192.168.4.10 -> 8.8.8.8 | src_ip=USB_IP, sin puerto |
| NAT-04 | Response inbound dst_port=mapped | dst_ip=192.168.4.10, dst_port=orig |
| NAT-05 | Entry timeout 5 min | Eliminada en cleanup |
| NAT-06 | 256 conexiones simultaneas | Sin crash, oldest evicted |

### 1.2 DHCP Server (`dhcp_server.c`)
```bash
# Verificar:
# - dhcp_server_init() bindea UDP 67
# - DISCOVER -> OFFER con IP del pool
# - REQUEST (IP ofrecida) -> ACK
# - REQUEST (IP distinta) -> NAK
# - RELEASE -> marca lease libre
# - INFORM -> ACK con opciones
# - Lease expiry -> IP liberada
# - Opciones: mask, router, DNS, lease time, server ID
```

**Casos de Prueba:**
| ID | Entrada | Esperado |
|----|---------|----------|
| DHCP-01 | DISCOVER broadcast | OFFER unicast con IP 192.168.4.2 |
| DHCP-02 | REQUEST con IP 4.2 | ACK con lease 3600s |
| DHCP-03 | REQUEST con IP 4.99 | NAK |
| DHCP-04 | RELEASE | Lease libre |
| DHCP-05 | 16 clientes simultaneos | Todos con IP unica |
| DHCP-06 | Lease expira (mock time) | IP vuelve a pool |

### 1.3 USB Netif (`usb_netif.c`)
```bash
# Verificar:
# - usb_netif_init() registra netif, inicia DHCP client
# - tud_network_recv_cb() -> pbuf_alloc -> netif->input()
# - tud_network_xmit_cb() copia pbuf chain a buffer lineal
# - link_up/down callbacks actualizan netif flags
# - sys_check_timeouts() llamado periodicamente
```

### 1.4 WiFi AP (`wifi_ap.c`)
```bash
# Verificar:
# - wifi_ap_init() -> cyw43_arch_init + enable_ap_mode
# - netif[1] configurado con IP static 192.168.4.1
# - wifi_ap_poll() llama cyw43_arch_poll()
# - Connected clients counter actualizado
```

### 1.5 OLED SSD1306 (`ssd1306.c`)
```bash
# Verificar:
# - ssd1306_init() secuencia comandos correcta
# - draw_pixel/char/string/line/rect funcionan
# - dirty pages solo actualizan cambios
# - show_status() renderiza dashboard completo
# - I2C 400kHz en GP4/GP5
```

---

## 2. Pruebas de Integracion

### 2.1 USB Device Enumeration
```bash
# Objetivo: Pico aparece como dispositivo RNDIS/CDC-ECM valido
# Pasos:
1. Flashear UF2
2. Conectar USB a PC Linux
3. Verificar:
   $ lsusb -v -d 2e8a:000a
   # Debe mostrar:
   #   bDeviceClass: 0xEF (Misc)
   #   bDeviceSubClass: 0x02
   #   bDeviceProtocol: 0x01 (IAD)
   #   Interface 0: CDC Control (RNDIS)
   #   Interface 1: CDC Data (RNDIS)
   
4. dmesg debe mostrar:
   usb 1-1: new full-speed USB device
   usb 1-1: RNDIS device
   usb0: register 'rndis_host' at usb-0000:00:14.0-1
```

### 2.2 USB DHCP Client
```bash
# Objetivo: Pico obtiene IP del host via DHCP en interfaz USB
# Pasos:
1. Host Linux configura sharing de Internet:
   # nm-connection-editor -> "Compartir con otros ordenadores"
   # O: echo 1 > /proc/sys/net/ipv4/ip_forward
   #    iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
   
2. Verificar en Pico (via serial/OLED):
   IP USB: 10.42.0.xxx (o rango host)
   
3. Ping desde Pico a host:
   # En main.c agregar: ping test periodico
```

### 2.3 WiFi AP + DHCP Server
```bash
# Objetivo: Clientes WiFi obtienen IP y config
# Pasos:
1. Escanear WiFi:
   $ nmcli dev wifi list | grep PicoDongle
   # Debe mostrar: PicoDongle  Infra  6  65 Mbit/s  WPA2
   
2. Conectar cliente:
   $ nmcli dev wifi connect PicoDongle password pico1234
   
3. Verificar IP cliente:
   $ ip addr show wlan0
   # inet 192.168.4.x/24
   
4. Verificar gateway/DNS:
   $ ip route | grep default
   # default via 192.168.4.1
   
   $ cat /etc/resolv.conf
   # nameserver 192.168.4.1
```

### 2.4 NAT Forwarding (End-to-End)
```bash
# Objetivo: Cliente WiFi accede a Internet via USB host
# Prerequisitos: 2.1, 2.2, 2.3 OK

# Pruebas basicas:
$ ping 192.168.4.1        # Gateway AP - DEBE FUNCIONAR
$ ping 8.8.8.8            # Google DNS - DEBE FUNCIONAR (NAT)
$ ping google.com         # DNS resolution - DEBE FUNCIONAR

# HTTP/HTTPS:
$ curl -v http://httpbin.org/get
$ curl -v https://httpbin.org/get
$ curl -v https://google.com

# Verificar en OLED:
# Clientes: 1/8
# USB: CONECTADO
# WiFi AP: ACTIVO
```

### 2.5 Multiples Clientes
```bash
# Objetivo: 4+ clientes simultaneos
# Pasos:
1. Conectar 4 dispositivos (phone, laptop, tablet, IoT)
2. Verificar OLED: "Clientes: 4/8"
3. Ejecutar speedtest simultaneo en 2+
4. Verificar no hay colisiones IP
5. Verificar NAT table no overflow
   # Agregar nat_dump_table() periodico en main.c
```

---

## 3. Pruebas de Estrés (Stress Testing)

### 3.1 Larga Duracion
```bash
# Objetivo: Estabilidad 24h+
# Setup:
- 2 clientes descargando archivos grandes (wget/curl loop)
- 1 cliente ping continuo
- 1 cliente navegacion web automatizada (selenium/playwright)

# Metricas a monitorear:
- Uptime (watchdog no triggereado)
- Memory leaks (heap free estable)
- NAT table entries (cleanup funciona)
- DHCP leases (renewal funciona)
- OLED no se corrompe
- Temperatura Pico (tactil < 50C)
```

### 3.2 Reconexion USB
```bash
# Objetivo: Recuperacion automatica
# Pasos:
1. Cliente conectado, trafico activo
2. Desconectar USB fisico
3. Verificar OLED: "USB: DESCONECTADO"
4. Reconectar USB
5. Verificar:
   - Re-enumeration USB < 3s
   - DHCP renew en host
   - NAT table limpia (entries viejas expiradas)
   - Trafico reanuda automatico
```

### 3.3 Reconexion WiFi
```bash
# Objetivo: Cliente reconecta sin reiniciar Pico
# Pasos:
1. Cliente conectado, trafico
2. Desactivar WiFi en cliente (modo avion)
3. Esperar 30s
4. Reactivar WiFi
6. Verificar:
   - Re-asociacion automatica
   - DHCP renew (misma IP o nueva)
   - NAT entries viejas limpiadas
   - Trafico reanuda
```

### 3.4 Throughput Test
```bash
# Herramientas: iperf3, speedtest-cli, wget
# Setup: Host PC con iperf3 -s, Cliente WiFi iperf3 -c

# TCP throughput:
$ iperf3 -c 192.168.1.100 -t 30 -P 4
# Esperado: > 5 Mbps (limitado por USB FS 12Mbps)

# UDP throughput:
$ iperf3 -c 192.168.1.100 -u -b 10M -t 30
# Esperado: ~8 Mbps

# Latencia:
$ ping -c 100 8.8.8.8
# rtt min/avg/max/mdev = 20/35/80/15 ms
```

---

## 4. Pruebas de Regresion

### Checklist Pre-Commit
```bash
# Ejecutar antes de cada push:
□ make clean && ./build.sh                    # Compila sin warnings
□ ./build.sh 2>&1 | grep -i error             # Sin errores
□ ls -la build/dongle_wifi_usb.uf2            # UF2 generado
□ arm-none-eabi-size build/dongle_wifi_usb.elf # RAM/Flash usage
```

### Memory Usage Targets
```bash
# arm-none-eabi-size build/dongle_wifi_usb.elf
# text    data     bss     dec     hex filename
# 180000   2000   35000  217000  35170 dongle_wifi_usb.elf

# Targets:
# Flash (text): < 2 MB (2M available)
# RAM (data+bss): < 100 KB (264 KB available)
# Stack margin: > 8 KB
```

### Flash Usage
```bash
# Verificar UF2 size
$ ls -lh build/dongle_wifi_usb.uf2
# ~680 KB esperado

# Picotool info
$ picotool info build/dongle_wifi_usb.uf2
# Program Information
# Name: dongle_wifi_usb
# Features: stdout to USB
# Binary start: 0x10000000
# Binary end: 0x10035000
```

---

## 5. Hardware Testing (Pico W Fisica)

### 5.1 Setup Minimo
```
Pico W
  │
  ├─ USB micro-B -> PC Host
  ├─ GP4 (SDA) -> OLED SDA
  ├─ GP5 (SCL) -> OLED SCL
  ├─ GND -> OLED GND
  ├─ 3V3 -> OLED VCC
  └─ (LED onboard en CYW43)
```

### 5.2 Verificaciones Electricas
```bash
# Multimetro:
# 3V3 pin: 3.3V estable
# VBUS: 5V cuando USB conectado
# GP4/GP5: 3.3V pull-up I2C (~4.7k onboard)

# Osciloscopio (opcional):
# I2C SCL/SDA: 400kHz square wave
# USB D+/D-: eye diagram OK
```

### 5.3 Casos Edge Hardware
| Test | Accion | Esperado |
|------|--------|----------|
| HW-01 | Solo USB (sin OLED) | Funciona, LED blink |
| HW-02 | Solo OLED (sin USB) | OLED muestra "USB: DESCONECTADO" |
| HW-03 | Cortocircuito I2C | No damage, watchdog reboot |
| HW-04 | Desconectar USB en caliente | Recuperacion < 3s |
| HW-05 | Sobrecorriente USB | Host protege, Pico OK |
| HW-06 | ESD en GPIO | Sin latch-up |

---

## 6. Debugging Tools

### 6.1 Serial Debug (USB CDC)
```c
// En main.c, stdio_init_all() habilita USB CDC
printf("Debug: %s\n", "message");
// Ver en host:
# screen /dev/ttyACM0 115200
# minicom -D /dev/ttyACM0 -b 115200
```

### 6.2 OLED Debug
```c
// Agregar en loop Core1:
static int debug_counter = 0;
if (debug_counter++ % 100 == 0) {
    char dbg[32];
    snprintf(dbg, sizeof(dbg), "NAT:%d DHCP:%d", 
             nat_count_active(), dhcp_count_leases());
    ssd1306_draw_string(&display, 0, 56, dbg, true);
}
```

### 6.3 Nat Dump
```c
// En main.c Core1 loop:
if (debug_dump_nat) {
    nat_dump_table();
    debug_dump_nat = false;
}
// Trigger via serial command
```

### 6.4 Picotool Debug
```bash
# Info dispositivo
picotool info -a build/dongle_wifi_usb.uf2

# Reboot a BOOTSEL
picotool reboot -b

# Dump flash
picotool save -a -f firmware.bin
```

### 6.5 OpenOCD + GDB
```bash
# Terminal 1: openocd
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg

# Terminal 2: gdb
arm-none-eabi-gdb build/dongle_wifi_usb.elf
(gdb) target remote localhost:3333
(gdb) monitor reset halt
(gdb) break main.c:100
(gdb) continue
```

---

## 7. Test Automation Scripts

### 7.1 test_basic.sh
```bash
#!/bin/bash
# test_basic.sh - Pruebas basicas automatizadas

set -e

echo "=== Test Basico dongle_wifi_usb ==="

# 1. Build
echo "[1/6] Building..."
cd dongle_wifi_usb
./build.sh > /dev/null
echo "  OK"

# 2. Check UF2 exists
echo "[2/6] UF2 exists..."
[ -f build/dongle_wifi_usb.uf2 ] && echo "  OK" || { echo "  FAIL"; exit 1; }

# 3. Check size
echo "[3/6] Size check..."
arm-none-eabi-size build/dongle_wifi_usb.elf | tail -1
RAM=$(arm-none-eabi-size build/dongle_wifi_usb.elf | tail -1 | awk '{print $2+$3}')
[ $RAM -lt 100000 ] && echo "  OK (${RAM} bytes)" || { echo "  FAIL: RAM ${RAM} > 100KB"; exit 1; }

# 4. Check symbols
echo "[4/6] Symbols check..."
arm-none-eabi-nm build/dongle_wifi_usb.elf | grep -q "tud_descriptor_device_cb" && echo "  OK" || { echo "  FAIL: missing descriptors"; exit 1; }

# 5. Check sections
echo "[5/6] Sections check..."
arm-none-eabi-objdump -h build/dongle_wifi_usb.elf | grep -q ".text" && echo "  OK" || { echo "  FAIL"; exit 1; }

# 6. Picotool verify
echo "[6/6] Picotool verify..."
picotool info build/dongle_wifi_usb.uf2 > /dev/null && echo "  OK" || { echo "  FAIL"; exit 1; }

echo ""
echo "=== ALL TESTS PASSED ==="
```

### 7.2 test_hardware.sh (Requiere Hardware)
```bash
#!/bin/bash
# test_hardware.sh - Requiere Pico W fisica conectada

set -e

echo "=== Test Hardware dongle_wifi_usb ==="

# 1. Verificar device USB
echo "[1/5] USB Device..."
lsusb -d 2e8a:000a > /dev/null && echo "  OK" || { echo "  FAIL: No device found"; exit 1; }

# 2. Verificar interfaz red
echo "[2/5] Network Interface..."
sleep 3  # Wait for enumeration
ip addr show | grep -q "usb0\|enx" && echo "  OK" || { echo "  FAIL: No USB network iface"; exit 1; }

# 3. Ping gateway USB (host)
echo "[3/5] USB Gateway..."
ping -c 3 -W 2 $(ip route | grep usb0 | awk '{print $3}') > /dev/null && echo "  OK" || echo "  WARN: No ping to USB host"

# 4. Verificar WiFi AP visible
echo "[4/5] WiFi AP Scan..."
nmcli -t -f SSID dev wifi list | grep -q "PicoDongle" && echo "  OK" || { echo "  FAIL: AP not visible"; exit 1; }

# 5. Conectar y test NAT
echo "[5/5] Connect & NAT Test..."
# Requiere interaccion manual o expect script
echo "  MANUAL: Conectar a PicoDongle/pico1234 y test:"
echo "    ping 192.168.4.1"
echo "    ping 8.8.8.8"
echo "    curl https://google.com"

echo ""
echo "=== HARDWARE TEST COMPLETE ==="
```

---

## 8. Metricas de Exito (KPIs)

| Metrica | Target | Critico |
|---------|--------|---------|
| Compilacion | 0 warnings, 0 errors | SI |
| Flash usage | < 1.5 MB / 2 MB | SI |
| RAM usage | < 100 KB / 264 KB | SI |
| Boot time | < 2s | SI |
| USB enumeration | < 3s | SI |
| WiFi AP up | < 5s | SI |
| DHCP lease time | < 1s | SI |
| NAT throughput | > 5 Mbps TCP | SI |
| Latencia añadida | < 50ms | NO |
| Clientes concurrentes | 4+ estables | SI |
| Uptime | > 24h sin reinicio | SI |
| Watchdog recovery | < 10s | SI |
| OLED refresh | 1Hz estable | NO |

---

## 9. Documentacion de Bugs

### Template Bug Report
```markdown
## Bug: [Titulo corto]

**Severidad**: Critical/High/Medium/Low
**Modulo**: nat/usb_netif/wifi_ap/dhcp/ssd1306/main
**Version**: commit hash

### Descripcion
[Descripcion clara del problema]

### Pasos para Reproducir
1. [Paso 1]
2. [Paso 2]
3. [Paso 3]

### Comportamiento Esperado
[Que deberia pasar]

### Comportamiento Actual
[Que pasa realmente]

### Logs/Output
```
[Pegar logs relevantes]
```

### Configuracion
- Hardware: Pico W / Pico W + OLED
- Host OS: Linux/Windows/macOS
- Pico SDK version: [commit]
- Toolchain: arm-none-eabi-gcc [version]

### Workaround (si existe)
[Solucion temporal]

### Fix Propuesto
[Idea de solucion]
```

---

## 10. Continuous Integration

### GitHub Actions Matrix
```yaml
strategy:
  matrix:
    board: [pico_w]
    build_type: [Release, Debug]
    toolchain: [gcc-13, gcc-12]
```

### Artefacts a Guardar
- `dongle_wifi_usb.uf2` (release)
- `dongle_wifi_usb.elf` (debug symbols)
- `dongle_wifi_usb.map` (memory map)
- `build.log` (full build log)
- `size-report.txt` (arm-none-eabi-size output)

---

*Documento actualizado: 2026-10-04*