#ifndef WIFI_AP_H
#define WIFI_AP_H

#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include <stdbool.h>

#define WIFI_AP_MTU                       1500
#define WIFI_AP_HWADDR_LEN                6
#define MAX_WIFI_CLIENTS                  8

typedef struct {
    struct netif netif;
    uint8_t mac[WIFI_AP_HWADDR_LEN];
    bool ap_active;
    int connected_clients;
    uint32_t last_poll;
} wifi_ap_t;

bool wifi_ap_init(wifi_ap_t *wifi_ap, const uint8_t *mac,
                  const char *ssid, const char *password,
                  uint8_t channel, uint8_t auth, uint8_t country);
void wifi_ap_poll(wifi_ap_t *wifi_ap);
bool wifi_ap_is_active(wifi_ap_t *wifi_ap);
int wifi_ap_get_client_count(wifi_ap_t *wifi_ap);
err_t wifi_ap_output(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr);
err_t wifi_ap_linkoutput(struct netif *netif, struct pbuf *p);

#endif