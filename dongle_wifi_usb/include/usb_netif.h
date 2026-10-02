#ifndef USB_NETIF_H
#define USB_NETIF_H

#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include <stdbool.h>

#define USB_NETIF_MTU                     1500
#define USB_NETIF_HWADDR_LEN              6

typedef struct {
    struct netif netif;
    uint8_t mac[USB_NETIF_HWADDR_LEN];
    bool link_up;
    bool dhcp_bound;
    ip4_addr_t ip_addr;
    ip4_addr_t netmask;
    ip4_addr_t gw;
    uint32_t last_poll;
} usb_netif_t;

bool usb_netif_init(usb_netif_t *usb_netif, const uint8_t *mac);
void usb_netif_poll(usb_netif_t *usb_netif);
bool usb_netif_is_connected(usb_netif_t *usb_netif);
void usb_netif_set_link_up(usb_netif_t *usb_netif, bool up);
err_t usb_netif_output(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr);
err_t usb_netif_linkoutput(struct netif *netif, struct pbuf *p);

#endif