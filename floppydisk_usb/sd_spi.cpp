/*
 * sd_spi.c - Driver de microSD en modo SPI para RP2040 (SPI1)
 *
 * Secuencia de inicializacion (Physical Layer Simplified Spec 6.0):
 *   1. SPI a <=400 kHz, CS alto, 80 clocks, espera >=1 ms
 *   2. CMD0 (GO_IDLE_STATE)  -> R1 = 0x01
 *   3. CMD8 (SEND_IF_COND, 0x1AA) -> R7 (valida SDv2)
 *   4. ACMD41 (SD_SEND_OP_COND, HCS=1) hasta 0x00 (timeout ~1 s)
 *   5. CMD58 (OCR) -> bit CCS: 1 = SDHC/SDXC (bloques de 512 B)
 *   6. CMD59 (CRC_OFF), CMD16 (SET_BLOCKLEN 512), SPI a 10 MHz
 *
 * Lectura: CMD17 + token 0xFE + 512 bytes + 2 CRC
 * Escritura: CMD24 + token 0xFE + 512 bytes + 2 CRC + espera busy
 */

#include <string.h>
#include <cstdio>
#include "sd_spi.h"
#include "shell.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "config.h"

//--------------------------------------------------------------------+
// Comandos SD
//--------------------------------------------------------------------+
#define CMD0    (0)    // GO_IDLE_STATE
#define CMD1    (1)    // SEND_OP_COND (MMC)
#define CMD8    (8)    // SEND_IF_COND
#define CMD9    (9)    // SEND_CSD
#define CMD12   (12)   // STOP_TRANSMISSION
#define CMD16   (16)   // SET_BLOCKLEN
#define CMD17   (17)   // READ_SINGLE_BLOCK
#define CMD18   (18)   // READ_MULTIPLE_BLOCK
#define CMD24   (24)   // WRITE_SINGLE_BLOCK
#define CMD55   (55)   // APP_CMD
#define CMD58   (58)   // READ_OCR
#define CMD59   (59)   // CRC_ON_OFF
#define ACMD41  (41)   // SD_SEND_OP_COND (precedido de CMD55)

// Respuestas R1
#define R1_IDLE          0x01
#define R1_ERASE_RESET   0x02
#define R1_ILLEGAL_CMD   0x04
#define R1_CRC_ERR       0x08
#define R1_ERASE_SEQ_ERR 0x10
#define R1_ADDR_ERR      0x20
#define R1_PARAM_ERR     0x40
#define R1_READY         0x00

// Tokens de datos
#define DATA_TOKEN_START     0xFE  // bloque unico
#define DATA_RESP_MASK       0x1F
#define DATA_RESP_ACCEPTED   0x05

#define SD_TIMEOUT_MS        1000
#define SD_ACMD41_TRIES      2000  // ~1 s con sleep_ms(1)
#define SD_SECTOR_SIZE       512

// Tipos de tarjeta
typedef enum {
    SD_TYPE_UNKNOWN = 0,
    SD_TYPE_MMC,
    SD_TYPE_V1,
    SD_TYPE_V2,
    SD_TYPE_V2_HC
} sd_type_t;

//--------------------------------------------------------------------+
// Estado
//--------------------------------------------------------------------+
static sd_type_t s_type = SD_TYPE_UNKNOWN;
static bool card_sdhc = false;
static bool card_ok = false;

//--------------------------------------------------------------------+
// Helpers SPI
//--------------------------------------------------------------------+
static void sd_cs_select(void)   { gpio_put(SD_CS_PIN, 0); }
static void sd_cs_deselect(void) { gpio_put(SD_CS_PIN, 1); }

static void spi_tx(uint8_t b) {
    spi_write_blocking(SD_SPI, &b, 1);
}

static uint8_t spi_xfer(uint8_t b) {
    uint8_t r;
    spi_write_read_blocking(SD_SPI, &b, &r, 1);
    return r;
}

// Libera el bus MISO (tarjeta puede quedar reteniendo el ultimo bit)
static void spi_release(void) {
    for (int i = 0; i < 8; i++) spi_xfer(0xFF);
}

// Espera hasta que la tarjeta deja de ocupar el bus (devuelve 0xFF)
static bool sd_wait_ready(uint32_t timeout_ms) {
    uint64_t deadline = time_us_64() + (uint64_t) timeout_ms * 1000u;
    do {
        if (spi_xfer(0xFF) == 0xFF) return true;
    } while (time_us_64() < deadline);
    return false;
}

