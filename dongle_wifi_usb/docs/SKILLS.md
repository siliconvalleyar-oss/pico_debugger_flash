# SKILLS - Aprendizajes y Conocimientos del Proyecto dongle_wifi_usb

## 1. Raspberry Pi Pico W (RP2040 + CYW43439)

### Hardware
- **MCU**: RP2040 (dual-core Cortex-M0+ @ 133MHz)
- **WiFi**: CYW43439 (2.4GHz, 802.11b/g/n)
- **LED**: En chip CYW43439 (GPIO 0 del chip wireless)
- **I2C**: GP4 (SDA), GP5 (SCL) para OLED SSD1306
- **USB**: USB 1.1 FS device (12 Mbps)

### cyw43_arch (SDK)
```c
// Inicializacion con polling mode (NO_SYS=1)
cyw43_arch_init_with_country(CYW43_COUNTRY_WORLDWIDE);

// Modo Access Point
cyw43_arch_enable_ap_mode(ssid, password, auth);
// auth: 2 = WPA2-AES-PSK

// Polling requerido en loop principal
cyw43_arch_poll();

// GPIO del LED en chip wireless
cyw43_arch_gpio_put(0, true);  // LED on
cyw43_arch_gpio_put(0, false); // LED off

// Obtener netif AP (interface 1)
struct netif *ap_netif = &cyw43_state.netif[1];
```

## 2. TinyUSB Device Stack

### Configuracion RNDIS/ECM
```c
// tusb_config.h
#define CFG_TUD_NET               1
#define CFG_TUD_ECM_RNDIS         1
#define CFG_TUD_CDC               1
#define CFG_TUD_CDC_RX_BUFSIZE    256
#define CFG_TUD_CDC_TX_BUFSIZE    256
```

### Callbacks Obligatorios
```c
// Device descriptor
const uint8_t *tud_descriptor_device_cb(void);

// Configuration descriptor (RNDIS + CDC)
const uint8_t *tud_descriptor_configuration_cb(uint8_t index);

// String descriptors
const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid);

// Network callbacks
void tud_network_init_cb(void);
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg);
bool tud_network_recv_cb(const uint8_t *src, uint16_t size);
uint16_t tud_network_mtu_cb(void);
bool tud_network_link_speed_cb(uint32_t *tx, uint32_t *rx);

// MAC address
uint8_t tud_network_mac_address[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
```

### Endpoints RNDIS
- EP1 OUT: Bulk data from host
- EP1 IN: Bulk data to host
- EP2 IN: Notification (interrupt)

## 3. lwIP (NO_SYS=1, Polling Mode)

### lwipopts.h Critico
```c
#define NO_SYS                      1
#define LWIP_NETIF_API              0
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_DHCP                   1
#define LWIP_DNS                    1
#define LWIP_IPV4                   1
#define LWIP_IPV6                   0
#define IP_FORWARD                  1
#define LWIP_ETHERNET               1
#define LWIP_ARP                    1
#define LWIP_ICMP                   1
#define LWIP_UDP                    1
#define LWIP_TCP                    1

// Memoria
#define MEM_SIZE                    (16 * 1024)
#define PBUF_POOL_SIZE              64
#define PBUF_POOL_BUFSIZE           1520
#define MEMP_NUM_PBUF               64
#define MEMP_NUM_UDP_PCB            16
#define MEMP_NUM_TCP_PCB            16
#define MEMP_NUM_TCP_SEG            32

// TCP
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (4 * TCP_MSS)
#define TCP_WND                     (4 * TCP_MSS)

// Random
#define LWIP_RAND                   get_rand_32
```

### Netif Setup
```c
// USB RNDIS netif
netif_add(&netif, &ipaddr, &netmask, &gw, state, init_fn, netif_input);
netif->output = etharp_output;
netif->linkoutput = linkoutput_fn;
netif_set_default(&netif);
netif_set_up(&netif);
dhcp_start(&netif);

// AP netif (desde cyw43_driver)
struct netif *ap_netif = &cyw43_state.netif[1];
netif_set_ipaddr(ap_netif, &ap_ip);
netif_set_netmask(ap_netif, &ap_netmask);
netif_set_gw(ap_netif, &ap_gw);
netif_set_up(ap_netif);
```

