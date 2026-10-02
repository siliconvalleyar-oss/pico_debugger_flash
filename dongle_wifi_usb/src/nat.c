#include "nat.h"
#include "config.h"
#include "lwip/ip4.h"
#include "lwip/ip4_frag.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/tcp.h"
#include "lwip/prot/udp.h"
#include "lwip/prot/icmp.h"
#include "lwip/etharp.h"
#include "pico/time.h"
#include <string.h>

static nat_table_t nat_table;

static uint16_t ip4_chksum_pseudo_partial(struct pbuf *p, uint8_t proto,
                                         uint16_t proto_len, uint32_t src, uint32_t dst) {
    return inet_chksum_pseudo_partial(p, proto, proto_len, src, dst);
}

static void update_ip4_checksum(struct ip4_hdr *iphdr) {
    iphdr->_chksum = 0;
    iphdr->_chksum = inet_chksum(iphdr, IP4_HLEN);
}

static void update_tcp_checksum(struct pbuf *p, struct ip4_hdr *iphdr,
                                const ip4_addr_t *old_src, const ip4_addr_t *new_src,
                                const ip4_addr_t *old_dst, const ip4_addr_t *new_dst) {
    struct tcp_hdr *tcphdr = (struct tcp_hdr *)((uint8_t *)iphdr + IP4_HLEN);
    uint16_t tcplen = lwip_ntohs(iphdr->_len) - IP4_HLEN;

    tcphdr->chksum = 0;
    tcphdr->chksum = ip4_chksum_pseudo_partial(p, IP_PROTO_TCP, tcplen,
                                               ip4_addr_get_u32(new_src),
                                               ip4_addr_get_u32(new_dst));
}

static void update_udp_checksum(struct pbuf *p, struct ip4_hdr *iphdr,
                                const ip4_addr_t *old_src, const ip4_addr_t *new_src,
                                const ip4_addr_t *old_dst, const ip4_addr_t *new_dst) {
    struct udp_hdr *udphdr = (struct udp_hdr *)((uint8_t *)iphdr + IP4_HLEN);
    uint16_t udplen = lwip_ntohs(udphdr->len);

    if (udphdr->chksum == 0) {
        return;
    }

    udphdr->chksum = 0;
    udphdr->chksum = ip4_chksum_pseudo_partial(p, IP_PROTO_UDP, udplen,
                                               ip4_addr_get_u32(new_src),
                                               ip4_addr_get_u32(new_dst));
}

static void update_icmp_checksum(struct pbuf *p, struct ip4_hdr *iphdr,
                                 const ip4_addr_t *old_src, const ip4_addr_t *new_src,
                                 const ip4_addr_t *old_dst, const ip4_addr_t *new_dst) {
    struct icmp_echo_hdr *icmphdr = (struct icmp_echo_hdr *)((uint8_t *)iphdr + IP4_HLEN);
    uint16_t icmplen = lwip_ntohs(iphdr->_len) - IP4_HLEN;

    icmphdr->chksum = 0;
    icmphdr->chksum = ip4_chksum_pseudo_partial(p, IP_PROTO_ICMP, icmplen,
                                               ip4_addr_get_u32(new_src),
                                               ip4_addr_get_u32(new_dst));
}

void nat_init(ip4_addr_t *usb_ip, ip4_addr_t *ap_ip) {
    memset(&nat_table, 0, sizeof(nat_table));
    nat_table.usb_ip = *usb_ip;
    nat_table.ap_ip = *ap_ip;
    nat_table.next_port = 1024;
    nat_table.last_cleanup = to_ms_since_boot(get_absolute_time());

    for (int i = 0; i < NAT_MAX_ENTRIES - 1; i++) {
        nat_table.entries[i].next = &nat_table.entries[i + 1];
    }
    nat_table.entries[NAT_MAX_ENTRIES - 1].next = NULL;
    nat_table.free_list = &nat_table.entries[0];
    nat_table.active_list = NULL;
}

static nat_entry_t *nat_alloc_entry(void) {
    nat_entry_t *entry = nat_table.free_list;
    if (entry) {
        nat_table.free_list = entry->next;
        entry->next = nat_table.active_list;
        nat_table.active_list = entry;
        entry->in_use = true;
    }
    return entry;
}

