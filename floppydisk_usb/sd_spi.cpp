#include "sd_spi.h"

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "config.h"

static bool card_ok = false;

static void cs_high(void) { gpio_put(SD_CS_PIN, 1); }
static void cs_low(void) { gpio_put(SD_CS_PIN, 0); }

static uint8_t xchg(uint8_t b) {
    uint8_t r;
    spi_write_read_blocking(SD_SPI, &b, &r, 1);
    return r;
}

static void dummy_clocks(int n) {
    while (n--) (void)xchg(0xFF);
}

/* Send a command with argument and a manually computed CRC7 (only CMD0 and
 * CMD8 need a valid CRC in SPI mode; the rest use 0x00). */
static uint8_t sd_cmd(uint8_t idx, uint32_t arg, uint8_t crc) {
    uint8_t buf[6] = {uint8_t(0x40 | idx),
                      uint8_t(arg >> 24), uint8_t(arg >> 16),
                      uint8_t(arg >> 8), uint8_t(arg), crc};
    spi_write_blocking(SD_SPI, buf, 6);
    /* wait for a response (0xFF while card is busy) */
    for (int i = 0; i < 64; i++) {
        uint8_t r = xchg(0xFF);
        if (!(r & 0x80)) return r;
    }
    return 0xFF;
}

static uint8_t crc7_cmd(uint8_t idx, uint32_t arg) {
    uint8_t crc = 0;
    uint8_t buf[5] = {uint8_t(0x40 | idx),
                      uint8_t(arg >> 24), uint8_t(arg >> 16),
                      uint8_t(arg >> 8), uint8_t(arg)};
    for (int i = 0; i < 5; i++) {
        crc ^= buf[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 0x09) : uint8_t(crc << 1);
        }
    }
    return uint8_t((crc << 1) | 1);
}

static bool wait_ready(void) {
    for (int i = 0; i < 100000; i++) {
        if (xchg(0xFF) == 0xFF) return true;
    }
    return false;
}

bool sd_init(void) {
    spi_init(SD_SPI, SD_INIT_BAUD);
    gpio_set_function(SD_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SD_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SD_MISO_PIN, GPIO_FUNC_SPI);
    gpio_init(SD_CS_PIN);
    gpio_set_dir(SD_CS_PIN, GPIO_OUT);
    cs_high();

    card_ok = false;

    /* >74 dummy clocks with CS de-asserted */
    dummy_clocks(10);
    cs_low();
    dummy_clocks(1);

    for (int i = 0; i < 10; i++) {
        if (sd_cmd(0, 0, crc7_cmd(0, 0)) == 0x01) break;
    }

    /* Try CMD8 (SDHC/SDXC detection) */
    uint8_t cmd8_r = sd_cmd(8, 0x000001AA, crc7_cmd(8, 0x000001AA));
    bool is_v2_sd = (cmd8_r == 0x01);
    
    if (is_v2_sd) {
        /* read R7: */
        uint8_t r7[4];
        for (int i = 0; i < 4; i++) r7[i] = xchg(0xFF);
        if (r7[2] != 0x01 || r7[3] != 0xAA) {
            cs_high();
            return false;
        }
    }

    bool sdhc = false;
    if (is_v2_sd) {
        /* ACMD41 with HCS for SDHC */
        for (int i = 0; i < 1000; i++) {
            sd_cmd(55, 0, 0);          /* CMD55, next is ACMD */
            uint8_t r = sd_cmd(41, 0x40000000, 0); /* ACMD41 HCS=1 */
            if (r == 0x00) {
                sdhc = true;
                break;
            }
            busy_wait_us(500);
        }
    }
    /* try without HCS (SDSC) - works for both v2 SDSC and v1 cards */
    if (!sdhc) {
        for (int i = 0; i < 1000; i++) {
            sd_cmd(55, 0, 0);
            if (sd_cmd(41, 0, 0) == 0x00) break;
            busy_wait_us(500);
        }
    }

    /* read OCR to determine card type */
    uint8_t r = sd_cmd(58, 0, 0);
    uint8_t ocr[4];
    for (int i = 0; i < 4; i++) ocr[i] = xchg(0xFF);
    bool ccs = ocr[0] & 0x40; /* bit 30 */

    if (!ccs) {
        /* SDSC: set block length to 512 */
        sd_cmd(16, 512, 0);
    }

    spi_set_baudrate(SD_SPI, SD_RUN_BAUD);
    card_ok = true;
    return true;
}

uint32_t sd_card_capacity(void) {
    if (!card_ok) return 0;
    if (sd_cmd(9, 0, 0) != 0x00) return 0; /* CMD9 read CSD */
    uint8_t csd[16];
    if (xchg(0xFF) != 0xFE) return 0;
    for (int i = 0; i < 16; i++) csd[i] = xchg(0xFF);
    for (int i = 0; i < 2; i++) (void)xchg(0xFF); /* CRC16 */

    uint32_t c_size = ((uint32_t)(csd[6] & 0x03) << 10) | ((uint32_t)csd[7] << 2) |
                      ((uint32_t)csd[8] >> 6);
    uint32_t c_size_mult = ((csd[9] & 0x03) << 1) | (csd[10] >> 7);
    uint32_t read_bl_len = csd[5] & 0x0F;

    uint32_t blocks = (c_size + 1) << (c_size_mult + 2 + read_bl_len - 9);
    return blocks;
}

static bool read_data_block(uint8_t *buf) {
    for (int i = 0; i < 64; i++) {
        uint8_t r = xchg(0xFF);
        if (r == 0xFE) {
            spi_read_blocking(SD_SPI, 0xFF, buf, 512);
            dummy_clocks(2);
            return true;
        }
        if (r == 0x00) break; /* data error */
    }
    return false;
}

bool sd_read_block(uint32_t lba, uint8_t *buf) {
    if (!card_ok) return false;
    cs_low();
    uint8_t r = sd_cmd(17, lba, 0);
    bool ok = (r == 0x00) && read_data_block(buf);
    cs_high();
    dummy_clocks(1);
    return ok;
}

static bool write_data_block(const uint8_t *buf) {
    uint8_t tok = 0xFE;
    spi_write_blocking(SD_SPI, &tok, 1);
    spi_write_blocking(SD_SPI, buf, 512);
    dummy_clocks(2); /* CRC accepted by card, we ignore values */
    uint8_t r = xchg(0xFF);
    if ((r & 0x1F) != 0x05) return false; /* data accepted */
    return wait_ready();
}

bool sd_write_block(uint32_t lba, const uint8_t *buf) {
    if (!card_ok) return false;
    cs_low();
    uint8_t r = sd_cmd(24, lba, 0);
    bool ok = (r == 0x00) && write_data_block(buf);
    cs_high();
    dummy_clocks(1);
    return ok;
}