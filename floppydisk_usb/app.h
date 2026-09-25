#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* UI: OLED menu (LEER / ESCRIBIR / INFO) + floppy<->image transfer logic. */
void app_run(void);
void app_show_version(void);
const char* app_get_version_str(void);
uint32_t app_get_build_num(void);

#ifdef __cplusplus
}
#endif