static void nat_free_entry(nat_entry_t *entry) {
    nat_entry_t **prev = &nat_table.active_list;
    while (*prev) {
        if (*prev == entry) {
            *prev = entry->next;
            break;
        }
        prev = &(*prev)->next;
    }
    entry->in_use = false;
    entry->next = nat_table.free_list;
    nat_table.free_list = entry;
}

static nat_entry_t *nat_find_entry(ip4_addr_t *src_ip, ip4_addr_t *dst_ip,
                                   uint16_t src_port, uint16_t dst_port,
                                   uint8_t protocol, nat_direction_t dir) {
    nat_entry_t *entry = nat_table.active_list;
    while (entry) {
        if (entry->protocol == protocol) {
            if (dir == NAT_DIR_OUTBOUND) {
                if (ip4_addr_cmp(&entry->src_ip, src_ip) &&
                    ip4_addr_cmp(&entry->dst_ip, dst_ip) &&
                    entry->src_port == src_port &&
                    entry->dst_port == dst_port) {
                    return entry;
                }
            } else {
                if (ip4_addr_cmp(&entry->dst_ip, src_ip) &&
                    ip4_addr_cmp(&entry->src_ip, dst_ip) &&
                    entry->dst_port == src_port &&
                    entry->mapped_port == dst_port) {
                    return entry;
                }
            }
        }
        entry = entry->next;
    }
    return NULL;
}

uint16_t nat_get_mapped_port(uint8_t protocol) {
    uint16_t port = nat_table.next_port++;
    if (nat_table.next_port >= 65535) {
        nat_table.next_port = 1024;
    }
    return port;
}

bool nat_translate_outbound(struct pbuf *p, struct netif *in_if, struct netif *out_if) {
    if (p->len < sizeof(struct ip4_hdr)) {
        return false;
    }

    struct ip4_hdr *iphdr = (struct ip4_hdr *)p->payload;
    uint8_t proto = IPH_PROTO(iphdr);

    if (proto != IP_PROTO_TCP && proto != IP_PROTO_UDP && proto != IP_PROTO_ICMP) {
        return false;
    }

    ip4_addr_t src_ip, dst_ip;
    ip4_addr_copy(src_ip, iphdr->src);
    ip4_addr_copy(dst_ip, iphdr->dest);

    uint16_t src_port = 0, dst_port = 0;

    if (proto == IP_PROTO_TCP || proto == IP_PROTO_UDP) {
        if (p->len < IP4_HLEN + 4) {
            return false;
        }
        uint16_t *ports = (uint16_t *)((uint8_t *)iphdr + IP4_HLEN);
        src_port = lwip_ntohs(ports[0]);
        dst_port = lwip_ntohs(ports[1]);
    }

    nat_entry_t *entry = nat_find_entry(&src_ip, &dst_ip, src_port, dst_port,
                                        proto, NAT_DIR_OUTBOUND);

    if (!entry) {
        entry = nat_alloc_entry();
        if (!entry) {
            return false;
        }
        ip4_addr_copy(entry->src_ip, src_ip);
        ip4_addr_copy(entry->dst_ip, dst_ip);
        entry->src_port = src_port;
        entry->dst_port = dst_port;
        entry->protocol = proto;
        entry->mapped_port = nat_get_mapped_port(proto);
    }

    entry->last_seen = to_ms_since_boot(get_absolute_time());

    ip4_addr_t old_src = src_ip;
    ip4_addr_copy(iphdr->src, nat_table.usb_ip);

    if (proto == IP_PROTO_TCP || proto == IP_PROTO_UDP) {
        uint16_t *ports = (uint16_t *)((uint8_t *)iphdr + IP4_HLEN);
        ports[0] = lwip_htons(entry->mapped_port);
    }

    update_ip4_checksum(iphdr);

    if (proto == IP_PROTO_TCP) {
        update_tcp_checksum(p, iphdr, &old_src, &iphdr->src, &dst_ip, &iphdr->dest);
    } else if (proto == IP_PROTO_UDP) {
        update_udp_checksum(p, iphdr, &old_src, &iphdr->src, &dst_ip, &iphdr->dest);
    } else if (proto == IP_PROTO_ICMP) {
        update_icmp_checksum(p, iphdr, &old_src, &iphdr->src, &dst_ip, &iphdr->dest);
    }

    return true;
}

