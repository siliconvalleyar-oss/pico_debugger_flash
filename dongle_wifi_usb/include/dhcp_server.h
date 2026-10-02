#ifndef DHCP_SERVER_H
#define DHCP_SERVER_H

#include "lwip/ip_addr.h"
#include "lwip/prot/dhcp.h"
#include <stdint.h>

#define DHCP_START_IP                     "192.168.4.2"
#define DHCP_END_IP                       "192.168.4.20"
#define DHCP_LEASE_TIME                   3600
#define DHCP_MAX_LEASES                   16

void dhcp_server_init(ip4_addr_t *ip, ip4_addr_t *netmask, ip4_addr_t *gw, ip4_addr_t *dns);
void dhcp_server_tick(void);

#endif