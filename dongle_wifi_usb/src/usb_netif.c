#include "usb_netif.h"
#include "config.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/snmp.h"
#include "lwip/stats.h"
#include "lwip/prot/ethernet.h"
#include "lwip/pbuf.h"
#include "lwip/timeouts.h"
#include "tusb.h"
#include "class/net/net_device.h"
#include "pico/time.h"
#include <string.h>
#include <stdlib.h>

static usb_netif_t *g_usb_netif = NULL;

static err_t usb_netif_init_fn(struct netif *netif) {
    usb_netif_t *usb_netif = (usb_netif_t *)netif->state;

    netif->name[0] = 'u';
    netif->name[1] = 's';
    netif->mtu = USB_NETIF_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
    netif->hwaddr_len = USB_NETIF_HWADDR_LEN;
    memcpy(netif->hwaddr, usb_netif->mac, USB_NETIF_HWADDR_LEN);
    netif->output = etharp_output;
    netif->linkoutput = usb_netif_linkoutput;

    MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 12000000);

    return ERR_OK;
}

err_t usb_netif_linkoutput(struct netif *netif, struct pbuf *p) {
    usb_netif_t *usb_netif = (usb_netif_t *)netif->state;

    if (!usb_netif->link_up) {
        return ERR_IF;
    }

    struct pbuf *q;
    uint32_t total_len = 0;
    for (q = p; q != NULL; q = q->next) {
        total_len += q->len;
    }

    if (total_len > USB_NETIF_MTU) {
        return ERR_MEM;
    }

    if (tud_network_can_xmit(total_len)) {
        tud_network_xmit(p, total_len);
    }

    LINK_STATS_INC(link.xmit);
    return ERR_OK;
}

err_t usb_netif_output(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr) {
    return etharp_output(netif, p, ipaddr);
}

bool usb_netif_init(usb_netif_t *usb_netif, const uint8_t *mac) {
    memset(usb_netif, 0, sizeof(usb_netif_t));
    memcpy(usb_netif->mac, mac, USB_NETIF_HWADDR_LEN);

    ip4_addr_t ipaddr, netmask, gw;
    ip4_addr_set_zero(&ipaddr);
    ip4_addr_set_zero(&netmask);
    ip4_addr_set_zero(&gw);

    netif_add(&usb_netif->netif, &ipaddr, &netmask, &gw, usb_netif, usb_netif_init_fn, netif_input);
    netif_set_default(&usb_netif->netif);
    netif_set_up(&usb_netif->netif);

    dhcp_start(&usb_netif->netif);

    g_usb_netif = usb_netif;
    return true;
}

void usb_netif_poll(usb_netif_t *usb_netif) {
    if (!tud_ready()) {
        if (usb_netif->link_up) {
            usb_netif_set_link_up(usb_netif, false);
        }
        return;
    }

    if (!usb_netif->link_up) {
        usb_netif_set_link_up(usb_netif, true);
    }

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - usb_netif->last_poll >= LWIP_TIMER_INTERVAL_MS) {
        usb_netif->last_poll = now;
        sys_check_timeouts();
    }
}

bool usb_netif_is_connected(usb_netif_t *usb_netif) {
    return usb_netif->link_up && usb_netif->dhcp_bound;
}

void usb_netif_set_link_up(usb_netif_t *usb_netif, bool up) {
    if (up != usb_netif->link_up) {
        usb_netif->link_up = up;
        if (up) {
            netif_set_link_up(&usb_netif->netif);
        } else {
            netif_set_link_down(&usb_netif->netif);
        }
    }
}

void tud_network_init_cb(void) {
    if (g_usb_netif) {
        usb_netif_set_link_up(g_usb_netif, true);
    }
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
    struct pbuf *p = (struct pbuf *)ref;
    uint16_t total_len = arg;
    uint16_t copied = 0;

    if (!p || !dst) {
        return 0;
    }

    for (struct pbuf *q = p; q != NULL; q = q->next) {
        uint16_t to_copy = q->len;
        if (copied + to_copy > total_len) {
            to_copy = total_len - copied;
        }
        memcpy(dst + copied, q->payload, to_copy);
        copied += to_copy;
        if (copied >= total_len) break;
    }

    pbuf_free(p);
    return copied;
}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
    if (!g_usb_netif || !g_usb_netif->link_up) {
        return false;
    }

    struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
    if (p) {
        pbuf_take(p, src, size);
        if (g_usb_netif->netif.input(p, &g_usb_netif->netif) != ERR_OK) {
            pbuf_free(p);
        }
    }
    return true;
}

uint16_t tud_network_mtu_cb(void) {
    return USB_NETIF_MTU;
}

bool tud_network_link_speed_cb(uint32_t *tx_kbps, uint32_t *rx_kbps) {
    *tx_kbps = 12000;
    *rx_kbps = 12000;
    return true;
}