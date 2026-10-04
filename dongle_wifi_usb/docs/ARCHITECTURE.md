# Architecture - dongle_wifi_usb

## Diagrama General del Sistema

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         RASPBERRY PI PICO W (RP2040)                         │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐  │
│  │   CORE 0    │    │   CORE 1    │    │   CYW43439  │    │  HARDWARE   │  │
│  ├─────────────┤    ├─────────────┤    ├─────────────┤    ├─────────────┤  │
│  │ main()      │    │ core1_entry │    │  WiFi PHY   │    │  GP4 (SDA)  │  │
│  │  - init     │◄───│  - tud_task │◄───│  802.11 b/g/n│    │  GP5 (SCL)  │  │
│  │  - OLED     │    │  - usb_poll │    │  AP Mode    │    │  GPIO 0     │  │
│  │  - Watchdog │    │  - wifi_poll│    │  (Channel 6)│    │  (LED)      │  │
│  │  - LED      │    │  - nat_tick │    └──────┬──────┘    └─────────────┘  │
│  │  - multicore│    │  - dhcp_tick│           │                               │
│  │  - LED blink│    │  - sys_check│           ▼                               │
│  └──────┬──────┘    └──────┬──────┘    ┌─────────────────┐                  │
│         │                  │           │   lwIP Stack    │                  │
│         │                  │           │  (NO_SYS=1)     │                  │
│         │                  │           ├─────────────────┤                  │
│         │                  │           │ netif[0] USB    │                  │
│         │                  │           │  - RNDIS/ECM    │                  │
│         │                  │           │  - DHCP Client  │                  │
│         │                  │           │  - etharp       │                  │
│         │                  │           ├─────────────────┤                  │
│         │                  │           │ netif[1] WiFi   │                  │
│         │                  │           │  - AP Mode      │                  │
│         │                  │           │  - Static IP    │                  │
│         │                  │           │  - DHCP Server  │                  │
│         │                  │           └────────┬────────┘                  │
│         │                  │                    │                           │
│         │                  │                    ▼                           │
│         │                  │           ┌─────────────────┐                  │
│         │                  │           │   NAT Table     │                  │
│         │                  │           │  (256 entries)  │                  │
│         │                  │           │  - Outbound:    │                  │
│         │                  │           │    WiFi->USB    │                  │
│         │                  │           │  - Inbound:     │                  │
│         │                  │           │    USB->WiFi    │                  │
│         │                  │           │  - Checksum fix │                  │
│         │                  │           └─────────────────┘                  │
│         │                  │                    │                           │
│         └──────────────────┴────────────────────┘                           │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                    ┌───────────────┴───────────────┐
                    ▼                               ▼
           ┌─────────────────┐             ┌─────────────────┐
           │   USB HOST      │             │  WIFI CLIENTS   │
           │  (PC/Router)    │             │  (Phone/Laptop) │
           ├─────────────────┤             ├─────────────────┤
           │ RNDIS/ECM CDC   │             │ 802.11          │
           │ DHCP Server     │             │ WPA2-PSK        │
           │ Internet Access │             │ DHCP Client     │
           └─────────────────┘             └─────────────────┘
```

---

## Flujo de Paquetes - Outbound (WiFi → Internet)

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  WiFi Client │────►│  cyw43439    │────►│  lwIP netif[1]│────►│  NAT Table   │
│  (192.168.4.x)│    │  (RX packet) │    │  (ethernet_input)    │  (lookup)    │
└──────────────┘     └──────────────┘     └──────────────┘     └──────┬───────┘
                                                                        │
                    ┌──────────────────────────────────────────────────┘
                    ▼
           ┌────────────────┐
           │ NAT Translate  │
           │ Outbound:      │
           │ 1. src_ip =    │
           │    AP_IP (192.168.4.1)    │
           │ 2. src_port =  │
           │    mapped_port │
           │ 3. Fix checksums (IP/TCP/UDP/ICMP) │
           └───────┬────────┘
                   │
                   ▼
           ┌────────────────┐
           │ lwIP netif[0]  │
           │ (USB RNDIS)    │
           │ etharp_output  │
           └───────┬────────┘
                   │
                   ▼
           ┌────────────────┐
           │ TinyUSB RNDIS  │
           │ tud_network_   │
           │ xmit_cb()      │
           └───────┬────────┘
                   │
                   ▼
           ┌────────────────┐
           │ USB PHY        │
           │ EP1 IN (Bulk)  │
           └───────┬────────┘
                   │
                   ▼
           ┌────────────────┐
           │ Host PC        │
           │ usb0 / RNDIS   │
           │ Internet       │
           └────────────────┘
```

