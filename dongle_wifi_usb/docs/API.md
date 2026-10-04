# API Reference - dongle_wifi_usb

## Indice de Modulos

1. [config.h](#configh) - Configuracion global
2. [main.c](#mainc) - Entry point y nucleo principal
3. [usb_netif.c/h](#usb_netifch) - Interfaz USB RNDIS/ECM
4. [wifi_ap.c/h](#wifi_apch) - Access Point WiFi
5. [nat.c/h](#natch) - NAT Table y traduccion
6. [dhcp_server.c/h](#dhcp_serverch) - Servidor DHCP
7. [ssd1306.c/h](#ssd1306ch) - Driver OLED I2C
8. [lwipopts.h](#lwipoptsh) - Configuracion lwIP

---

## config.h

### Constantes de Red
```c
#define WIFI_SSID              "PicoDongle"
#define WIFI_PASSWORD          "pico1234"
#define WIFI_CHANNEL           6
#define WIFI_AUTH              2              // WPA2-AES-PSK
#define WIFI_COUNTRY           0xFFFFFFFF     // Worldwide

#define AP_IP_ADDR             "192.168.4.1"
#define AP_NETMASK             "255.255.255.0"
#define AP_GATEWAY             "192.168.4.1"
#define AP_DHCP_START          "192.168.4.2"
#define AP_DHCP_END            "192.168.4.20"
#define AP_DHCP_LEASE_TIME     3600
```

### USB Device
```c
#define USB_VENDOR_ID          0x2E8A         // Raspberry Pi
#define USB_PRODUCT_ID         0x000A
#define USB_DEVICE_VERSION     0x0100

#define USB_RNDIS_MAC          {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}
#define WIFI_AP_MAC            {0x02, 0x00, 0x00, 0x00, 0x00, 0x02}
```

### NAT
```c
#define NAT_MAX_ENTRIES        256
#define NAT_ENTRY_TIMEOUT_MS   300000         // 5 min
#define NAT_CLEANUP_INTERVAL_MS 60000         // 1 min
```

### Timeouts
```c
#define WATCHDOG_TIMEOUT_MS    8000
#define MAIN_LOOP_DELAY_MS     10
#define LWIP_TIMER_INTERVAL_MS 100
```

### Hardware
```c
#define LED_PIN                0              // CYW43 GPIO 0 (LED onboard)
#define LED_ON_CYW43           1              // LED en chip wireless
#define MAX_WIFI_CLIENTS       8
#define MAX_USB_MTU            1500
#define MAX_WIFI_MTU           1500
```

---

## main.c

### Variables Globales
```c
static usb_netif_t usb_netif;     // Estado interfaz USB
static wifi_ap_t wifi_ap;         // Estado WiFi AP
static ssd1306_t display;         // Estado OLED

static bool usb_connected;        // USB link up + DHCP bound
static bool wifi_ap_active;       // AP habilitado
static int wifi_clients;          // Clientes asociados
static char ip_str[16];           // IP USB string
```

### Funciones

#### `led_set(bool on)`
Controla LED onboard (GPIO 0 en CYW43439).
```c
void led_set(bool on);  // on=true: encendido, false: apagado
```

#### `core1_entry(void)`
Loop principal en Core 1. Ejecuta:
- `tud_task()` - TinyUSB device task
- `usb_netif_poll()` - Poll USB packets
- `wifi_ap_poll()` - cyw43_arch_poll()
- `nat_tick()` - NAT cleanup
- `dhcp_server_tick()` - DHCP lease expiry
- Status update cada 1s (OLED)
- LED blink pattern
- Watchdog update

#### `main(void)`
Inicializacion en Core 0:
1. `stdio_init_all()`
2. GPIO/Watchdog init
3. `ssd1306_init()` - OLED
4. `usb_netif_init()` - USB RNDIS + DHCP client
5. `wifi_ap_init()` - WiFi AP + static IP
6. `nat_init()` - Tabla NAT
6. `dhcp_server_init()` - DHCP server en AP
7. `multicore_launch_core1(core1_entry)`
8. Core 0: LED blink + watchdog loop

---

## usb_netif.c/h

### Struct
```c
typedef struct {
    struct netif netif;           // lwIP netif
    uint8_t mac[6];               // MAC address
    bool link_up;                 // USB link status
    bool dhcp_bound;              // DHCP client got IP
    ip4_addr_t ip_addr;           // IP asignada por host
    ip4_addr_t netmask;           // Netmask
    ip4_addr_t gw;                // Gateway
    uint32_t last_poll;           // Timestamp ultimo poll
} usb_netif_t;
```

### API

#### `bool usb_netif_init(usb_netif_t *usb_netif, const uint8_t *mac)`
Inicializa interfaz USB RNDIS.
- Registra netif en lwIP
- Inicia DHCP client
- Configura callbacks TinyUSB
- Returns: `true` si exito

#### `void usb_netif_poll(usb_netif_t *usb_netif)`
Polling periodico (llamar desde loop principal).
- Verifica `tud_ready()`
- Procesa paquetes RX via `tud_network_recv_cb`
- Llama `sys_check_timeouts()` cada 100ms

#### `bool usb_netif_is_connected(usb_netif_t *usb_netif)`
Returns: `true` si USB link up y DHCP bound.

#### `void usb_netif_set_link_up(usb_netif_t *usb_netif, bool up)`
Actualiza estado link en lwIP (`netif_set_link_up/down`).

### TinyUSB Callbacks (implementados en usb_netif.c)
```c
void tud_network_init_cb(void);                    // USB link up
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg);  // TX
bool tud_network_recv_cb(const uint8_t *src, uint16_t size);          // RX
uint16_t tud_network_mtu_cb(void);                 // Return 1500
bool tud_network_link_speed_cb(uint32_t *tx, uint32_t *rx);           // 12Mbps
```

### lwIP Netif Callbacks
```c
err_t usb_netif_init_fn(struct netif *netif);      // Setup netif
err_t usb_netif_linkoutput(struct netif *netif, struct pbuf *p);  // TX
err_t usb_netif_output(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr);  // etharp_output
```

---

## wifi_ap.c/h

### Struct
```c
typedef struct {
    struct netif netif;           // lwIP netif (copia de cyw43_state.netif[1])
    uint8_t mac[6];               // MAC address
    bool ap_active;               // AP habilitado
    int connected_clients;        // Numero clientes asociados
    uint32_t last_poll;           // Timestamp ultimo poll
} wifi_ap_t;
```

### API

#### `bool wifi_ap_init(wifi_ap_t *wifi_ap, const uint8_t *mac, const char *ssid, const char *password, uint8_t channel, uint8_t auth, uint8_t country)`
Inicializa WiFi Access Point.
- `cyw43_arch_init_with_country(country)`
- `cyw43_arch_enable_ap_mode(ssid, password, auth)`
- Configura IP static en `cyw43_state.netif[1]`
- Returns: `true` si exito

#### `void wifi_ap_poll(wifi_ap_t *wifi_ap)`
Polling periodico.
- `cyw43_arch_poll()`
- Actualiza `connected_clients` cada 1s

#### `bool wifi_ap_is_active(wifi_ap_t *wifi_ap)`
Returns: `true` si AP habilitado.

#### `int wifi_ap_get_client_count(wifi_ap_t *wifi_ap)`
Returns: Numero clientes asociados.

#### `err_t wifi_ap_output(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr)`
etharp_output wrapper.

#### `err_t wifi_ap_linkoutput(struct netif *netif, struct pbuf *p)`
No usado (TX via cyw43_driver).

### Callbacks cyw43_arch (weak)
```c
void cyw43_arch_ap_sta_connect(uint8_t *mac);    // Cliente conecta
void cyw43_arch_ap_sta_disconnect(uint8_t *mac); // Cliente desconecta
```

---

## nat.c/h

### Structs
```c
typedef enum {
    NAT_DIR_OUTBOUND = 0,   // WiFi -> USB
    NAT_DIR_INBOUND  = 1    // USB -> WiFi
} nat_direction_t;

typedef struct nat_entry {
    ip4_addr_t src_ip;
    ip4_addr_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t mapped_port;
    uint8_t protocol;       // 6=TCP, 17=UDP, 1=ICMP
    uint32_t last_seen;
    bool in_use;
    struct nat_entry *next;
} nat_entry_t;

typedef struct {
    nat_entry_t entries[256];
    nat_entry_t *free_list;
    nat_entry_t *active_list;
    uint16_t next_port;     // 1024-65535
    uint32_t last_cleanup;
    ip4_addr_t usb_ip;      // IP interfaz USB
    ip4_addr_t ap_ip;       // IP interfaz AP
} nat_table_t;
```

### API

#### `void nat_init(ip4_addr_t *usb_ip, ip4_addr_t *ap_ip)`
Inicializa tabla NAT.
- Inicializa free list (256 entradas)
- Guarda IPs de referencia

#### `void nat_tick(void)`
Llama `nat_cleanup_expired()` - elimina entradas > 5 min sin uso.

#### `bool nat_translate_outbound(struct pbuf *p)`
Traduce paquete WiFi -> USB.
- Busca/crea entrada NAT
- Cambia src_ip a `usb_ip`
- Cambia src_port a `mapped_port`
- Recalcula checksums IP/TCP/UDP/ICMP
- Returns: `true` si traducido

#### `bool nat_translate_inbound(struct pbuf *p)`
Traduce paquete USB -> WiFi.
- Busca entrada por `mapped_port` (dst_port)
- Cambia dst_ip a IP cliente original
- Cambia dst_port a puerto original
- Recalcula checksums
- Returns: `true` si traducido

#### `uint16_t nat_get_mapped_port(uint8_t protocol)`
Retorna puerto disponible (round-robin 1024-65535).

#### `void nat_dump_table(void)`
Debug: imprime tabla NAT activa por serial.

### Helpers Internos
```c
static nat_entry_t *nat_alloc_entry(void);
static void nat_free_entry(nat_entry_t *entry);
static nat_entry_t *nat_find_entry(ip4_addr_t *src_ip, ip4_addr_t *dst_ip, uint16_t src_port, uint16_t dst_port, uint8_t protocol, nat_direction_t dir);
static void update_ip4_checksum(void *iphdr);
static void update_tcp_checksum(struct pbuf *p, void *iphdr, uint32_t new_src, uint32_t new_dst);
static void update_udp_checksum(struct pbuf *p, void *iphdr, uint32_t new_src, uint32_t new_dst);
static void update_icmp_checksum(struct pbuf *p, void *iphdr, uint32_t new_src, uint32_t new_dst);
```

---

## dhcp_server.c/h

### Struct
```c
typedef struct {
    ip4_addr_t ip;           // IP asignada
    uint8_t mac[6];          // MAC cliente
    uint32_t lease_expires;  // Unix timestamp expiracion
    bool in_use;             // Entrada activa
} dhcp_lease_t;
```

### Constantes
```c
#define DHCP_SERVER_PORT       67
#define DHCP_CLIENT_PORT       68
#define DHCP_MAX_LEASES        16
#define DHCP_LEASE_TIME        3600
```

### API

#### `void dhcp_server_init(ip4_addr_t *ip, ip4_addr_t *netmask, ip4_addr_t *gw, ip4_addr_t *dns)`
Inicializa servidor DHCP en AP.
- Bind UDP port 67
- Registra callback `dhcp_recv`
- Configura pool IPs 192.168.4.2-20

#### `void dhcp_server_tick(void)`
Limpia leases expirados (llamado cada loop).

### DHCP Message Flow
```
Cliente                    Servidor (Pico)
  |                           |
  |---- DISCOVER ------------>|
  |                           |--- OFFER (IP propuesta)
  |<-- OFFER -----------------|
  |                           |
  |---- REQUEST (IP) -------->|
  |                           |--- ACK (confirma)
  |<-- ACK -------------------|
  |                           |
```

### Opciones DHCP Enviadas
| Code | Option | Value |
|------|--------|-------|
| 53   | Message Type | OFFER/ACK/NAK |
| 1    | Subnet Mask | 255.255.255.0 |
| 3    | Router | 192.168.4.1 |
| 6    | DNS Server | 192.168.4.1 |
| 51   | Lease Time | 3600s |
| 54   | Server ID | 192.168.4.1 |
| 255  | End | - |

---

## ssd1306.c/h

### Struct
```c
typedef struct {
    uint8_t buffer[1024];     // 128x64 = 8 pages x 128 bytes
    bool dirty[8];            // Pages que necesitan update
} ssd1306_t;
```

### Constantes
```c
#define SSD1306_I2C_ADDR       0x3C
#define SSD1306_WIDTH          128
#define SSD1306_HEIGHT         64
#define SSD1306_PAGES          8
#define SSD1306_I2C_SDA_PIN    4
#define SSD1306_I2C_SCL_PIN    5
#define SSD1306_I2C_INSTANCE   i2c0
#define SSD1306_I2C_BAUDRATE   400000
```

### API

#### `bool ssd1306_init(ssd1306_t *display)`
Inicializa OLED via I2C.
- Configura I2C 400kHz GP4/GP5
- Secuencia comandos init SSD1306
- Limpia buffer
- Returns: `true` si exito

#### `void ssd1306_clear(ssd1306_t *display)`
Limpia buffer y marca todas pages dirty.

#### `void ssd1306_draw_pixel(ssd1306_t *display, int x, int y, bool on)`
Dibuja pixel individual (0-127, 0-63).

#### `void ssd1306_draw_char(ssd1306_t *display, int x, int y, char c, bool on)`
Dibuja caracter 5x7 (ASCII 32-126).

#### `void ssd1306_draw_string(ssd1306_t *display, int x, int y, const char *str, bool on)`
Dibuja string.

#### `void ssd1306_draw_line(ssd1306_t *display, int x1, int y1, int x2, int y2, bool on)`
Linea Bresenham.

#### `void ssd1306_draw_rect(ssd1306_t *display, int x, int y, int w, int h, bool filled, bool on)`
Rectangulo.

#### `void ssd1306_update(ssd1306_t *display)`
Envia pages dirty al display via I2C.

#### `void ssd1306_set_contrast(ssd1306_t *display, uint8_t contrast)`
Contraste 0-255.

#### `void ssd1306_invert(ssd1306_t *display, bool invert)`
Invertir colores.

#### `void ssd1306_sleep(ssd1306_t *display, bool sleep)`
Sleep mode.

#### `void ssd1306_show_status(ssd1306_t *display, bool usb_connected, bool wifi_ap_active, int wifi_clients, const char *ip_str)`
Muestra dashboard completo:
```
Pico WiFi Dongle
--------------
USB: CONECTADO
WiFi AP: ACTIVO
Clientes: 2/8
IP: 192.168.1.100
--------------
SSID: PicoDongle
```

#### `void ssd1306_show_error(ssd1306_t *display, const char *msg)`
Muestra pantalla de error.

---

## lwipopts.h

### Configuracion Critica
```c
// Core
#define NO_SYS                 1          // Polling mode, no RTOS
#define LWIP_NETIF_API         0          // No netifapi
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_HWADDRHINT       1

// Memoria
#define MEM_SIZE               (16 * 1024)
#define MEMP_NUM_PBUF          64
#define MEMP_NUM_UDP_PCB       16
#define MEMP_NUM_TCP_PCB       16
#define MEMP_NUM_TCP_PCB_LISTEN 8
#define MEMP_NUM_TCP_SEG       32
#define MEMP_NUM_ARP_QUEUE     16
#define MEMP_NUM_SYS_TIMEOUT   16
#define PBUF_POOL_SIZE         64
#define PBUF_POOL_BUFSIZE      1520

// Protocolos
#define LWIP_ARP               1
#define LWIP_ETHERNET          1
#define LWIP_ICMP              1
#define LWIP_RAW               1
#define LWIP_IPV4              1
#define LWIP_IPV6              0
#define LWIP_DHCP              1
#define LWIP_DNS               1
#define LWIP_UDP               1
#define LWIP_TCP               1

// Forwarding/NAT
#define IP_FORWARD             1
#define IP_REASSEMBLY          1
#define IP_FRAG                1

// TCP
#define TCP_MSS                1460
#define TCP_SND_BUF            (4 * TCP_MSS)
#define TCP_WND                (4 * TCP_MSS)

// DHCP Server (lwIP 2.1+)
#define LWIP_DHCP_SERVER       1

// Random
#define LWIP_RAND              get_rand_32
```

### Funcion get_rand_32()
Proporcionada por `pico_rand`:
```c
uint32_t get_rand_32(void);  // En pico/rand.h
```

---

## Flujo de Datos Resumido

```
[Cliente WiFi] --802.11--> [wifi_ap] --lwIP netif[1]-->
    nat_translate_outbound() --> [usb_netif] --lwIP netif[0]-->
    USB RNDIS --> [Host PC] --> Internet

[Internet] --> [Host PC] --> USB RNDIS --> [usb_netif] -->
    nat_translate_inbound() --> [wifi_ap] --> 802.11 --> [Cliente WiFi]
```

---

## Error Codes

| Codigo | Significado |
|--------|-------------|
| `ERR_OK` | 0 - Exito |
| `ERR_MEM` | -1 - Sin memoria |
| `ERR_BUF` | -2 - Buffer error |
| `ERR_TIMEOUT` | -3 - Timeout |
| `ERR_RTE` | -4 - Routing error |
| `ERR_INPROGRESS` | -5 - En progreso |
| `ERR_VAL` | -6 - Valor invalido |
| `ERR_WOULDBLOCK` | -7 - Bloquearia |
| `ERR_USE` | -8 - En uso |
| `ERR_ALREADY` | -9 - Ya existe |
| `ERR_ISCONN` | -10 - Ya conectado |
| `ERR_CONN` | -11 - No conectado |
| `ERR_IF` | -12 - Interfaz error |
| `ERR_ABRT` | -13 - Abortado |
| `ERR_RST` | -14 - Reset |
| `ERR_CLSD` | -15 - Cerrado |
| `ERR_ARG` | -16 - Argumento invalido |

---

## Constantes de Protocolo

```c
#define NAT_PROTO_TCP   6
#define NAT_PROTO_UDP   17
#define NAT_PROTO_ICMP  1

#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17
#define IP_PROTO_ICMP   1
#define IP_PROTO_IGMP   2

#define DHCP_SERVER_PORT  67
#define DHCP_CLIENT_PORT  68
#define DNS_PORT          53
```