#ifndef CONFIG_H
#define CONFIG_H

#define WIFI_SSID                       "PicoDongle"
#define WIFI_PASSWORD                   "pico1234"
#define WIFI_CHANNEL                    6
#define WIFI_AUTH                       CYW43_AUTH_WPA2_AES_PSK
#define WIFI_COUNTRY                    CYW43_COUNTRY_WORLDWIDE

#define AP_IP_ADDR                      "192.168.4.1"
#define AP_NETMASK                      "255.255.255.0"
#define AP_GATEWAY                      "192.168.4.1"
#define AP_DHCP_START                   "192.168.4.2"
#define AP_DHCP_END                     "192.168.4.20"
#define AP_DHCP_LEASE_TIME              3600

#define USB_VENDOR_ID                   0x2E8A
#define USB_PRODUCT_ID                  0x000A
#define USB_DEVICE_VERSION              0x0100

#define USB_RNDIS_MAC                   {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}
#define WIFI_AP_MAC                     {0x02, 0x00, 0x00, 0x00, 0x00, 0x02}

#define NAT_MAX_ENTRIES                 256
#define NAT_ENTRY_TIMEOUT_MS            300000
#define NAT_CLEANUP_INTERVAL_MS         60000

#define WATCHDOG_TIMEOUT_MS             8000
#define MAIN_LOOP_DELAY_MS              10
#define USB_POLL_INTERVAL_MS            1
#define LWIP_TIMER_INTERVAL_MS          100

#define MAX_WIFI_CLIENTS                8
#define MAX_USB_MTU                     1500
#define MAX_WIFI_MTU                    1500

#define DEBUG_LEVEL                     1

#define LED_PIN                         PICO_DEFAULT_LED_PIN
#define LED_BLINK_CONNECTED_MS          1000
#define LED_BLINK_DISCONNECTED_MS       200
#define LED_BLINK_ERROR_MS              50

#endif