### Detalle NAT Outbound

```
Paquete Original (WiFi side)          Paquete Traducido (USB side)
┌─────────────────────────────┐       ┌─────────────────────────────┐
│ ETH Header                  │       │ ETH Header                  │
│   dst: AP_MAC               │       │   dst: USB_HOST_MAC         │
│   src: CLIENT_MAC           │       │   src: USB_DEVICE_MAC       │
├─────────────────────────────┤       ├─────────────────────────────┤
│ IP Header                   │  ──►  │ IP Header                   │
│   src: 192.168.4.10 (client)│       │   src: 192.168.1.100 (USB)  │
│   dst: 8.8.8.8              │       │   dst: 8.8.8.8              │
│   protocol: TCP (6)         │       │   protocol: TCP (6)         │
│   checksum: 0x1234          │       │   checksum: 0x5678 (NEW)    │
├─────────────────────────────┤       ├─────────────────────────────┤
│ TCP Header                  │       │ TCP Header                  │
│   src_port: 54321           │       │   src_port: 1024 (mapped)   │
│   dst_port: 80              │       │   dst_port: 80              │
│   checksum: 0xABCD          │       │   checksum: 0xEF01 (NEW)    │
├─────────────────────────────┤       ├─────────────────────────────┤
│ Payload                     │       │ Payload (unchanged)         │
└─────────────────────────────┘       └─────────────────────────────┘

NAT Table Entry Created:
┌─────────────────────────────────────────────────────────────────┐
│ src_ip=192.168.4.10  dst_ip=8.8.8.8  src_port=54321  dst_port=80│
│ protocol=TCP  mapped_port=1024  last_seen=now                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Flujo de Paquetes - Inbound (Internet → WiFi)

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  Internet    │────►│  Host PC     │────►│  USB PHY     │────►│  TinyUSB     │
│  (8.8.8.8)   │    │  (USB Host)  │    │  EP1 OUT     │    │  tud_network_│
└──────────────┘     └──────────────┘     └──────────────┘     │  recv_cb()   │
                                                                └──────┬───────┘
                                                                       │
                              ┌───────────────────────────────────────┘
                              ▼
                     ┌────────────────┐
                     │ lwIP netif[0]  │
                     │ (ethernet_input)   │
                     └───────┬────────┘
                             │
                             ▼
                     ┌────────────────┐
                     │ NAT Translate  │
                     │ Inbound:       │
                     │ 1. Find entry  │
                     │    by dst_port │
                     │    (mapped)    │
                     │ 2. dst_ip =    │
                     │    client_ip   │
                     │ 3. dst_port =  │
                     │    original    │
                     │ 4. Fix checksums     │
                     └───────┬────────┘
                             │
                             ▼
                     ┌────────────────┐
                     │ lwIP netif[1]  │
                     │ (WiFi AP)      │
                     │ cyw43_send_    │
                     │ ethernet()     │
                     └───────┬────────┘
                             │
                             ▼
                     ┌────────────────┐
                     │ cyw43439       │
                     │ 802.11 TX      │
                     └───────┬────────┘
                             │
                             ▼
                     ┌────────────────┐
                     │ WiFi Client    │
                     │ (192.168.4.10) │
                     └────────────────┘
```

### Detalle NAT Inbound