static void sd_crc(uint8_t crc_on) {
    sd_cs_select();
    // CMD59: CRC_ON_OFF, arg = crc_on, CRC dummy 0x01 (valido en CMD0 y CMD59)
    spi_tx(0x40 | CMD59);
    spi_tx(0x00); spi_tx(0x00); spi_tx(0x00); spi_tx(crc_on);
    spi_tx(0x01);
    spi_xfer(0xFF); // consume R1
    sd_cs_deselect();
    spi_xfer(0xFF);
}

static uint8_t sd_command(uint8_t cmd, uint32_t arg) {
    // Esperar ready ANTES de seleccionar CS (excepto CMD0 y CMD12)
    if (cmd != CMD0 && cmd != CMD12) {
        if (!sd_wait_ready(SD_TIMEOUT_MS)) return 0xFF;
    }
    sd_cs_select();

    // Calcular CRC para CMD0 y CMD8 (el resto usa dummy 0x01)
    uint8_t crc = 0x01;
    if (cmd == CMD0) crc = 0x95;  // CRC7 valido para CMD0 arg=0
    if (cmd == CMD8) crc = 0x87;  // CRC7 valido para CMD8 arg=0x1AA

    spi_tx(0x40 | cmd);
    spi_tx((uint8_t)(arg >> 24));
    spi_tx((uint8_t)(arg >> 16));
    spi_tx((uint8_t)(arg >> 8));
    spi_tx((uint8_t)arg);
    spi_tx(crc);

    // R1: esperar respuesta (bit 7 = 0 significa valida)
    uint8_t resp = 0xFF;
    for (int i = 0; i < 16; i++) {
        resp = spi_xfer(0xFF);
        if ((resp & 0x80) == 0) break; // bit 7 = 0 -> respuesta valida
    }
    return resp;
}

static uint8_t sd_acmd(uint8_t cmd, uint32_t arg) {
    sd_command(CMD55, 0);
    return sd_command(cmd, arg);
}

// Lee el resto de una respuesta multibyte (R2..R7) ya emitido el primer byte
static void sd_read_bytes(uint8_t *buf, uint32_t n) {
    while (n--) *buf++ = spi_xfer(0xFF);
}

static void sd_end_command(void) {
    spi_release();
    sd_cs_deselect();
    spi_xfer(0xFF); // marginar CS
}

//--------------------------------------------------------------------+
// Lectura / escritura de bloques
//--------------------------------------------------------------------+
static bool sd_read_block(uint8_t *buf, uint32_t n) {
    uint64_t deadline = time_us_64() + (uint64_t)SD_TIMEOUT_MS * 1000;
    uint8_t tok;
    do {
        tok = spi_xfer(0xFF);
        if (tok == DATA_TOKEN_START) break;
        if (time_us_64() > deadline) return false;
    } while (true);

    for (uint32_t i = 0; i < n; i++) *buf++ = spi_xfer(0xFF);
    spi_xfer(0xFF); // CRC
    spi_xfer(0xFF);
    return true;
}

static bool sd_write_block(const uint8_t *buf, uint32_t n) {
    spi_tx(DATA_TOKEN_START);
    for (uint32_t i = 0; i < n; i++) spi_tx(*buf++);
    spi_xfer(0xFF); // CRC
    spi_xfer(0xFF);

    uint8_t resp = spi_xfer(0xFF);
    if ((resp & DATA_RESP_MASK) != DATA_RESP_ACCEPTED) return false;

    return sd_wait_ready(SD_TIMEOUT_MS * 4);
}

