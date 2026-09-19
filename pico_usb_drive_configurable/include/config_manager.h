/*
 * config_manager.h - Runtime configuration (config.txt) reader and hot-plug watcher
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _CONFIG_MANAGER_H_
#define _CONFIG_MANAGER_H_

#include "pendrive.h"

#ifdef __cplusplus
extern "C" {
#endif

/* File name used for runtime configuration (in the pendrive root). */
#define CONFIG_FILE      "config.txt"

/* Initialise: load config.txt (or auto-create it with config.h defaults). */
void config_manager_init(void);

/* Hot-plug watcher: poll the file every CONFIG_POLL_INTERVAL_MS and re-apply
 * any change (volume label, read-only flag, etc.) without a reboot. */
void config_manager_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* _CONFIG_MANAGER_H_ */
