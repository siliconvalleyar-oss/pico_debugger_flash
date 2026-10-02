#ifndef NAT_H
#define NAT_H

#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/prot/tcp.h"
#include "lwip/prot/udp.h"
#include "lwip/pbuf.h"
#include "config.h"
#include <stdint.h>
#include <stdbool.h>

#define NAT_PROTO_TCP                   6
#define NAT_PROTO_UDP                   17
#define NAT_PROTO_ICMP                  1

typedef enum {
    NAT_DIR_OUTBOUND = 0,
    NAT_DIR_INBOUND  = 1
} nat_direction_t;

typedef struct nat_entry {
    ip4_addr_t src_ip;
    ip4_addr_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t mapped_port;
    uint8_t protocol;
    uint32_t last_seen;
    bool in_use;
    struct nat_entry *next;
} nat_entry_t;

typedef struct {
    nat_entry_t entries[NAT_MAX_ENTRIES];
    nat_entry_t *free_list;
    nat_entry_t *active_list;
    uint16_t next_port;
    uint32_t last_cleanup;
    ip4_addr_t usb_ip;
    ip4_addr_t ap_ip;
} nat_table_t;

void nat_init(ip4_addr_t *usb_ip, ip4_addr_t *ap_ip);
void nat_tick(void);
bool nat_translate_outbound(struct pbuf *p);
bool nat_translate_inbound(struct pbuf *p);
void nat_cleanup_expired(void);
uint16_t nat_get_mapped_port(uint8_t protocol __attribute__((unused)));
void nat_dump_table(void);

#endif