### Loop de Polling
```c
void core1_entry(void) {
    while (true) {
        tud_task();                    // TinyUSB
        usb_netif_poll(&usb_netif);    // Check USB packets
        wifi_ap_poll(&wifi_ap);        // cyw43_arch_poll()
        nat_tick();                    // NAT cleanup
        dhcp_server_tick();            // DHCP leases
        sys_check_timeouts();          // lwIP timers
        sleep_ms(10);
    }
}
```

## 4. NAT Implementation

### Tabla NAT (256 entradas)
```c
typedef struct {
    ip4_addr_t src_ip;      // IP origen (cliente WiFi)
    ip4_addr_t dst_ip;      // IP destino (internet)
    uint16_t src_port;      // Puerto origen
    uint16_t dst_port;      // Puerto destino
    uint16_t mapped_port;   // Puerto mapeado (USB side)
    uint8_t protocol;       // 6=TCP, 17=UDP, 1=ICMP
    uint32_t last_seen;     // Timestamp
    bool in_use;
} nat_entry_t;
```

### Traduccion Outbound (WiFi -> USB)
```c
// 1. Buscar/crear entrada en tabla
// 2. Cambiar src_ip a IP_USB
// 3. Cambiar src_port a mapped_port
// 4. Recalcular checksums IP/TCP/UDP/ICMP
```

### Traduccion Inbound (USB -> WiFi)
```c
// 1. Buscar entrada por dst_port (mapped_port)
// 2. Cambiar dst_ip a IP_cliente_WiFi
// 3. Cambiar dst_port a puerto original
// 4. Recalcular checksums
```

### Checksum Fix (NO_SYS=1)
```c
// IP header checksum
static void update_ip4_checksum(void *iphdr) {
    uint16_t *hdr = (uint16_t *)iphdr;
    uint32_t sum = 0;
    for (int i = 0; i < 10; i++) sum += lwip_ntohs(hdr[i]);
    sum = (sum & 0xFFFF) + (sum >> 16);
    sum = (sum & 0xFFFF) + (sum >> 16);
    hdr[10] = lwip_htons(~sum & 0xFFFF);
}

// TCP/UDP/ICMP pseudo-header checksum
tcphdr->chksum = inet_chksum_pseudo_partial(p, IP_PROTO_TCP, len, src, dst);
```

## 5. DHCP Server (UDP Port 67)

### Estructura DHCP
```c
// RFC 2131 message format
struct dhcp_msg {
    u8_t op, htype, hlen, hops;
    u32_t xid;
    u16_t secs, flags;
    ip4_addr_p_t ciaddr, yiaddr, siaddr, giaddr;
    u8_t chaddr[16];
    u8_t sname[64], file[128];
    u32_t cookie;  // 0x63825363
    u8_t options[308];
};
```

### Flujo DORA
```
DISCOVER -> OFFER -> REQUEST -> ACK
```

### Opciones Enviadas
- 53: Message Type
- 1: Subnet Mask (255.255.255.0)
- 3: Router (Gateway = AP IP)
- 6: DNS Server (AP IP)
- 51: Lease Time (3600s)
- 54: Server ID (AP IP)
- 255: End

## 6. OLED SSD1306 (I2C)

### Inicializacion
```c
// GP4=SDA, GP5=SCL, 400kHz
i2c_init(i2c0, 400000);
gpio_set_function(4, GPIO_FUNC_I2C);
gpio_set_function(5, GPIO_FUNC_I2C);
gpio_pull_up(4); gpio_pull_up(5);

// Comandos SSD1306 128x64
0xAE (display off), 0xD5 0x80 (clk div), 0xA8 0x3F (mux ratio)
0xD3 0x00 (display offset), 0x40 (start line)
0x8D 0x14 (charge pump), 0x20 0x00 (horizontal addressing)
0xA1 (seg remap), 0xC8 (com scan dir), 0xDA 0x12 (com pins)
0x81 0xCF (contrast), 0xD9 0xF1 (pre-charge), 0xDB 0x40 (vcomh)
0xA4 (entire display on), 0xA6 (normal), 0xAF (display on)
```

