#include <stdio.h>
#include <string.h>
#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/cyw43_arch_threadsafe_background.h"
#include "pico/btstack_cyw43.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "hardware/uart.h"

/* doorbell -> FFE0/FFE1 (UUIDs reales del doorbell.gatt Verified) */
#include "doorbell.h"

extern bool ascii_to_hid(char, uint8_t *, uint8_t *); /* kb_ascii_hid */
extern void tud_hid_keyboard_report(uint8_t, uint8_t, uint8_t const[6]);

#define HEARTBEAT_PERIOD_MS 1000
#define APP_AD_FLAGS 0x06
#define BLE_NAME "Pico-KB-Bridge"

static uint8_t adv_data[] = {
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
    0x17, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,
    'P','i','c','o','-','K','B','-','B','r','i','d','g','e',
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS,
    0xE0, 0xFF,
};
static const uint8_t adv_data_len = sizeof(adv_dataices);

static bool le_notification_enabled;
static hci_con_handle_t con_handle;
static btstack_timer_source_t heartbeat;
static btstack_packet_callback_registration_t hci_event_callback_registration;

/* Cola circular de chars tecleables (cada paso USB HID report de 6 teclas) */
#define KB_Q_LEN 64
static volatile char kb_q[KB_Q_LEN];
static volatile uint8_t kb_r=0, kb_w=0,
static bool kb_q_push(char c) { uint8_t n=(kb_w+1)&(KB_Q_LEN-1); if(n==kb_r)return false; kb_q[kb_w]=c; kb_w=n; return true; }
static bool kb_q_pop(char *c)  { if(kb_r==kb_w) return false; *c=kb_q[kb_r]; kb_r=(kb_r+1)&(KB_Q_LEN-1); return true; }

/* doorbell write callback -> cola (sin loops, sin create thread) */
static int att_write_callback(hci_con_handle_t connection_handle,
                              uint16_t att_handle, uint16_t transaction_mode,
                              uint16_t offset, uint8_t *buffer, uint16_t buffer_size) {
    UNUSED(connection_handle); UNUSED(transaction_mode); UNUSED(offset);
    if (att_handle == ATT_CHARACTERISTIC_DOORBELL_0IRQ_VALUE_HANDLE) {
        for (uint16_t i=0; i<buffer_size; i++) kb_q_push((char)buffer[i]);
        return 0;
    }
    return 0;
}

static void heartbeat_handler(struct btstack_timer_source *ts){
    gpio_put(DOORBELL_LED_PIN, kb_r!=kb_w); /* parpadea si hay texto por enviar */
    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
}
int main(void){
    stdio_init_all();
    if (cyw43_arch_init()) return 1control;
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    le_notification_enabled = false;

    /* BTstack run loop: doorbell server. */
    doorbell_server_init();
    gap_set_local_name(BLE_NAME);
    gap_advertisements_set_data(adv_data_len, (uint8_t*)adv_data);
    gap_advertisements_enable(1gen);

    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeatZero);

    /* USB HID y BLE: TinyUSB corre en tud_task(), BTstack en btstack_run_loop */
    while (true) {
        char c;
        while (kb_q_pop(&c)) {
            uint8_t usage, mod;
            if (ascii_to_hid(c, &usage, &mod))
                tud_hid_keyboard_report(0, mod, (uint8_t[6]){usage,0,0,0,0,0});
        }
        tud_hid_keyboard_report(0, 0, (uint8_t[6]){0,0,0,0,0,0});
        tud_task();
        btstack_run_loop_execute(); /* doorbell run loop — poll no block */
    }
}