```
Paquete Original (USB side)           Paquete Traducido (WiFi side)
┌─────────────────────────────┐       ┌─────────────────────────────┐
│ ETH Header                  │       │ ETH Header                  │
│   dst: USB_DEVICE_MAC       │       │   dst: CLIENT_MAC           │
│   src: USB_HOST_MAC         │       │   src: AP_MAC               │
├─────────────────────────────┤       ├─────────────────────────────┤
│ IP Header                   │  ──►  │ IP Header                   │
│   src: 8.8.8.8              │       │   src: 8.8.8.8              │
│   dst: 192.168.1.100 (USB)  │       │   dst: 192.168.4.10 (client)│
│   protocol: TCP (6)         │       │   protocol: TCP (6)         │
│   checksum: 0x5678          │       │   checksum: 0x9012 (NEW)    │
├─────────────────────────────┤       ├─────────────────────────────┤
│ TCP Header                  │       │ TCP Header                  │
│   src_port: 80              │       │   src_port: 80              │
│   dst_port: 1024 (mapped)   │       │   dst_port: 54321 (orig)    │
│   checksum: 0xEF01          │       │   checksum: 0x3456 (NEW)    │
├─────────────────────────────┤       ├─────────────────────────────┤
│ Payload                     │       │ Payload (unchanged)         │
└─────────────────────────────┘       └─────────────────────────────┘

NAT Table Lookup (by dst_port=mapped_port=1024):
┌─────────────────────────────────────────────────────────────────┐
│ FOUND: src_ip=192.168.4.10  dst_ip=8.8.8.8  src_port=54321     │
│        dst_port=80  protocol=TCP  mapped_port=1024             │
└─────────────────────────────────────────────────────────────────┘
```

---

## Memoria y Recursos

### Mapa de Memoria lwIP
```
MEM_SIZE = 16 KB
├── PBUF_POOL: 64 x 1520 bytes = ~97 KB (from heap)
├── MEMP_NUM_PBUF: 64
├── MEMP_NUM_TCP_PCB: 16
├── MEMP_NUM_TCP_SEG: 32
├── MEMP_NUM_UDP_PCB: 16
├── MEMP_NUM_ARP_QUEUE: 16
└── MEMP_NUM_SYS_TIMEOUT: 16

Total estimado: ~25-30 KB RAM para lwIP
```

### NAT Table
```
256 entries x sizeof(nat_entry_t)
nat_entry_t = 4+4+2+2+2+1+4+1+4 = 24 bytes (padded to 28)
Total: 256 x 28 = 7,168 bytes (~7 KB)
```

### DHCP Leases
```
16 leases x sizeof(dhcp_lease_t)
dhcp_lease_t = 4+6+4+1 = 15 bytes (padded to 16)
Total: 256 bytes
```

### OLED Buffer
```
128 x 64 = 8192 bits = 1024 bytes
dirty[8] = 8 bytes
Total: ~1 KB
```

### Total RAM Estimado
```
lwIP:        ~25 KB
NAT:         ~7 KB
DHCP:        ~0.25 KB
OLED:        ~1 KB
Stack Core0: ~2 KB
Stack Core1: ~4 KB
Heap/Other:  ~4 KB
─────────────────────────────
TOTAL:       ~39 KB / 264 KB (RP2040)
```

---

## Timing y Polling

### Loop Principal Core 1 (cada ~10ms)
```
┌─────────────────────────────────────────────────────────────┐
│ while (true) {                                              │
│   tud_task();              // ~1ms  - TinyUSB device task   │
│   usb_netif_poll();        // ~0.5ms - USB RX/TX            │
│   wifi_ap_poll();          // ~1ms  - cyw43_arch_poll()     │
│   nat_tick();              // ~0.1ms - cleanup cada 60s     │
│   dhcp_server_tick();      // ~0.1ms - lease expiry check   │
│                                                          │
│   if (1s elapsed) {        // Status update                 │
│     update OLED            // ~2ms I2C                      │
│   }                                                          │
│   if (blink interval) {    // LED                           │
│     gpio_put()             // ~0.01ms                       │
│   }                                                          │
│   watchdog_update();       // ~0.01ms                       │
│   sleep_ms(10);            // ~10ms                         │
│ }                                                          │
└─────────────────────────────────────────────────────────────┘
```

