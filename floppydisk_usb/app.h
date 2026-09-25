#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "fat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* UI: OLED menu (LEER / ESCRIBIR / INFO) + floppy<->image transfer logic. */
void app_run(void);
void app_show_version(void);
const char* app_get_version_str(void);
uint32_t app_get_build_num(void);

/* FAT access for shell commands */
bool app_fat_ok(void);
uint32_t app_fat_free_kb(void);
uint32_t app_fat_total_kb(void);
int app_fat_list_images(fat_scan_entry_t *out, int maxn);
bool app_fat_read_image_block(const char *base8, const char *ext3, uint32_t block_idx, uint8_t *buf);
uint32_t app_fat_image_size(const char *base8, const char *ext3);

#ifdef __cplusplus
}
#endif