//--------------------------------------------------------------------+
// Inicializacion
//--------------------------------------------------------------------+
bool sd_init(void) {
    s_type = SD_TYPE_UNKNOWN;
    card_sdhc = false;
    card_ok = false;

    // SPI a 400 kHz, CPOL=0 CPHA=0 (SPI Mode 0), bits MSB-first
    spi_init(SD_SPI, SD_INIT_BAUD);
    spi_set_format(SD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(SD_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SD_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SD_MISO_PIN, GPIO_FUNC_SPI);
    // Con CS alto la tarjeta libera MISO; el pull-up garantiza leer 0xFF
    // (necesario para sd_wait_ready() y la deteccion de tarjeta ausente).
    gpio_pull_up(SD_MISO_PIN);

    // CS como GPIO con push-pull; idle alto
    gpio_init(SD_CS_PIN);
    gpio_set_dir(SD_CS_PIN, GPIO_OUT);
    sd_cs_deselect();

    sleep_ms(2);

    // >= 74 clocks con CS alto y DI/MOSI alto (10 x 8 = 80 clocks)
    for (int i = 0; i < 10; i++) spi_release();
    sleep_ms(1);

    // CMD0: GO_IDLE_STATE (hasta 200 intentos: algunas tarjetas demoran)
    uint8_t r1 = 0xFF;
    for (int i = 0; i < 200; i++) {
        r1 = sd_command(CMD0, 0);
        sd_end_command();
        if (r1 == R1_IDLE) break;
        sleep_ms(1);
    }
    if (r1 != R1_IDLE) return false;

    // CMD8: SEND_IF_COND 0x1AA (solo SDv2)
    bool is_v2 = false;
    uint8_t resp[4];
    r1 = sd_command(CMD8, 0x1AA);
    if (r1 == R1_IDLE) {
        sd_read_bytes(resp, 4);
        if (resp[3] == 0xAA) is_v2 = true; // eco correcto
    }
    // MMC/SDv1 devuelven illegal command (sin payload)
    sd_end_command();

    // ACMD41 / CMD1: esperar que la tarjeta salga de idle (HCS=1 para SDv2)
    bool ok = false;
    for (int i = 0; i < SD_ACMD41_TRIES; i++) {
        if (is_v2) {
            r1 = sd_acmd(ACMD41, 1u << 30); // HCS
        } else {
            r1 = sd_acmd(ACMD41, 0);
            if (r1 & R1_ILLEGAL_CMD) {
                // tarjeta MMC: usar CMD1
                r1 = sd_command(CMD1, 0);
                s_type = SD_TYPE_MMC;
            }
        }
        if (r1 == R1_READY) { ok = true; break; }
        sleep_ms(1);
    }
    sd_end_command();
    if (!ok) return false;

    // CMD58: leer OCR para detectar SDHC (CCS)
    if (s_type != SD_TYPE_MMC) {
        r1 = sd_command(CMD58, 0);
        if (r1 == R1_READY) {
            sd_read_bytes(resp, 4);
            uint32_t ocr = ((uint32_t)resp[0] << 24) | ((uint32_t)resp[1] << 16) |
                           ((uint32_t)resp[2] << 8) | resp[3];
            if (is_v2) {
                s_type = (ocr & (1u << 30)) ? SD_TYPE_V2_HC : SD_TYPE_V2;
            } else {
                s_type = SD_TYPE_V1;
            }
        }
    }
    sd_end_command();

    // CRC off y blocklen 512
    sd_crc(0);

    sd_cs_select();
    r1 = sd_command(CMD16, SD_SECTOR_SIZE);
    sd_end_command();
    if (r1 != R1_READY) return false;

    // Modo rapido de transferencia
    spi_set_baudrate(SD_SPI, SD_RUN_BAUD);

    card_ok = true;
    card_sdhc = (s_type == SD_TYPE_V2_HC);
    return true;
}

sd_type_t sd_type(void) { return card_ok ? s_type : SD_TYPE_UNKNOWN; }

const char *sd_type_name(void) {
    switch (s_type) {
        case SD_TYPE_MMC:   return "MMC";
        case SD_TYPE_V1:    return "SD1";
        case SD_TYPE_V2:    return "SD2";
        case SD_TYPE_V2_HC: return "SDHC";
        default:            return "?";
    }
}

uint32_t sd_card_capacity(void) {
    if (!card_ok) return 0;

    // Leer CSD (CMD9) y calcular capacidad
    uint8_t csd[16];
    uint8_t r1 = sd_command(CMD9, 0);
    if (r1 != R1_READY) { sd_end_command(); return 0; }
    if (!sd_read_block(csd, sizeof(csd))) { sd_end_command(); return 0; }
    sd_end_command();

    uint8_t csd_ver = (csd[0] >> 6) & 0x03;
    if (csd_ver == 1) {
        // CSD v2.0 (SDHC/SDXC): capacidad = (C_SIZE+1) x 512 KiB
        // -> sectores de 512 B = (C_SIZE+1) x 1024
        uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) |
                          ((uint32_t)csd[8] << 8) | csd[9];
        return (c_size + 1u) * 1024u;
    } else {
        // CSD v1.0: size = (C_SIZE+1) * 2^(READ_BL_LEN+2 + C_SIZE_MULT+2) bytes
        uint32_t read_bl_len = csd[5] & 0x0F;
        uint32_t c_size = ((uint32_t)(csd[6] & 0x03) << 10) |
                          ((uint32_t)csd[7] << 2) | (csd[8] >> 6);
        uint32_t c_size_mult = ((csd[9] & 0x03) << 1) | (csd[10] >> 7);
        uint32_t mult = 1u << (c_size_mult + 2);
        uint32_t blocknr = (c_size + 1u) * mult;
        uint32_t blocklen = 1u << read_bl_len;
        return (blocknr * blocklen) / SD_SECTOR_SIZE;
    }
}