### cyw43_arch_poll() Timing
- Debe llamarse al menos cada **10ms** para WiFi
- Maneja: beacon TX, probe response, association, data TX/RX
- En AP mode: beacon cada 100ms (10 beacons/seg)

### USB Polling
- `tud_task()`: maneja control endpoints, bulk endpoints
- `tud_network_recv_cb()`: llamado desde ISR USB cuando hay datos
- `tud_network_xmit()`: no-blocking, usa double buffering

---

## Inicializacion - Secuencia de Arranque

```
POWER ON / RESET
      │
      ▼
┌─────────────────────┐
│ Boot ROM            │
│  - Core0 starts     │
│  - Core1 waits      │
└─────────┬───────────┘
          │
          ▼
┌─────────────────────┐
│ crt0.S              │
│  - Zero BSS         │
│  - Setup stacks     │
│  - Call main()      │
└─────────┬───────────┘
          │
          ▼
┌─────────────────────┐
│ main() - Core 0     │
│  stdio_init_all()   │
│  watchdog_enable()  │
│  gpio_init(LED)     │
│  ssd1306_init()     │
│                     │
│  usb_netif_init()   │────┐
│    netif_add()      │    │
│    dhcp_start()     │    │
│    tud_init()       │    │
└─────────┬───────────┘    │
          │                │
          ▼                │
┌─────────────────────┐    │
│ wifi_ap_init()      │    │
│  cyw43_arch_init()  │    │
│    async_context    │    │
│    cyw43_driver     │    │
│    lwip_nosys_init  │    │
│  enable_ap_mode()   │    │
│  netif config IP    │    │
└─────────┬───────────┘    │
          │                │
          ▼                │
┌─────────────────────┐    │
│ nat_init()          │    │
│ dhcp_server_init()  │    │
│                     │    │
│ multicore_launch_   │    │
│   core1(core1_entry)│    │
└─────────┬───────────┘    │
          │                │
          ▼                │
┌─────────────────────┐    │
│ Core 0 Loop:        │    │ Core 1 Loop:                  │
│  - LED blink        │    │  - tud_task()                 │
│  - watchdog_update  │    │  - usb_netif_poll()           │
│  - sleep_ms(10)     │    │  - wifi_ap_poll()             │
└─────────────────────┘    │  - nat_tick()                 │
                           │  - dhcp_server_tick()         │
                           │  - sys_check_timeouts()       │
                           │  - OLED update (1Hz)          │
                           │  - watchdog_update()          │
                           └───────────────────────────────┘
```

---

## Interfaces de Red

### netif[0] - USB RNDIS/ECM
```
Name: "us" (netif->name[0]='u', [1]='s')
MTU: 1500
Flags: BROADCAST | ETHARP | LINK_UP | UP
HWaddr: 02:00:00:00:00:01
IP: DHCP (assigned by host)
GW: DHCP
DNS: DHCP
Output: etharp_output
LinkOutput: usb_netif_linkoutput() -> tud_network_xmit()
```

### netif[1] - WiFi AP
```
Name: "wl" (netif->name[0]='w', [1]='l')
MTU: 1500
Flags: BROADCAST | ETHARP | UP
HWaddr: 02:00:00:00:00:02
IP: 192.168.4.1 (static)
GW: 192.168.4.1
Mask: 255.255.255.0
DNS: 192.168.4.1
Output: etharp_output
LinkOutput: cyw43_netif_output() -> cyw43_send_ethernet()
```

---

## Thread Safety (NO_SYS=1)

