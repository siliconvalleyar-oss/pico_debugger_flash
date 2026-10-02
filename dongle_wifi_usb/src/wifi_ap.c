#include "wifi_ap.h"
#include "config.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/snmp.h"
#include "lwip/stats.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include <string.h>
#include <stdio.h>

extern cyw43_t cyw43_state;

static wifi_ap_t *g_wifi_ap = NULL;

bool wifi_ap_init(wifi_ap_t *wifi_ap, const uint8_t *mac,
                  const char *ssid, const char *password,
                  uint8_t channel, uint8_t auth, uint8_t country) {
    (void)channel;
    (void)mac;

    memset(wifi_ap, 0, sizeof(wifi_ap_t));

    if (cyw43_arch_init_with_country(country)) {
        printf("Failed to initialize CYW43\n");
        return false;
    }

    cyw43_arch_enable_ap_mode(ssid, password, auth);

    struct netif *ap_netif = &cyw43_state.netif[1];

    ip4_addr_t ipaddr, netmask, gw;
    ip4addr_aton(AP_IP_ADDR, &ipaddr);
    ip4addr_aton(AP_NETMASK, &netmask);
    ip4addr_aton(AP_GATEWAY, &gw);

    netif_set_ipaddr(ap_netif, &ipaddr);
    netif_set_netmask(ap_netif, &netmask);
    netif_set_gw(ap_netif, &gw);

    netif_set_up(ap_netif);

    wifi_ap->netif = *ap_netif;
    wifi_ap->ap_active = true;

    g_wifi_ap = wifi_ap;
    return true;
}

void wifi_ap_poll(wifi_ap_t *wifi_ap) {
    cyw43_arch_poll();

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - wifi_ap->last_poll >= 1000) {
        wifi_ap->last_poll = now;
        wifi_ap->connected_clients = 0;
    }
}

bool wifi_ap_is_active(wifi_ap_t *wifi_ap) {
    return wifi_ap->ap_active;
}

int wifi_ap_get_client_count(wifi_ap_t *wifi_ap) {
    return wifi_ap->connected_clients;
}

err_t wifi_ap_output(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr) {
    return etharp_output(netif, p, ipaddr);
}

err_t wifi_ap_linkoutput(struct netif *netif, struct pbuf *p) {
    (void)netif;
    (void)p;
    return ERR_IF;
}

void cyw43_arch_ap_sta_connect(uint8_t *mac) {
    (void)mac;
    if (g_wifi_ap) {
        g_wifi_ap->connected_clients++;
    }
}

void cyw43_arch_ap_sta_disconnect(uint8_t *mac) {
    (void)mac;
    if (g_wifi_ap) {
        g_wifi_ap->connected_clients--;
        if (g_wifi_ap->connected_clients < 0) g_wifi_ap->connected_clients = 0;
    }
}