### Buffer y Update
```c
// 128x64 = 8 pages de 128 bytes = 1024 bytes
uint8_t buffer[1024];
bool dirty[8];

// Solo actualizar pages sucias
for (page = 0; page < 8; page++) {
    if (dirty[page]) {
        cmd(0xB0|page); cmd(0x00); cmd(0x10);
        write_data(&buffer[page*128], 128);
        dirty[page] = false;
    }
}
```

## 7. Pico SDK Build System

### CMakeLists.txt Minimal
```cmake
cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(dongle_wifi_usb C CXX ASM)
pico_sdk_init()

add_compile_options(-Wall -Wextra -Werror)

target_link_libraries(${PROJECT_NAME} PRIVATE
    pico_stdlib
    pico_cyw43_arch_lwip_poll  # incluye lwIP + cyw43 + polling
    tinyusb_device
    tinyusb_board
    hardware_pio
    hardware_dma
    hardware_i2c
    pico_multicore
    pico_sync
    pico_rand                  # para LWIP_RAND
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_add_extra_outputs(${PROJECT_NAME})  # genera .uf2
```

### Sources Adicionales Necesarias
```cmake
# TinyUSB networking library
${PICO_TINYUSB_PATH}/lib/networking/rndis_reports.c

# pico_rand para LWIP_RAND
${PICO_SDK_PATH}/src/rp2_common/pico_rand/rand.c
```

## 8. Multicore RP2040

### Core 0 (Main)
- Inicializacion hardware
- Watchdog
- LED blink (estado)
- OLED status display (1Hz)

### Core 1 (Network Loop)
- tud_task()
- usb_netif_poll()
- wifi_ap_poll()
- nat_tick()
- dhcp_server_tick()
- sys_check_timeouts()

```c
multicore_launch_core1(core1_entry);
// Core 0 continua en main loop
```

## 9. Watchdog
```c
watchdog_enable(8000, 1);  // 8s, reboot on timeout
watchdog_update();         // En cada loop iteration
watchdog_caused_reboot();  // Check al inicio
```

## 10. Limitaciones Conocidas

| Aspecto | Valor |
|---------|-------|
| USB Throughput | ~12 Mbps (FS) |
| WiFi Throughput | ~5-10 Mbps real |
| Latencia extra | 10-50ms |
| Clientes max | 4-8 recomendados |
| MTU USB | 1500 |
| MTU WiFi | 1500 |
| NAT entries | 256 |
| DHCP leases | 16 |
| IPv6 | No implementado |
| HTTPS | Funciona (passthrough) |

## 11. Debugging Tips

### Logs Utiles
```c
// Enable lwIP debug en lwipopts.h
#define LWIP_DBG_TYPES_ON (LWIP_DBG_ON | LWIP_DBG_TRACE | LWIP_DBG_STATE)

// TinyUSB debug
#define CFG_TUSB_DEBUG 2

// cyw43 debug
#define PICO_CYW43_ARCH_DEBUG_ENABLED 1
```

### Verificar Conexion
```bash
# Host Linux
ip addr show usb0
ping 192.168.4.1       # Gateway AP
ping 8.8.8.8           # Internet via USB
```

### OLED Status
```
Pico WiFi Dongle
USB: CONECTADO
WiFi AP: ACTIVO
Clientes: 2/8
IP: 192.168.1.100
SSID: PicoDongle
```

## 12. Referencias

- pico-sdk: https://github.com/raspberrypi/pico-sdk
- TinyUSB: https://github.com/hathach/tinyusb
- lwIP: https://savannah.nongnu.org/projects/lwip/
- cyw43-driver: https://github.com/raspberrypi/cyw43-driver
- USB RNDIS: https://www.usb.org/document-library/usb-rndis
- DHCP RFC 2131: https://tools.ietf.org/html/rfc2131