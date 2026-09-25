#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* microSD card over SPI1.  Block size fixed at 512 bytes (SDHC). */

bool sd_init(void);
bool sd_read_block(uint32_t lba, uint8_t *buf);
bool sd_write_block(uint32_t lba, const uint8_t *buf);
uint32_t sd_card_capacity(void); /* total 512B blocks, 0 if unknown */

/* Debug/test helpers */
void sd_test_read_block0(uint8_t *buf, char *output, size_t out_len);

#ifdef __cplusplus
}
#endif