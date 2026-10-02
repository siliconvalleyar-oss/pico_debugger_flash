#include "wifi_ap.h"
#include "config.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/snmp.h"
#include "lwip/stats.h"
#include "lwip/prot/ethernet.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include <string.h>
#include <stdio.h>

static wifi_ap_t *g_wifi_ap = NULL;

static err_t wifi_ap_init_fn(struct netif *netif) {
    wifi_ap_t *wifi_ap = (wifi_ap_t *)netif->state;

    netif->name[0] = 'w';
    netif->name[1] = 'l';
    netif->mtu = WIFI_AP_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_UP;
    netif->hwaddr_len = WIFI_AP_HWADDR_LEN;
    memcpy(netif->hwaddr, wifi_ap->mac, WIFI_AP_HWADDR_LEN);
    netif->output = etharp_output;
    netif->linkoutput = wifi_ap_linkoutput;

    MIB2_INIT_NETIF(netif, snmp_ifType_ieee80211, 1000000);

    return ERR_OK;
}

err_t wifi_ap_linkoutput(struct netif *netif, struct pbuf *p) {
    wifi_ap_t *wifi_ap = (wifi_ap_t *)netif->state;

    if (!wifi_ap->ap_active) {
        return ERR_IF;
    }

    struct pbuf *q;
    uint32_t total_len = 0;
    for (q = p; q != NULL; q = q->next) {
        total_len += q->len;
    }

    if (total_len > WIFI_AP_MTU) {
        return ERR_MEM;
    }

    uint8_t *buf = malloc(total_len);
    if (!buf) {
        return ERR_MEM;
    }

    uint8_t *ptr = buf;
    for (q = p; q != NULL; q = q->next) {
        memcpy(ptr, q->payload, q->len);
        ptr += q->len;
    }

    cyw43_arch_send_ethernet(buf, total_len);
    free(buf);

    LINK_STATS_INC(link.xmit);
    return ERR_OK;
}

err_t wifi_ap_output(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr) {
    return etharp_output(netif, p, ipaddr);
}

bool wifi_ap_init(wifi_ap_t *wifi_ap, const uint8_t *mac,
                  const char *ssid, const char *password,
                  uint8_t channel, uint8_t auth, uint8_t country) {
    memset(wifi_ap, 0, sizeof(wifi_ap_t));
    memcpy(wifi_ap->mac, mac, WIFI_AP_HWADDR_LEN);

    if (cyw43_arch_init_with_country(country)) {
        printf("Failed to initialize CYW43\n");
        return false;
    }

    cyw43_arch_enable_ap_mode(ssid, password, auth);

    ip4_addr_t ipaddr, netmask, gw;
    ip4addr_aton(AP_IP_ADDR, &ipaddr);
    ip4addr_aton(AP_NETMASK, &netmask);
    ip4addr_aton(AP_GATEWAY, &gw);

    netif_add(&wifi_ap->netif, &ipaddr, &netmask, &gw, wifi_ap, wifi_ap_init_fn, netif_input);
    netif_set_up(&wifi_ap->netif);

    wifi_ap->ap_active = true;

    g_wifi_ap = wifi_ap;
    return true;
}

void wifi_ap_poll(wifi_ap_t *wifi_ap) {
    cyw43_arch_poll();

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - wifi_ap->last_poll >= 1000) {
        wifi_ap->last_poll = now;
        wifi_ap->connected_clients = cyw43_arch_get_sta_count();
    }
}

bool wifi_ap_is_active(wifi_ap_t *wifi_ap) {
    return wifi_ap->ap_active;
}

int wifi_ap_get_client_count(wifi_ap_t *wifi_ap) {
    return wifi_ap->connected_clients;
}

void cyw43_arch_ap_sta_connect(uint8_t *mac) {
    if (g_wifi_ap) {
        g_wifi_ap->connected_clients++;
    }
}

void cyw43_arch_ap_sta_disconnect(uint8_t *mac) {
    if (g_wifi_ap) {
        g_wifi_ap->connected_clients--;
        if (g_wifi_ap->connected_clients < 0) g_wifi_ap->connected_clients = 0;
    }
}