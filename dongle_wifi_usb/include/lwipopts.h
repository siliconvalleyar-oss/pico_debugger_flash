#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define NO_SYS                          1
#define LWIP_SOCKET                     0
#define LWIP_NETCONN                    0
#define LWIP_NETIF_API                  0
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LINK_CALLBACK        1
#define LWIP_NETIF_HOSTNAME             1
#define LWIP_NETIF_HWADDRHINT           1

#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (16 * 1024)
#define MEMP_NUM_PBUF                   64
#define MEMP_NUM_UDP_PCB                16
#define MEMP_NUM_TCP_PCB                16
#define MEMP_NUM_TCP_PCB_LISTEN         8
#define MEMP_NUM_TCP_SEG                32
#define MEMP_NUM_ARP_QUEUE              16
#define MEMP_NUM_SYS_TIMEOUT            16
#define MEMP_NUM_NETBUF                 8
#define MEMP_NUM_NETCONN                8

#define PBUF_POOL_SIZE                  64
#define PBUF_POOL_BUFSIZE               1520
#define PBUF_LINK_HLEN                  16

#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_ICMP                       1
#define LWIP_RAW                        1
#define LWIP_IPV4                       1
#define LWIP_IPV6                       0
#define LWIP_DHCP                       1
#define LWIP_DHCP_BOOTP_FILE            0
#define LWIP_DHCP_CHECK_LINK_UP         1
#define LWIP_AUTOIP                     0
#define LWIP_DNS                        1
#define LWIP_DNS_API_DECLARATIONS       1
#define DNS_TABLE_SIZE                  4
#define DNS_MAX_NAME_LENGTH             256
#define DNS_MAX_SERVERS                 2
#define DNS_DOES_NAME_CHECK             1
#define DNS_USES_STATIC_BUF             1
#define LWIP_UDP                        1
#define LWIP_TCP                        1
#define TCP_MSS                         1460
#define TCP_SND_BUF                     (4 * TCP_MSS)
#define TCP_SND_QUEUELEN                16
#define TCP_WND                         (4 * TCP_MSS)
#define TCP_SNDLOWAT                    (TCP_SND_BUF / 4)
#define TCP_LISTEN_BACKLOG              1
#define TCP_QUEUE_OOSEQ                 1

#define IP_FORWARD                      1
#define IP_REASSEMBLY                   1
#define IP_FRAG                         1
#define IP_FRAG_USES_STATIC_BUF         1
#define IP_DEFAULT_TTL                  64

#define LWIP_NETIF_TX_SINGLE_PBUF       1
#define LWIP_NETIF_HWADDRHINT           1

#define LWIP_STATS                      0
#define LWIP_STATS_DISPLAY              0

#define LWIP_DBG_TYPES_ON               (LWIP_DBG_ON | LWIP_DBG_TRACE | LWIP_DBG_STATE | LWIP_DBG_FRESH | LWIP_DBG_HALT)
#define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_ALL

#define ETH_PAD_SIZE                    2
#define ETHARP_SUPPORT_VLAN             0


#define LWIP_RAND                        get_rand_32



#define LWIP_DHCP_SERVER                1
#define LWIP_DHCP_SERVER_BOOTP          0
#define LWIP_DHCP_SERVER_STATIC_LEASES  0

#define LWIP_NETIF_EXT_STATUS_CALLBACK  1

#endif