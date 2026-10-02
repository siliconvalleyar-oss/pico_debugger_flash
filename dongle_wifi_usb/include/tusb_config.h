#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#define CFG_TUSB_MCU                      OPT_MCU_RP2040
#define CFG_TUSB_RHPORT0_MODE             OPT_MODE_DEVICE
#define CFG_TUSB_OS                       OPT_OS_NONE

#define CFG_TUD_ENDPOINT0_SIZE            64

#define CFG_TUD_CDC                       0
#define CFG_TUD_MSC                       0
#define CFG_TUD_HID                       0
#define CFG_TUD_MIDI                      0
#define CFG_TUD_VENDOR                    0
#define CFG_TUD_NET                       1

#define CFG_TUD_NET_ENDPOINT_SIZE         64
#define CFG_TUD_NET_MTU                   1500
#define CFG_TUD_NET_MAC                   USB_RNDIS_MAC

#define CFG_TUD_RNDIS                     1
#define CFG_TUD_ECM_RNDIS                 0

#define CFG_TUSB_DEBUG                    0

#define BOARD_TUD_RHPORT                  0
#define BOARD_TUD_MAX_SPEED               OPT_MODE_FULL_SPEED

#define CFG_TUD_TASK_QUEUE_SZ             32

#endif