bool nat_translate_inbound(struct pbuf *p, struct netif *in_if, struct netif *out_if) {
    if (p->len < sizeof(struct ip4_hdr)) {
        return false;
    }

    struct ip4_hdr *iphdr = (struct ip4_hdr *)p->payload;
    uint8_t proto = IPH_PROTO(iphdr);

    if (proto != IP_PROTO_TCP && proto != IP_PROTO_UDP && proto != IP_PROTO_ICMP) {
        return false;
    }

    ip4_addr_t src_ip, dst_ip;
    ip4_addr_copy(src_ip, iphdr->src);
    ip4_addr_copy(dst_ip, iphdr->dest);

    uint16_t src_port = 0, dst_port = 0;

    if (proto == IP_PROTO_TCP || proto == IP_PROTO_UDP) {
        if (p->len < IP4_HLEN + 4) {
            return false;
        }
        uint16_t *ports = (uint16_t *)((uint8_t *)iphdr + IP4_HLEN);
        src_port = lwip_ntohs(ports[0]);
        dst_port = lwip_ntohs(ports[1]);
    }

    nat_entry_t *entry = nat_find_entry(&dst_ip, &src_ip, dst_port, src_port,
                                        proto, NAT_DIR_INBOUND);

    if (!entry) {
        return false;
    }

    entry->last_seen = to_ms_since_boot(get_absolute_time());

    ip4_addr_t old_dst = dst_ip;
    ip4_addr_copy(iphdr->dest, entry->src_ip);

    if (proto == IP_PROTO_TCP || proto == IP_PROTO_UDP) {
        uint16_t *ports = (uint16_t *)((uint8_t *)iphdr + IP4_HLEN);
        ports[1] = lwip_htons(entry->src_port);
    }

    update_ip4_checksum(iphdr);

    if (proto == IP_PROTO_TCP) {
        update_tcp_checksum(p, iphdr, &iphdr->src, &iphdr->src, &old_dst, &iphdr->dest);
    } else if (proto == IP_PROTO_UDP) {
        update_udp_checksum(p, iphdr, &iphdr->src, &iphdr->src, &old_dst, &iphdr->dest);
    } else if (proto == IP_PROTO_ICMP) {
        update_icmp_checksum(p, iphdr, &iphdr->src, &iphdr->src, &old_dst, &iphdr->dest);
    }

    return true;
}

void nat_cleanup_expired(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (now - nat_table.last_cleanup < NAT_CLEANUP_INTERVAL_MS) {
        return;
    }
    nat_table.last_cleanup = now;

    nat_entry_t *entry = nat_table.active_list;
    nat_entry_t *prev = NULL;

    while (entry) {
        if (now - entry->last_seen > NAT_ENTRY_TIMEOUT_MS) {
            nat_entry_t *to_free = entry;
            if (prev) {
                entry = entry->next;
                prev->next = entry;
            } else {
                entry = entry->next;
                nat_table.active_list = entry;
            }
            nat_free_entry(to_free);
        } else {
            prev = entry;
            entry = entry->next;
        }
    }
}

void nat_tick(void) {
    nat_cleanup_expired();
}

void nat_dump_table(void) {
    nat_entry_t *entry = nat_table.active_list;
    printf("=== NAT Table ===\n");
    printf("Active entries:\n");
    while (entry) {
        char src[16], dst[16];
        ip4addr_ntoa_r(&entry->src_ip, src, sizeof(src));
        ip4addr_ntoa_r(&entry->dst_ip, dst, sizeof(dst));
        printf("  %s:%d -> %s:%d (mapped:%d) proto:%d age:%lums\n",
               src, entry->src_port, dst, entry->dst_port,
               entry->mapped_port, entry->protocol,
               to_ms_since_boot(get_absolute_time()) - entry->last_seen);
        entry = entry->next;
    }
    printf("=================\n");
}