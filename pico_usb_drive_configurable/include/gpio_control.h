/*
 * gpio_control.h - Status LED control (blink patterns)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _GPIO_CONTROL_H_
#define _GPIO_CONTROL_H_

#include "pendrive.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Configure the LED GPIO as output, off by default. */
void gpio_led_init(void);

/* Blink patterns selected by the orchestrator (see pendrive.h). */
void gpio_led_set_fast_blink(void);
void gpio_led_set_solid(void);
void gpio_led_set_slow_blink(void);
void gpio_led_set_off(void);

/* Non-blocking pattern tick; call frequently from the main loop. */
void gpio_led_task(void);

#ifdef __cplusplus
}
#endif

#endif /* _GPIO_CONTROL_H_ */
