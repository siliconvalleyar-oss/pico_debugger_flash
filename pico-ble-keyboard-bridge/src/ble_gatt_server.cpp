#include "ble_gatt_server.h"
#include "usb_hid_keyboard.h"
#include "ascii_to_hid.h"

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include <cstring>
#include <cstdio>

// Generado por pico_btstack_make_gatt_header (compile_gatt.py) a partir de
// gatt/pico_kb_bridge.gatt; el SDK lo nombra <nombre_sin_ext>.h.
#include "pico_kb_bridge.h"

namespace {

constexpr const char *kDeviceName = "Pico-KB-Bridge";
constexpr size_t kMaxCmdLen = 128;

hci_con_handle_t g_con_handle = HCI_CON_HANDLE_INVALID;
btstack_packet_callback_registration_t g_hci_event_cb_registration;
btstack_packet_callback_registration_t g_sm_event_cb_registration;

// Buffer de advertising: nombre + flags + UUID de servicio
uint8_t g_adv_data[31];
uint8_t g_adv_data_len;

/// Enciende/apaga el LED onboard de la Pico W (vía el chip CYW43).
void set_led(bool on) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on ? 1 : 0);
}

/// Interpreta un comando especial con prefijo '\' y lo traduce a una
/// pulsación de tecla o a un delay encolado en usb_hid_keyboard.
/// Devuelve true si "cmd" era efectivamente un comando reconocido.
bool handle_special_command(const char *cmd) {
    if (strcmp(cmd, "\\ENTER") == 0) {
        usb_kbd_press(HidKey::kEnter, HidMod::kNone);
        return true;
    }
    if (strcmp(cmd, "\\TAB") == 0) {
        usb_kbd_press(HidKey::kTab, HidMod::kNone);
        return true;
    }
    if (strcmp(cmd, "\\ESC") == 0) {
        usb_kbd_press(HidKey::kEscape, HidMod::kNone);
        return true;
    }
    if (strcmp(cmd, "\\DEL") == 0) {
        usb_kbd_press(HidKey::kBackspace, HidMod::kNone);
        return true;
    }
    if (strcmp(cmd, "\\SLEEP") == 0) {
        usb_kbd_delay(500);
        return true;
    }
    return false;
}

/// Procesa el payload crudo recibido por la característica de escritura.
/// Soporta múltiples comandos "\XXX" separados por espacios dentro del
/// mismo paquete, y texto plano en cualquier otra posición.
void process_incoming_payload(const uint8_t *data, uint16_t len) {
    char buf[kMaxCmdLen + 1];
    uint16_t copy_len = (len > kMaxCmdLen) ? kMaxCmdLen : len;
    memcpy(buf, data, copy_len);
    buf[copy_len] = '\0';

    // Tokenizamos buscando '\' como inicio de comando especial; todo lo
    // demás se manda tal cual a usb_kbd_type_string().
    const char *p = buf;
    while (*p != '\0') {
        if (*p == '\\') {
            // Buscamos el próximo separador (espacio) o el fin de cadena
            const char *end = strchr(p, ' ');
            char token[32];
            size_t token_len = end ? (size_t)(end - p) : strlen(p);
            if (token_len >= sizeof(token)) token_len = sizeof(token) - 1;
            memcpy(token, p, token_len);
            token[token_len] = '\0';

            if (!handle_special_command(token)) {
                // No era un comando reconocido: se tipea literalmente
                usb_kbd_type_string(token);
            }
            p += token_len;
        } else {
            const char *next_backslash = strchr(p, '\\');
            size_t plain_len = next_backslash ? (size_t)(next_backslash - p) : strlen(p);
            char plain[kMaxCmdLen + 1];
            if (plain_len > kMaxCmdLen) plain_len = kMaxCmdLen;
            memcpy(plain, p, plain_len);
            plain[plain_len] = '\0';
            usb_kbd_type_string(plain);
            p += plain_len;
        }
        if (*p == ' ') p++; // saltar separador entre tokens
    }
}

/// Callback de ATT: se dispara cuando el cliente BLE (ya bonded) escribe
/// en nuestra característica.
int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle,
                        uint16_t transaction_mode, uint16_t offset,
                        uint8_t *buffer, uint16_t buffer_size) {
    (void)con_handle; (void)transaction_mode; (void)offset;

    if (att_handle == ATT_CHARACTERISTIC_0000FFE1_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE) {
        process_incoming_payload(buffer, buffer_size);
    }
    return 0; // 0 = ATT_ERROR_SUCCESS
}

/// Callback central de eventos HCI/GAP/SM (conexión, desconexión, bonding).
void hci_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    (void)channel; (void)size;
    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t event_type = hci_event_packet_get_type(packet);

    switch (event_type) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                ble_gatt_server_start_advertising();
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            g_con_handle = HCI_CON_HANDLE_INVALID;
            set_led(false);
            ble_gatt_server_start_advertising(); // volver a ser visible
            break;

        case HCI_EVENT_LE_META:
            if (hci_event_le_meta_get_subevent_code(packet) ==
                HCI_SUBEVENT_LE_CONNECTION_COMPLETE) {
                g_con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                // El LED se enciende recién cuando el bonding/encryption
                // se confirma (ver evento SM), no en la mera conexión.
            }
            break;

        default:
            break;
    }
}

/// Callback de eventos del Security Manager (emparejamiento / bonding).
void sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    (void)channel; (void)size;
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            // "Just Works": aceptamos automáticamente, sin pedir
            // confirmación de PIN (apto para uso doméstico/personal).
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;

        case SM_EVENT_PAIRING_COMPLETE:
            if (sm_event_pairing_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
                set_led(true); // conexión emparejada y encriptada: LED fijo
            }
            break;

        default:
            break;
    }
}

} // namespace

void ble_gatt_server_init(void) {
    l2cap_init();
    sm_init();

    // Bonding obligatorio + "Just Works" (sin requerir teclado/pantalla
    // en el dispositivo remoto). LE Secure Connections cuando el
    // dispositivo remoto lo soporte.
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING | SM_AUTHREQ_SECURE_CONNECTION);

    att_server_init(profile_data, nullptr, att_write_callback);

    g_hci_event_cb_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&g_hci_event_cb_registration);

    g_sm_event_cb_registration.callback = &sm_packet_handler;
    sm_add_event_handler(&g_sm_event_cb_registration);

    // Advertising: flags + nombre completo
    g_adv_data_len = 0;
    g_adv_data[g_adv_data_len++] = 0x02; // length
    g_adv_data[g_adv_data_len++] = BLUETOOTH_DATA_TYPE_FLAGS;
    g_adv_data[g_adv_data_len++] = 0x06; // LE General Discoverable + BR/EDR not supported

    size_t name_len = strlen(kDeviceName);
    g_adv_data[g_adv_data_len++] = (uint8_t)(name_len + 1);
    g_adv_data[g_adv_data_len++] = BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME;
    memcpy(&g_adv_data[g_adv_data_len], kDeviceName, name_len);
    g_adv_data_len += name_len;

    hci_power_control(HCI_POWER_ON);
}

void ble_gatt_server_start_advertising(void) {
    uint16_t adv_int_min = 0x0030; // ~30ms
    uint16_t adv_int_max = 0x0030;
    bd_addr_t null_addr{};

    gap_advertisements_set_params(adv_int_min, adv_int_max, 0, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(g_adv_data_len, g_adv_data);
    gap_advertisements_enable(1);
}
