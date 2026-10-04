# Troubleshooting Guide - dongle_wifi_usb

## Indice Rapido

1. [Build Errors](#build-errors)
2. [USB No Enumeration](#usb-no-enumeration)
3. [USB Enumerates But No Network](#usb-enumerates-but-no-network)
4. [WiFi AP Not Visible](#wifi-ap-not-visible)
5. [WiFi Connects But No IP](#wifi-connects-but-no-ip)
6. [NAT Not Working](#nat-not-working)
7. [OLED Not Working](#oled-not-working)
8. [Watchdog Reboots](#watchdog-reboots)
9. [Performance Issues](#performance-issues)
10. [Debug Tips](#debug-tips)

---

## Build Errors

### Error: "pico-sdk not found"
```
CMake Error at CMakeLists.txt:3 (include):
  include could not find load file:
    pico_sdk_import.cmake
```
**Solucion:**
```bash
export PICO_SDK_PATH=/ruta/a/pico-sdk
# O editar pico_sdk_import.cmake con path correcto
```

### Error: "lwIP submodule has not been initialized"
```
WARNING: LWIP submodule has not been initialized; Pico W wireless support will be unavailable
```
**Solucion:**
```bash
cd $PICO_SDK_PATH
git submodule update --init lib/lwip
git submodule update --init lib/tinyusb
git submodule update --init lib/cyw43-driver
```

### Error: "arch/cc.h: No such file or directory"
```
fatal error: arch/cc.h: No such file or directory
```
**Causa:** lwIP no encuentra arch/cc.h del pico-sdk
**Solucion:** Usar `pico_cyw43_arch_lwip_poll` en CMakeLists.txt (incluye lwIP con arch correcto)
```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE
    pico_cyw43_arch_lwip_poll  # En lugar de pico_lwip + pico_cyw43_arch
)
```

### Error: "CFG_TUD_OS redefined"
```
error: "CFG_TUD_OS" redefined [-Werror]
```
**Causa:** Definido en tusb_config.h y en pico-sdk
**Solucion:** Remover de `include/tusb_config.h`:
```c
// ELIMINAR esta linea:
#define CFG_TUD_OS OPT_OS_NONE
```

### Error: "pico_get_random undefined reference"
```
/usr/lib/gcc/.../ld: undefined reference to `pico_get_random'
```
**Causa:** lwIP usa `LWIP_RAND` pero no esta linkeado pico_rand
**Solucion:**
```cmake
# CMakeLists.txt
target_link_libraries(${PROJECT_NAME} PRIVATE
    pico_rand  # AGREGAR
)
```
```c
// lwipopts.h
#define LWIP_RAND get_rand_32  // Cambiar de pico_get_random
```

### Error: "rndis_class_set_handler undefined"
```
undefined reference to `rndis_class_set_handler'
```
**Causa:** Falta source de networking de TinyUSB
**Solucion:**
```cmake
set(SOURCES
    ...
    ${PICO_TINYUSB_PATH}/lib/networking/rndis_reports.c
)
```

### Error: "tud_descriptor_* defined but not used"
```
error: 'tud_descriptor_device_cb' defined but not used [-Werror=unused-function]
```
**Causa:** Callbacks TinyUSB deben estar en file scope con `__attribute__((used))`
**Solucion:**
```c
// main.c - file scope, NO dentro de main()
const uint8_t * __attribute__((used)) tud_descriptor_device_cb(void) { ... }
const uint8_t * __attribute__((used)) tud_descriptor_configuration_cb(uint8_t index) { ... }
const uint16_t * __attribute__((used)) tud_descriptor_string_cb(uint8_t index, uint16_t langid) { ... }
```

### Error: "ISO C forbids conversion of function pointer"
```
error: ISO C forbids conversion of function pointer to object pointer type [-Werror=pedantic]
```
**Causa:** `-Wpedantic` en pico-sdk irq.c
**Solucion:** Remover `-Wpedantic` de CMakeLists.txt:
```cmake
add_compile_options(-Wall -Wextra -Werror)  # Sin -Wpedantic
```

### Error: "stdio USB configured with TinyUSB but CDC not enabled"
```
#warning stdio USB was configured along with user use of TinyUSB device mode, but CDC is not enabled [-Werror=cpp]
```
**Causa:** pico_stdio_usb requiere CDC habilitado
**Solucion:** En `tusb_config.h`:
```c
#define CFG_TUD_CDC 1
#define CFG_TUD_CDC_RX_BUFSIZE 256
#define CFG_TUD_CDC_TX_BUFSIZE 256
```

### Error: "LWIP_HOOK_FILENAME lwip_hooks.h not found"
```
fatal error: lwip_hooks.h: No such file or directory
```
**Solucion:** Remover de `lwipopts.h`:
```c
// ELIMINAR:
#define LWIP_HOOK_FILENAME "lwip_hooks.h"
```

### Error: "LWIP_HOOK_IP4_ROUTE_SRC makes pointer from integer"
```
error: initialization of 'struct netif *' from 'int' makes pointer from integer without a cast
```
**Solucion:** Remover de `lwipopts.h`:
```c
// ELIMINAR:
#define LWIP_HOOK_IP4_ROUTE_SRC(dest, src) ip4_route_src_hook(dest, src)
```

### Error: "NETIF API requires NO_SYS=0"
```
#error "If you want to use NETIF API, you have to define NO_SYS=0 in your lwipopts.h"
```
**Solucion:** En `lwipopts.h`:
```c
#define LWIP_NETIF_API 0  // Cambiar de 1 a 0
```

---

## USB No Enumeration

### Sintomas
- `lsusb` no muestra dispositivo
- `dmesg` no muestra nuevos dispositivos USB
- LED parpadeo rapido (sin USB) nunca cambia a lento

### Diagnostico
```bash
# 1. Verificar hardware
lsusb -t
# Debe aparecer dispositivo en puerto USB

# 2. Verificar modo BOOTSEL accidental
lsusb | grep "2e8a:0003"  # RP2040 BOOTSEL mode
# Si aparece, Pico esta en modo BOOTSEL, no ejecutando firmware

# 3. Verificar pines USB
# GP0/GP1 no deben usarse (son USB D+/D-)
# Verificar cortocircuitos en PCB

# 4. Verificar reloj USB
# Requiere cristal 12MHz o reloj interno configurado
```

### Soluciones Comunes
| Causa | Solucion |
|-------|----------|
| Pico en BOOTSEL | Desconectar, esperar 1s, reconectar SIN presionar BOOTSEL |
| Cable USB malo | Cambiar cable (algunos solo cargan, no datos) |
| Hub USB incompatible | Conectar directo a PC |
| Firmware corrupto | Reflashear UF2 en modo BOOTSEL |
| USB suspend | Host no envia SET_CONFIGURATION; verificar `pico_stdio_usb` |

---

## USB Enumerates But No Network

### Sintomas
- `lsusb` muestra "Pico WiFi Dongle"
- `dmesg` muestra RNDIS/CDC-ECM
- Pero `ip addr` NO muestra interfaz usb0/enx...
- Host no obtiene IP via DHCP

### Diagnostico
```bash
# 1. Verificar driver host
dmesg -T | grep -i rndis
# rndis_host 1-1:1.0 usb0: register 'rndis_host' at usb-xxxx

# 2. Verificar interfaz
ip link show usb0
# Debe estar UP, LOWER_UP

# 3. Verificar DHCP client en host
sudo dhclient -v usb0
# Debe mostrar DHCPDISCOVER, DHCPOFFER, DHCPREQUEST, DHCPACK

# 4. Verificar en Pico (serial/OLED)
# IP USB debe mostrar algo != 0.0.0.0
```

### Causas y Soluciones
| Causa | Solucion |
|-------|----------|
| Host no tiene DHCP client | Instalar `dhcpcd` o `isc-dhcp-client` |
| NetworkManager interfiere | `nmcli dev set usb0 managed no` |
| Firewall bloquea DHCP | Permitir UDP 67/68 |
| USB gadget mode no configurado | Verificar `tud_network_init_cb()` llamado |
| TinyUSB callbacks no registrados | Verificar `__attribute__((used))` en callbacks |

---

## WiFi AP Not Visible

### Sintomas
- Escaneo WiFi no muestra "PicoDongle"
- `nmcli dev wifi list` no lo lista
- OLED muestra "WiFi AP: INACTIVO"

### Diagnostico
```bash
# 1. Verificar cyw43 init
# En serial debug:
printf("cyw43_arch_init: %d\n", cyw43_arch_init_with_country(0xFFFFFFFF));

# 2. Verificar AP enable
cyw43_arch_enable_ap_mode("PicoDongle", "pico1234", 2);

# 3. Verificar netif[1]
struct netif *ap = &cyw43_state.netif[1];
printf("AP up: %d, flags: 0x%X\n", netif_is_up(ap), ap->flags);

# 3. Verificar canal y potencia
# Canal 6 por defecto, 20dBm max
```

### Causas y Soluciones
| Causa | Solucion |
|-------|----------|
| cyw43_arch_init falla | Verificar country code (0xFFFFFFFF = worldwide) |
| Antena no conectada | Pico W tiene antena onboard, no necesita externa |
| Region regulatory | Usar `CYW43_COUNTRY_WORLDWIDE` (0xFFFFFFFF) |
| Flash corrupto cyw43 | Reflashear firmware cyw43 (incluido en UF2) |
| `LWIP_NETIF_API=1` con NO_SYS=1 | Cambiar a 0 en lwipopts.h |

---

## WiFi Connects But No IP

### Sintomas
- Cliente conecta a "PicoDongle" (WPA2 OK)
- Pero se queda "Obteniendo direccion IP..."
- O obtiene IP 169.254.x.x (APIPA)

### Diagnostico
```bash
# 1. Verificar DHCP server en Pico
# Agregar debug en dhcp_server.c dhcp_recv():
printf("DHCP recv: type=%d, xid=0x%X\n", msg_type, msg->xid);

# 2. Verificar netif[1] config
printf("AP IP: %s\n", ip4addr_ntoa(&ap_netif->ip_addr));
printf("AP Mask: %s\n", ip4addr_ntoa(&ap_netif->netmask));
printf("AP GW: %s\n", ip4addr_ntoa(&ap_netif->gw));

# 3. Verificar DHCP pool
# IPs 192.168.4.2 - 192.168.4.20
```

### Causas y Soluciones
| Causa | Solucion |
|-------|----------|
| DHCP server no iniciado | Verificar `dhcp_server_init()` llamado despues de `wifi_ap_init()` |
| netif[1] sin IP static | Verificar `netif_set_ipaddr/gw/netmask` en `wifi_ap_init()` |
| UDP port 67 no bindeado | Verificar `udp_bind(dhcp_pcb, IP_ANY_TYPE, 67)` |
| Firewall en cliente | Desactivar firewall temporalmente |
| Cliente espera DHCPv6 | Desactivar IPv6 en cliente |

---

## NAT Not Working

### Sintomas
- Cliente WiFi tiene IP 192.168.4.x
- Ping a 192.168.4.1 OK
- Ping a 8.8.8.8 FALLA (timeout)
- Navegacion web no funciona

### Diagnostico
```bash
# 1. Verificar IP forwarding en lwIP
#define IP_FORWARD 1  // En lwipopts.h

# 2. Verificar netif[0] (USB) tiene IP valida
printf("USB IP: %s\n", ip4addr_ntoa(&usb_netif.netif.ip_addr));

# 3. Verificar NAT table
nat_dump_table();
# Debe mostrar entries activas

# 4. Verificar checksums
# Agregar debug en nat_translate_outbound/inbound
printf("NAT out: %s:%d -> %s:%d (mapped:%d)\n", ...);

# 5. Verificar host tiene Internet y forwarding
# En host Linux:
sysctl net.ipv4.ip_forward=1
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
```

### Causas y Soluciones
| Causa | Solucion |
|-------|----------|
| `IP_FORWARD=0` | Poner `1` en `lwipopts.h` |
| USB netif sin IP | Esperar DHCP client en USB; verificar host tiene DHCP server |
| NAT table llena | Aumentar `NAT_MAX_ENTRIES` o reducir timeout |
| Checksums incorrectos | Verificar `update_tcp/udp/icmp_checksum` |
| Host sin IP forwarding | `sysctl -w net.ipv4.ip_forward=1` + iptables MASQUERADE |
| MTU mismatch | Verificar MTU 1500 en ambas interfaces |

---

## OLED Not Working

### Sintomas
- Pantalla apagada
- Basura en pantalla
- Solo primera linea visible
- I2C error

### Diagnostico
```bash
# 1. Verificar conexiones I2C
# GP4 = SDA, GP5 = SCL
# Pull-ups 4.7k a 3V3 (necesarios si cable largo)

# 2. Scan I2C
# En Pico:
i2c_init(i2c0, 400000);
for (int addr = 8; addr < 120; addr++) {
    int ret = i2c_write_blocking(i2c0, addr, NULL, 0, false);
    if (ret >= 0) printf("Found: 0x%02X\n", addr);
}
# Debe encontrar 0x3C

# 3. Verificar alimentacion
# OLED VCC a 3V3 (no 5V!)
# GND comun

# 4. Verificar reset
# SSD1306 no tiene pin reset, usa comando 0xAE/0xAF
```

### Causas y Soluciones
| Causa | Solucion |
|-------|----------|
| Direccion I2C incorrecta | SSD1306 tipico 0x3C (algunos 0x3D) |
| Sin pull-ups I2C | Agregar 4.7k SDA/SCL a 3V3 |
| Velocidad I2C muy alta | Bajar a 100kHz: `i2c_init(i2c0, 100000)` |
| Alimentacion 5V en VCC | Conectar VCC a 3V3 |
| Cableado SDA/SCL invertido | GP4=SDA, GP5=SCL |
| OLED defectuoso | Probar con otro modulo |

---

## Watchdog Reboots

### Sintomas
- Pico se reinicia cada ~8 segundos
- Mensaje "Rebooted by watchdog" en serial
- OLED se reinicia constantemente

### Diagnostico
```bash
# 1. Verificar watchdog_update() en loop
# Core 0 y Core 1 deben llamarlo

# 2. Verificar loop no se bloquea
# sleep_ms() en loop principal
# No bucles infinitos sin watchdog_update()

# 3. Verificar tiempo de ejecucion
# watchdog_enable(8000, 1) = 8 segundos
# Loop debe iterar < 8s

# 4. Debug: aumentar timeout temporal
watchdog_enable(30000, 1);  // 30s para debug
```

### Causas y Soluciones
| Causa | Solucion |
|-------|----------|
| Loop principal bloqueado | Agregar `sleep_ms(10)` en while(true) |
| `tud_task()` nunca retorna | Verificar `CFG_TUD_TASK_QUEUE_SZ` suficiente |
| `cyw43_arch_poll()` bloquea | Verificar interrupciones habilitadas |
| Core 1 no llama watchdog_update | Agregar en loop Core 1 |
| Stack overflow | Aumentar stack size o reducir variables locales |

---

## Performance Issues

### Sintomas
- Throughput < 2 Mbps
- Latencia > 100ms
- Paquetes perdidos
- Clientes se desconectan

### Diagnostico
```bash
# 1. Verificar MTU
# Ambas interfaces MTU=1500

# 2. Verificar buffer sizes
#define PBUF_POOL_SIZE 64
#define PBUF_POOL_BUFSIZE 1520
#define TCP_SND_BUF (4 * TCP_MSS)
#define TCP_WND (4 * TCP_MSS)

# 3. Verificar CPU usage
# Core 1 loop < 5ms por iteracion
# Si > 10ms, hay bottleneck

# 4. Verificar memoria
#define MEM_SIZE (16 * 1024)  // Aumentar si necesario
#define MEMP_NUM_PBUF 128     // Aumentar si pool agotado
```

### Optimizaciones
| Area | Mejora |
|-------|--------|
| TCP Window | Aumentar `TCP_WND` a 8*MSS |
| PBUF Pool | Aumentar `PBUF_POOL_SIZE` a 128 |
| MEM_SIZE | Aumentar a 32KB si RAM permite |
| TCP MSS | 1460 OK para Ethernet |
| NAT entries | Aumentar `NAT_MAX_ENTRIES` a 512 |
| DHCP lease | Reducir si muchos clientes temporales |

---

## Debug Tips

### 1. Serial Debug Output
```c
// En main.c, stdio_init_all() habilita USB CDC
// Ver en host:
screen /dev/ttyACM0 115200
# O:
minicom -D /dev/ttyACM0 -b 115200
```

### 2. OLED Debug Mode
```c
// Agregar en loop Core1:
static int dbg_sec = 0;
if (++dbg_sec >= 100) {  // cada ~1s
    dbg_sec = 0;
    char buf[64];
    snprintf(buf, sizeof(buf), "NAT:%d DHCP:%d USB:%s",
             nat_count_active(),
             dhcp_count_leases(),
             usb_connected ? "UP" : "DOWN");
    ssd1306_draw_string(&display, 0, 56, buf, true);
    ssd1306_update(&display);
}
```

### 3. NAT Table Dump
```c
// Trigger via serial command 'n'
if (serial_cmd == 'n') {
    nat_dump_table();
}
// Output ejemplo:
// === NAT Table ===
// Active entries:
//   192.168.4.10:54321 -> 8.8.8.8:80 (mapped:1024) proto:6 age:1234ms
//   192.168.4.11:12345 -> 1.1.1.1:53 (mapped:1025) proto:17 age:567ms
// =================
```

### 4. Packet Capture (Host Side)
```bash
# Capturar trafico USB RNDIS
sudo tcpdump -i usb0 -w capture.pcap
# Analizar en Wireshark
# Filtrar: usb.rndis
```

### 5. lwIP Stats
```c
// Habilitar en lwipopts.h:
#define LWIP_STATS 1
#define LWIP_STATS_DISPLAY 1

// Imprimir periodico:
stats_display();
```

### 6. cyw43 Debug
```c
// En lwipopts.h o CMakeLists.txt:
#define PICO_CYW43_ARCH_DEBUG_ENABLED 1

// En cyw43_arch.h:
#define CYW43_DEBUG 1
// Output: [cyw43] mensajes detallados
```

### 7. TinyUSB Debug
```c
// En tusb_config.h:
#define CFG_TUSB_DEBUG 2

// Output: [TUSB] mensajes detallados
```

### 8. Memory Debug
```c
// Ver heap libre
extern char _end;
extern char _estack;
uint32_t heap_free = &_estack - &_end - malloc_max_total_mem();
printf("Heap free: %d bytes\n", heap_free);

// Stack usage (Core 1)
uint32_t stack_used = &_estack - __get_MSP();
printf("Stack used: %d bytes\n", stack_used);
```

### 9. Timing Debug
```c
// Medir loop time
absolute_time_t t0 = get_absolute_time();
// ... loop body ...
absolute_time_t t1 = get_absolute_time();
int64_t loop_us = absolute_time_diff_us(t0, t1);
if (loop_us > 10000) printf("SLOW LOOP: %lld us\n", loop_us);
```

### 10. Picotool Commands
```bash
# Info dispositivo
picotool info -a build/dongle_wifi_usb.uf2

# Reboot a BOOTSEL
picotool reboot -b

# Dump flash
picotool save -a -f firmware.bin

# Ver GPIO state
picotool gpio get

# Ver clocks
picotool clock dump
```

---

## Contacto y Recursos

### Repositorios
- pico-sdk: https://github.com/raspberrypi/pico-sdk
- TinyUSB: https://github.com/hathach/tinyusb
- lwIP: https://savannah.nongnu.org/projects/lwip/
- cyw43-driver: https://github.com/raspberrypi/cyw43-driver

### Documentacion Clave
- RP2040 Datasheet: https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
- Pico W Datasheet: https://datasheets.raspberrypi.com/picow/pico-w-datasheet.pdf
- USB RNDIS Spec: https://www.usb.org/document-library/usb-rndis
- lwIP Wiki: https://www.nongnu.org/lwip/2_1_x/

### Reportar Issues
Si encuentras un bug no documentado aqui:
1. Revisar [GitHub Issues](https://github.com/siliconvalleyar-oss/pico_debugger_flash/issues)
2. Crear nuevo issue con template de [BUG_REPORT.md](BUG_REPORT.md)
3. Incluir: logs, configuracion, pasos para reproducir

---

*Ultima actualizacion: 2026-10-04*
*Version: dongle_wifi_usb v0.1*