#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "hardware/watchdog.h"
#include "pico/cyw43_arch.h"
#include "tusb.h"
#include "lwip/init.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/tcpip.h"
#include "config.h"
#include "nat.h"
#include "dhcp_server.h"
#include "usb_netif.h"
#include "wifi_ap.h"
#include "ssd1306.h"

static usb_netif_t usb_netif;
static wifi_ap_t wifi_ap;
static ssd1306_t display;

static bool usb_connected = false;
static bool wifi_ap_active = false;
static int wifi_clients = 0;
static char ip_str[16] = "0.0.0.0";
static uint32_t last_status_update = 0;
static uint32_t last_led_blink = 0;
static bool led_state = false;

uint8_t tud_network_mac_address[6] = USB_RNDIS_MAC;

static void led_set(bool on) {
#if LED_ON_CYW43
    cyw43_arch_gpio_put(LED_PIN, on);
#else
    gpio_put(LED_PIN, on);
#endif
}

const uint8_t * __attribute__((used)) tud_descriptor_device_cb(void) {
    static const tusb_desc_device_t desc_device = {
        .bLength            = sizeof(tusb_desc_device_t),
        .bDescriptorType    = TUSB_DESC_DEVICE,
        .bcdUSB             = 0x0200,
        .bDeviceClass       = TUSB_CLASS_MISC,
        .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
        .bDeviceProtocol    = MISC_PROTOCOL_IAD,
        .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
        .idVendor           = USB_VENDOR_ID,
        .idProduct          = USB_PRODUCT_ID,
        .bcdDevice          = USB_DEVICE_VERSION,
        .iManufacturer      = 0x01,
        .iProduct           = 0x02,
        .iSerialNumber      = 0x03,
        .bNumConfigurations = 0x01
    };
    return (const uint8_t *)&desc_device;
}

const uint8_t * __attribute__((used)) tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    static const uint8_t desc_cfg[] = {
        0x09, 0x02, 0x00, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
        0x08, 0x0B, 0x00, 0x02, 0x02, 0x06, 0x00, 0x00,
        0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x00, 0x00,
        0x05, 0x24, 0x00, 0x10, 0x01,
        0x05, 0x24, 0x06, 0x00, 0x01,
        0x0D, 0x24, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x07, 0x05, 0x82, 0x03, 0x08, 0x00, 0x0A,
        0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
        0x07, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00,
        0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00
    };
    return desc_cfg;
}

const uint16_t * __attribute__((used)) tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t desc_str[32];
    uint8_t len = 0;
    const char *str = NULL;

    switch (index) {
        case 0x00:
            desc_str[1] = 0x0409;
            len = 1;
            break;
        case 0x01:
            str = "Raspberry Pi";
            break;
        case 0x02:
            str = "Pico WiFi Dongle";
            break;
        case 0x03:
            str = "0001";
            break;
        default:
            return NULL;
    }

    if (str) {
        for (len = 0; str[len]; len++) {
            desc_str[1 + len] = str[len];
        }
    }

    desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * len + 2);
    return desc_str;
}

void core1_entry(void) {
    while (true) {
        tud_task();
        usb_netif_poll(&usb_netif);
        wifi_ap_poll(&wifi_ap);
        nat_tick();
        dhcp_server_tick();

        uint32_t now = to_ms_since_boot(get_absolute_time());

        if (now - last_status_update >= 1000) {
            last_status_update = now;

            usb_connected = tud_ready() && usb_netif_is_connected(&usb_netif);
            wifi_ap_active = wifi_ap_is_active(&wifi_ap);
            wifi_clients = wifi_ap_get_client_count(&wifi_ap);

            if (usb_netif.netif.ip_addr.addr != 0) {
                ip4addr_ntoa_r(&usb_netif.netif.ip_addr, ip_str, sizeof(ip_str));
            } else {
                strcpy(ip_str, "0.0.0.0");
            }

            ssd1306_show_status(&display, usb_connected, wifi_ap_active,
                                wifi_clients, ip_str);
        }

        if (now - last_led_blink >= (usb_connected ? LED_BLINK_CONNECTED_MS : LED_BLINK_DISCONNECTED_MS)) {
            last_led_blink = now;
            led_state = !led_state;
            led_set(led_state);
        }

        if (watchdog_caused_reboot()) {
            printf("Rebooted by watchdog\n");
        }
        watchdog_update();
    }
}

int main(void) {
    stdio_init_all();

#if LED_ON_CYW43
#else
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
#endif

    if (watchdog_caused_reboot()) {
        printf("Rebooted by watchdog!\n");
    }
    watchdog_enable(WATCHDOG_TIMEOUT_MS, 1);

    printf("\n=== Pico 1W USB -> WiFi Dongle ===\n");
    printf("Iniciando...\n");

    ssd1306_init(&display);
    ssd1306_show_status(&display, false, false, 0, "Iniciando...");

    uint8_t usb_mac[6] = USB_RNDIS_MAC;
    uint8_t ap_mac[6] = WIFI_AP_MAC;

    if (!usb_netif_init(&usb_netif, usb_mac)) {
        printf("Error: USB netif init failed\n");
        ssd1306_show_error(&display, "USB init failed");
        while (1) tight_loop_contents();
    }
    printf("USB netif inicializado\n");

    if (!wifi_ap_init(&wifi_ap, ap_mac, WIFI_SSID, WIFI_PASSWORD,
                      WIFI_CHANNEL, WIFI_AUTH, WIFI_COUNTRY)) {
        printf("Error: WiFi AP init failed\n");
        ssd1306_show_error(&display, "WiFi init failed");
        while (1) tight_loop_contents();
    }
    printf("WiFi AP iniciado: %s\n", WIFI_SSID);

    ip4_addr_t usb_ip, ap_ip;
    ip4_addr_copy(usb_ip, usb_netif.netif.ip_addr);
    ip4_addr_copy(ap_ip, wifi_ap.netif.ip_addr);

    nat_init(&usb_ip, &ap_ip);
    printf("NAT inicializado\n");

    ip4_addr_t ap_netmask, ap_gw, ap_dns;
    ip4addr_aton(AP_NETMASK, &ap_netmask);
    ip4addr_aton(AP_GATEWAY, &ap_gw);
    ip4addr_aton(AP_GATEWAY, &ap_dns);

    dhcp_server_init(&ap_ip, &ap_netmask, &ap_gw, &ap_dns);
    printf("DHCP server iniciado en %s\n", AP_IP_ADDR);

    multicore_launch_core1(core1_entry);

    printf("Sistema listo. Nucleo 1 ejecutando loop principal.\n");
    printf("USB: RNDIS/CDC-ECM, WiFi AP: %s\n", WIFI_SSID);

    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_led_blink >= (usb_connected ? LED_BLINK_CONNECTED_MS : LED_BLINK_DISCONNECTED_MS)) {
            last_led_blink = now;
            led_state = !led_state;
            led_set(led_state);
        }

        watchdog_update();
        sleep_ms(MAIN_LOOP_DELAY_MS);
    }

    return 0;
}