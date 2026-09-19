/*
 * oled_display.h - SSD1306 128x64 OLED driver (I2C) + status rendering
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _OLED_DISPLAY_H_
#define _OLED_DISPLAY_H_

#include "pendrive.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the I2C controller and the SSD1306 (raw driver). */
void oled_init(void);

/* Render the current status using the provided config and state. */
void oled_render(const pendrive_cfg_t *cfg, const pendrive_state_t *st);

#ifdef __cplusplus
}
#endif

#endif /* _OLED_DISPLAY_H_ */