```
┌─────────────────────────────────────────────────────────────┐
│                    SINGLE THREADED MODEL                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Core 0:                  Core 1:                          │
│  ┌─────────────┐          ┌─────────────────────────────┐  │
│  │ main loop   │          │ network loop                │  │
│  │ - LED       │          │ - tud_task()                │  │
│  │ - Watchdog  │          │ - usb_poll()                │  │
│  │ - OLED      │          │ - wifi_poll()               │  │
│  │             │          │ - nat_tick()                │  │
│  │             │          │ - dhcp_tick()               │  │
│  │             │          │ - sys_check_timeouts()      │  │
│  └─────────────┘          └─────────────────────────────┘  │
│                                                             │
│  NO CONCURRENT ACCESS TO lwIP FROM CORE 0                  │
│  (Core 0 solo usa OLED, GPIO, Watchdog - NO lwIP)          │
│                                                             │
│  lwIP NO ES THREAD-SAFE EN NO_SYS=1                        │
│  Todo acceso a lwIP debe ser desde Core 1                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Variables Compartidas (Core 0 ↔ Core 1)
```c
// Core 1 escribe, Core 0 lee (atomic en Cortex-M0+ para 32-bit)
static volatile bool usb_connected;
static volatile bool wifi_ap_active;
static volatile int wifi_clients;
static volatile char ip_str[16];

// Core 0 escribe, Core 1 lee (watchdog)
static volatile bool watchdog_triggered;
```

---

## Checksums en NAT

### Pseudocode update_ip4_checksum
```c
void update_ip4_checksum(void *iphdr) {
    uint16_t *hdr = (uint16_t *)iphdr;  // 10 words de 16-bit = 20 bytes
    uint32_t sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += lwip_ntohs(hdr[i]);      // Suma en host order
    }
    sum = (sum & 0xFFFF) + (sum >> 16); // Fold carry
    sum = (sum & 0xFFFF) + (sum >> 16); // Fold again
    hdr[10] = lwip_htons(~sum & 0xFFFF); // Complemento a 1
}
```

### TCP/UDP/ICMP Pseudo-header Checksum
```
Pseudo-header (12 bytes):
┌─────────────────────────────────────────────────────────┐
│ Source IP (4 bytes)                                     │
│ Dest IP (4 bytes)                                       │
│ Zero (1 byte) | Protocol (1 byte) | TCP/UDP Length (2) │
└─────────────────────────────────────────────────────────┘

Checksum = inet_chksum_pseudo_partial(pbuf, proto, len, src_ip, dst_ip)
```

---

## USB Descriptors (RNDIS + CDC-ECM)

### Device Descriptor
```
bLength: 18
bDescriptorType: 1 (DEVICE)
bcdUSB: 0x0200
bDeviceClass: 0xEF (MISC)
bDeviceSubClass: 0x02 (COMMON)
bDeviceProtocol: 0x01 (IAD)
bMaxPacketSize0: 64
idVendor: 0x2E8A
idProduct: 0x000A
bcdDevice: 0x0100
iManufacturer: 1
iProduct: 2
iSerialNumber: 3
bNumConfigurations: 1
```

### Configuration Descriptor (IAD + 2 Interfaces)
```
Configuration: 9 bytes
  IAD: 8 bytes (2 interfaces, class 0x02/0x06/0x00)
  
Interface 0: CDC Control (RNDIS)
  Interface: 9 bytes (class 0x02, subclass 0x02, protocol 0xFF)
  CDC Header: 5 bytes
  CDC Union: 5 bytes (master=0, slave=1)
  CDC Ethernet: 13 bytes
  Endpoint INT IN: 7 bytes (EP 0x82, interrupt, 8 bytes, 10ms)
  
Interface 1: CDC Data (RNDIS)
  Interface: 9 bytes (class 0x0A, subclass 0x00, protocol 0x00)
  Endpoint BULK OUT: 7 bytes (EP 0x01, bulk, 64 bytes)
  Endpoint BULK IN: 7 bytes (EP 0x81, bulk, 64 bytes)
