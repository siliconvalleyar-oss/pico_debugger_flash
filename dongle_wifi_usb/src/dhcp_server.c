#include "dhcp_server.h"
#include "config.h"
#include "lwip/ip4_addr.h"
#include "lwip/prot/dhcp.h"
#include "lwip/udp.h"
#include "lwip/dhcp.h"
#include "pico/time.h"
#include <string.h>
#include <stdio.h>

#define DHCP_SERVER_PORT                  67
#define DHCP_CLIENT_PORT                  68
#define DHCP_MAGIC_COOKIE                 0x63825363

#define DHCP_OP_REQUEST                   1
#define DHCP_OP_REPLY                     2

#define DHCP_HTYPE_ETHERNET               1
#define DHCP_HLEN_ETHERNET                6

#define DHCP_OPTION_MESSAGE_TYPE          53
#define DHCP_OPTION_SUBNET_MASK           1
#define DHCP_OPTION_ROUTER                3
#define DHCP_OPTION_DNS_SERVER            6
#define DHCP_OPTION_LEASE_TIME            51
#define DHCP_OPTION_SERVER_ID             54
#define DHCP_OPTION_END                   255

#define DHCP_DISCOVER                     1
#define DHCP_OFFER                        2
#define DHCP_REQUEST                      3
#define DHCP_DECLINE                      4
#define DHCP_ACK                          5
#define DHCP_NAK                          6
#define DHCP_RELEASE                      7
#define DHCP_INFORM                       8

typedef struct {
    ip4_addr_t ip;
    uint8_t mac[6];
    uint32_t lease_expires;
    bool in_use;
} dhcp_lease_t;

static struct udp_pcb *dhcp_pcb = NULL;
static dhcp_lease_t leases[DHCP_MAX_LEASES];
static ip4_addr_t server_ip;
static ip4_addr_t subnet_mask;
static ip4_addr_t gateway_ip;
static ip4_addr_t dns_ip;
static uint32_t lease_time;
static ip4_addr_t next_ip;

static uint32_t ip4_to_u32(ip4_addr_t *ip) {
    return lwip_ntohl(ip4_addr_get_u32(ip));
}

static void u32_to_ip4(uint32_t val, ip4_addr_t *ip) {
    ip4_addr_set_u32(ip, lwip_htonl(val));
}

static bool mac_equal(uint8_t *a, uint8_t *b) {
    return memcmp(a, b, 6) == 0;
}

static dhcp_lease_t *find_lease_by_mac(uint8_t *mac) {
    for (int i = 0; i < DHCP_MAX_LEASES; i++) {
        if (leases[i].in_use && mac_equal(leases[i].mac, mac)) {
            return &leases[i];
        }
    }
    return NULL;
}

static dhcp_lease_t *find_lease_by_ip(ip4_addr_t *ip) {
    for (int i = 0; i < DHCP_MAX_LEASES; i++) {
        if (leases[i].in_use && ip4_addr_cmp(&leases[i].ip, ip)) {
            return &leases[i];
        }
    }
    return NULL;
}

static dhcp_lease_t *allocate_lease(uint8_t *mac) {
    uint32_t now = to_ms_since_boot(get_absolute_time()) / 1000;

    for (int i = 0; i < DHCP_MAX_LEASES; i++) {
        if (!leases[i].in_use || leases[i].lease_expires < now) {
            leases[i].in_use = true;
            memcpy(leases[i].mac, mac, 6);
            ip4_addr_copy(leases[i].ip, next_ip);
            leases[i].lease_expires = now + lease_time;

            uint32_t next = ip4_to_u32(&next_ip) + 1;
            u32_to_ip4(next, &next_ip);

            if (ip4_to_u32(&next_ip) > ip4_to_u32((ip4_addr_t*)DHCP_END_IP)) {
                uint32_t start = ip4_to_u32((ip4_addr_t*)DHCP_START_IP);
                u32_to_ip4(start, &next_ip);
            }
            return &leases[i];
        }
    }
    return NULL;
}

static void dhcp_send_response(struct dhcp_msg *msg, uint8_t msg_type,
                               ip4_addr_t *client_ip, uint8_t *client_mac) {
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(struct dhcp_msg), PBUF_RAM);
    if (!p) return;

    struct dhcp_msg *resp = (struct dhcp_msg *)p->payload;
    memset(resp, 0, sizeof(struct dhcp_msg));

    resp->op = DHCP_OP_REPLY;
    resp->htype = DHCP_HTYPE_ETHERNET;
    resp->hlen = DHCP_HLEN_ETHERNET;
    resp->hops = 0;
    resp->xid = msg->xid;
    resp->secs = 0;
    resp->flags = msg->flags;
    ip4_addr_copy(resp->ciaddr, msg->ciaddr);
    ip4_addr_copy(resp->yiaddr, *client_ip);
    ip4_addr_set_zero(&resp->siaddr);
    ip4_addr_set_zero(&resp->giaddr);
    memcpy(resp->chaddr, client_mac, 6);
    resp->magic_cookie = lwip_htonl(DHCP_MAGIC_COOKIE);

    uint8_t *opt = resp->options;
    *opt++ = DHCP_OPTION_MESSAGE_TYPE;
    *opt++ = 1;
    *opt++ = msg_type;

    *opt++ = DHCP_OPTION_SUBNET_MASK;
    *opt++ = 4;
    memcpy(opt, &subnet_mask.addr, 4);
    opt += 4;

    *opt++ = DHCP_OPTION_ROUTER;
    *opt++ = 4;
    memcpy(opt, &gateway_ip.addr, 4);
    opt += 4;

    *opt++ = DHCP_OPTION_DNS_SERVER;
    *opt++ = 4;
    memcpy(opt, &dns_ip.addr, 4);
    opt += 4;

    *opt++ = DHCP_OPTION_LEASE_TIME;
    *opt++ = 4;
    uint32_t lt = lwip_htonl(lease_time);
    memcpy(opt, &lt, 4);
    opt += 4;

    *opt++ = DHCP_OPTION_SERVER_ID;
    *opt++ = 4;
    memcpy(opt, &server_ip.addr, 4);
    opt += 4;

    *opt++ = DHCP_OPTION_END;

    struct udp_pcb *pcb = udp_new();
    if (pcb) {
        ip_addr_t broadcast;
        IP4_ADDR(&broadcast, 255, 255, 255, 255);
        udp_sendto(pcb, p, &broadcast, DHCP_CLIENT_PORT);
        udp_remove(pcb);
    }
    pbuf_free(p);
}

