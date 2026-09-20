/*
 * btstack_config.h - BTstack configuration for bluetooth_device (Pico W)
 *
 * SPDX-License-Identifier: MIT
 *
 * This is the BTstack config the stack requires in the include path
 * (see pico-sdk src/rp2_common/pico_btstack/doc.h). It selects:
 *   - Classic BT (BR/EDR) for A2DP source (audio to headphones)
 *   - BLE (optional) via pico_btstack_ble
 *   - HCI controller flow control tuned for the CYW43439 shared bus
 *   - TLV flash bank for link keys / device DB persistence
 *
 * The bulk of the tuning lives in btstack_config_common.h (copied from the
 * official pico-examples, which is the recommended production configuration).
 *
 * NOTE: no include guard here on purpose - the common config defines
 * _PICO_BTSTACK_BTSTACK_CONFIG_H and must be allowed to declare everything.
 */

/* Common tuned configuration (buffers, flow control, NVM...) */
#include "btstack_config_common.h"