```

### Endpoints
| EP | Dir | Type | Size | Uso |
|----|-----|------|------|-----|
| 0x00 | OUT | Control | 64 | Device requests |
| 0x00 | IN | Control | 64 | Device responses |
| 0x82 | IN | Interrupt | 8 | CDC Notification |
| 0x01 | OUT | Bulk | 64 | RNDIS Data OUT |
| 0x81 | IN | Bulk | 64 | RNDIS Data IN |

---

## Watchdog

```
┌─────────────────────────────────────────────────────────────┐
│                    WATCHDOG TIMELINE                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  main()          core1_entry()                              │
│     │                │                                      │
│     ▼                ▼                                      │
│  watchdog_       while(true) {                              │
│  enable(8000)        │                                      │
│     │              tud_task()                               │
│     │              usb_poll()                               │
│     │              wifi_poll()                              │
│     │              nat_tick()                               │
│     │              dhcp_tick()                              │
│     │              │                                        │
│     │              if (1s) { OLED }                         │
│     │              if (blink) { LED }                       │
│     │              │                                        │
│     │           watchdog_update() ◄── Refresh cada ~10ms   │
│     │              │                                        │
│     │              sleep_ms(10)                             │
│     │              }                                        │
│     │                │                                      │
│     │                │ (si loop se cuelga > 8s)             │
│     │                ▼                                      │
│     │           REBOOT (hardware)                           │
│     │                │                                      │
│     ▼                │                                      │
│  watchdog_         (reinicio)                               │
│  caused_reboot()   │                                        │
│     │                │                                      │
│     ▼                ▼                                      │
│  printf("Rebooted") │                                      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Estados del Sistema

### USB States
```
DISCONNECTED → ATTACHED → POWERED → DEFAULT → ADDRESS → CONFIGURED
     │                                        │
     │          (tud_ready() = true)          │
     └──────────────────┬─────────────────────┘
                        ▼
              ┌─────────────────┐
              │ usb_netif.link_up = true  │
              │ dhcp_start()    │
              │                 │
              ▼                 ▼
         DHCP DISCOVER    DHCP OFFER
              │                 │
              ▼                 ▼
         DHCP REQUEST   DHCP ACK
              │                 │
              ▼                 ▼
    usb_netif.dhcp_bound = true
    usb_connected = true
    LED: slow blink (1s)
```

### WiFi AP States
```
DISABLED → cyw43_arch_init() → INITIALIZED
    │
    ▼
enable_ap_mode(ssid, pass, auth)
    │
    ▼
AP ACTIVE (beaconing)
    │
    ▼
Client PROBE REQUEST
    │
    ▼
Client AUTHENTICATE
    │
    ▼
Client ASSOCIATE
    │
    ▼
cyw43_arch_ap_sta_connect(mac)
    │
    ▼
wifi_ap.connected_clients++
    │
    ▼
DHCP DISCOVER from client
    │
    ▼
DHCP OFFER → REQUEST → ACK
    │
    ▼
Client has IP (192.168.4.x)
```

---

## Limitaciones de Hardware

| Componente | Limitacion | Impacto |
|------------|------------|---------|
| USB FS | 12 Mbps max | Throughput ~8-10 Mbps real |
| CYW43439 | 2.4GHz only, 1T1R | ~30-40 Mbps PHY, ~10-15 Mbps TCP |
| RP2040 SRAM | 264 KB | OK para buffers lwIP + NAT |
| RP2040 Flash | 2 MB | UF2 ~680 KB, espacio OK |
| I2C | 400 kHz | OLED update ~2ms |
| GPIO | 3.3V | LED en CYW43 (no RP2040) |

---

## Referencias de Implementacion

### Archivos Clave
| Archivo | Responsabilidad |
|---------|-----------------|
| `src/main.c` | Entry point, Core0 init, Core1 launch |
| `src/usb_netif.c` | USB RNDIS netif, TinyUSB callbacks |
| `src/wifi_ap.c` | WiFi AP, cyw43_arch integration |
| `src/nat.c` | NAT table, checksum fix |
| `src/dhcp_server.c` | DHCP server UDP 67 |
| `src/ssd1306.c` | OLED I2C driver |
| `include/config.h` | All tunable constants |
| `include/lwipopts.h` | lwIP configuration |
| `include/tusb_config.h` | TinyUSB configuration |

### Dependencias pico-sdk
```
pico_stdlib
pico_cyw43_arch_lwip_poll  (cyw43 + lwIP + polling)
tinyusb_device
tinyusb_board
hardware_pio
hardware_dma
hardware_i2c
pico_multicore
pico_sync
pico_rand
```

---

*Documento generado: 2026-10-04*
*Proyecto: dongle_wifi_usb*
*Target: Raspberry Pi Pico W (RP2040 + CYW43439)*