bool sd_read_block(uint32_t lba, uint8_t *buf) {
    if (!card_ok) return false;

    // SDHC/SDXC: lba directo. SDSC: direccion en bytes
    uint32_t addr = card_sdhc ? lba : lba * SD_SECTOR_SIZE;

    sd_cs_select();
    uint8_t r = sd_command(CMD17, addr);
    if (r != R1_READY) { sd_end_command(); return false; }
    if (!sd_read_block(buf, SD_SECTOR_SIZE)) { sd_end_command(); return false; }
    sd_end_command();
    return true;
}

bool sd_write_block(uint32_t lba, const uint8_t *buf) {
    if (!card_ok) return false;

    uint32_t addr = card_sdhc ? lba : lba * SD_SECTOR_SIZE;

    sd_cs_select();
    uint8_t r = sd_command(CMD24, addr);
    if (r != R1_READY) { sd_end_command(); return false; }
    if (!sd_write_block(buf, SD_SECTOR_SIZE)) { sd_end_command(); return false; }
    sd_end_command();
    return true;
}

void sd_spi_deinit(void) {
    card_ok = false;
    s_type = SD_TYPE_UNKNOWN;
}

void sd_test_read_block0(uint8_t *buf, char *output, size_t out_len) {
    int pos = 0;
    if (!card_ok) {
        snprintf(output + pos, out_len - pos, "card not ok");
        return;
    }

    pos += snprintf(output + pos, out_len - pos, "card_sdhc=%d ", card_sdhc);

    uint8_t csd[16];
    sd_cs_select();
    uint8_t r = sd_command(CMD9, 0);
    pos += snprintf(output + pos, out_len - pos, "CMD9=0x%02x ", r);
    if (r == R1_READY) {
        // Wait for data token (0xFE) with timeout
        uint64_t deadline = time_us_64() + (uint64_t)SD_TIMEOUT_MS * 1000;
        uint8_t tok = 0xFF;
        do {
            tok = spi_xfer(0xFF);
            if (tok == 0xFE) break;
            if (tok == 0x00) { pos += snprintf(output + pos, out_len - pos, "tok=0x00 "); break; }
            if (time_us_64() > deadline) { pos += snprintf(output + pos, out_len - pos, "timeout "); break; }
        } while (true);
        pos += snprintf(output + pos, out_len - pos, "tok=0x%02x ", tok);
        if (tok == 0xFE) {
            for (int i = 0; i < 16; i++) csd[i] = spi_xfer(0xFF);
            pos += snprintf(output + pos, out_len - pos,
                "CSD[0]=0x%02x CSD[5]=0x%02x CSD[6]=0x%02x CSD[7]=0x%02x CSD[8]=0x%02x CSD[9]=0x%02x ",
                csd[0], csd[5], csd[6], csd[7], csd[8], csd[9]);
            for (int i = 0; i < 2; i++) (void)spi_xfer(0xFF);
        }
    }
    sd_end_command();

    /* Now try read block 0 */
    pos += snprintf(output + pos, out_len - pos, "| ");
    sd_cs_select();
    uint32_t addr = card_sdhc ? 0 : 0;  // block 0
    r = sd_command(CMD17, addr);
    pos += snprintf(output + pos, out_len - pos, "CMD17=0x%02x ", r);
    if (r == R1_READY) {
        uint8_t local_buf[512];
        if (sd_read_block(local_buf, SD_SECTOR_SIZE)) {
            pos += snprintf(output + pos, out_len - pos, "OK");
            // Copy first 64 bytes for display
            for (int i = 0; i < 64 && i < 512; i++) buf[i] = local_buf[i];
        } else {
            pos += snprintf(output + pos, out_len - pos, "FAIL");
        }
    } else {
        pos += snprintf(output + pos, out_len - pos, "FAIL");
    }
    sd_end_command();
}