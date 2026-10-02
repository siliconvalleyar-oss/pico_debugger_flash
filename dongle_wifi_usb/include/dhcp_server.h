#ifndef DHCP_SERVER_H
#define DHCP_SERVER_H

#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include <stdint.h>

#define DHCP_START_IP                     "192.168.4.2"
#define DHCP_END_IP                       "192.168.4.20"
#define DHCP_LEASE_TIME                   3600
#define DHCP_MAX_LEASES                   16

#pragma pack(push, 1)
struct dhcp_msg {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    ip4_addr_t ciaddr;
    ip4_addr_t yiaddr;
    ip4_addr_t siaddr;
    ip4_addr_t giaddr;
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t magic_cookie;
    uint8_t options[308];
};
#pragma pack(pop)

void dhcp_server_init(ip4_addr_t *ip, ip4_addr_t *netmask, ip4_addr_t *gw, ip4_addr_t *dns);
void dhcp_server_tick(void);

#endif