static void dhcp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                      const ip_addr_t *addr, u16_t port) {
    if (p->len < sizeof(struct dhcp_msg)) {
        pbuf_free(p);
        return;
    }

    struct dhcp_msg *msg = (struct dhcp_msg *)p->payload;

    if (msg->magic_cookie != lwip_htonl(DHCP_MAGIC_COOKIE)) {
        pbuf_free(p);
        return;
    }

    uint8_t msg_type = 0;
    uint8_t *opt = msg->options;
    while (opt < (uint8_t *)msg + p->len) {
        if (*opt == DHCP_OPTION_END) break;
        if (*opt == DHCP_OPTION_MESSAGE_TYPE) {
            msg_type = *(opt + 2);
            break;
        }
        opt += 2 + *(opt + 1);
    }

    ip4_addr_t client_ip;
    ip4_addr_set_zero(&client_ip);
    memcpy(client_mac, msg->chaddr, 6);

    dhcp_lease_t *lease = find_lease_by_mac(client_mac);

    switch (msg_type) {
        case DHCP_DISCOVER: {
            dhcp_lease_t *new_lease = lease ? lease : allocate_lease(client_mac);
            if (new_lease) {
                ip4_addr_copy(client_ip, new_lease->ip);
                dhcp_send_response(msg, DHCP_OFFER, &client_ip, client_mac);
            }
            break;
        }
        case DHCP_REQUEST: {
            ip4_addr_t requested_ip;
            ip4_addr_set_zero(&requested_ip);
            opt = msg->options;
            while (opt < (uint8_t *)msg + p->len) {
                if (*opt == DHCP_OPTION_END) break;
                if (*opt == 50) {
                    memcpy(&requested_ip.addr, opt + 2, 4);
                    break;
                }
                opt += 2 + *(opt + 1);
            }

            if (ip4_addr_isany(&requested_ip)) {
                ip4_addr_copy(requested_ip, msg->ciaddr);
            }

            if (lease && ip4_addr_cmp(&lease->ip, &requested_ip)) {
                lease->lease_expires = to_ms_since_boot(get_absolute_time()) / 1000 + lease_time;
                ip4_addr_copy(client_ip, lease->ip);
                dhcp_send_response(msg, DHCP_ACK, &client_ip, client_mac);
            } else {
                dhcp_lease_t *new_lease = allocate_lease(client_mac);
                if (new_lease) {
                    ip4_addr_copy(client_ip, new_lease->ip);
                    dhcp_send_response(msg, DHCP_ACK, &client_ip, client_mac);
                } else {
                    dhcp_send_response(msg, DHCP_NAK, &client_ip, client_mac);
                }
            }
            break;
        }
        case DHCP_RELEASE: {
            if (lease) {
                lease->in_use = false;
            }
            break;
        }
        case DHCP_INFORM: {
            if (lease) {
                ip4_addr_copy(client_ip, lease->ip);
            } else {
                dhcp_lease_t *new_lease = allocate_lease(client_mac);
                if (new_lease) {
                    ip4_addr_copy(client_ip, new_lease->ip);
                }
            }
            dhcp_send_response(msg, DHCP_ACK, &client_ip, client_mac);
            break;
        }
    }

    pbuf_free(p);
}

void dhcp_server_init(ip4_addr_t *ip, ip4_addr_t *netmask, ip4_addr_t *gw, ip4_addr_t *dns) {
    memset(leases, 0, sizeof(leases));
    ip4_addr_copy(server_ip, *ip);
    ip4_addr_copy(subnet_mask, *netmask);
    ip4_addr_copy(gateway_ip, *gw);
    ip4_addr_copy(dns_ip, *dns);
    lease_time = DHCP_LEASE_TIME;

    ip4_addr_t start_ip;
    ip4addr_aton(DHCP_START_IP, &start_ip);
    ip4_addr_copy(next_ip, start_ip);

    dhcp_pcb = udp_new_ip_type(IPADDR_TYPE_V4);
    if (dhcp_pcb) {
        udp_bind(dhcp_pcb, IP_ANY_TYPE, DHCP_SERVER_PORT);
        udp_recv(dhcp_pcb, dhcp_recv, NULL);
    }
}

void dhcp_server_tick(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time()) / 1000;
    for (int i = 0; i < DHCP_MAX_LEASES; i++) {
        if (leases[i].in_use && leases[i].lease_expires < now) {
            leases[i].in_use = false;
        }
    }
}