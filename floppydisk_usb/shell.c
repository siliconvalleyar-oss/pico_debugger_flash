/* shell.c - USB/UART command shell implementation */
#include "shell.h"
#include "app.h"
#include "fat.h"
#include "sd_spi.h"

#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "config.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string.h>

#define SHELL_MAX_ARGS 8
#define SHELL_MAX_LINE 128
#define SHELL_MAX_CMDS 20

typedef struct {
    const char *name;
    void (*fn)(int argc, char **argv);
    const char *help;
} shell_cmd_t;

static shell_cmd_t s_cmds[SHELL_MAX_CMDS];
static int s_cmd_count = 0;

static char s_line[SHELL_MAX_LINE];
static int s_line_len = 0;
static bool s_echo = true;

static void shell_putc(char c) {
    putchar_raw(c);
}

static void shell_puts(const char *s) {
    while (*s) shell_putc(*s++);
}

static void shell_prompt(void) {
    shell_puts("\r\nfloppydisk> ");
}

void shell_print(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    shell_puts(buf);
}

void shell_error(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    shell_puts("ERROR: ");
    shell_puts(buf);
}

static void shell_help(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_print("Comandos disponibles:\r\n");
    for (int i = 0; i < s_cmd_count; i++) {
        shell_print("  %-12s - %s\r\n", s_cmds[i].name, s_cmds[i].help ? s_cmds[i].help : "");
    }
}

static void shell_echo_cmd(int argc, char **argv) {
    if (argc < 2) {
        shell_print("echo: %s\r\n", s_echo ? "on" : "off");
        return;
    }
    if (strcmp(argv[1], "on") == 0) s_echo = true;
    else if (strcmp(argv[1], "off") == 0) s_echo = false;
    else shell_print("Uso: echo on|off\r\n");
}

static void shell_reboot_cmd(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_print("Reiniciando...\r\n");
    busy_wait_ms(100);
    watchdog_reboot(0, 0, 0);
}

static void shell_version_cmd(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_print("floppydisk_usb %s (build %lu)\r\n", app_get_version_str(), (unsigned long)app_get_build_num());
    shell_print("Commit: %s\r\n", FLOPPYDISK_COMMIT);
}

static void shell_fat_cmd(int argc, char **argv) {
    (void)argc; (void)argv;
    if (!app_fat_ok()) {
        shell_print("FAT: SD no montada\r\n");
        return;
    }
    shell_print("FAT: OK  Libre=%lu KB  Total=%lu KB\r\n", (unsigned long)app_fat_free_kb(), (unsigned long)app_fat_total_kb());
}

static void shell_ls_cmd(int argc, char **argv) {
    (void)argc; (void)argv;
    if (!app_fat_ok()) {
        shell_print("FAT: SD no montada\r\n");
        return;
    }
    fat_scan_entry_t list[32];
    int n = app_fat_list_images(list, 32);
    shell_print("Imagenes en /IMG (%d):\r\n", n);
    for (int i = 0; i < n; i++) {
        shell_print("  %s.%s  %lu bytes\r\n", list[i].name8, list[i].ext4, (unsigned long)list[i].size);
    }
}

static void shell_dump_cmd(int argc, char **argv) {
    if (argc < 3) {
        shell_print("Uso: dump <base8> <ext> [bloque] [cuantos]\r\n");
        shell_print("Ej:  dump DISK0001 IMA 0 10\r\n");
        return;
    }
    const char *base = argv[1];
    const char *ext = argv[2];
    uint32_t start_block = 0;
    uint32_t count = 16;
    if (argc > 3) start_block = strtoul(argv[3], NULL, 0);
    if (argc > 4) count = strtoul(argv[4], NULL, 0);
    
    uint32_t img_size = app_fat_image_size(base, ext);
    if (img_size == 0) {
        shell_print("Imagen no encontrada: %s.%s\r\n", base, ext);
        return;
    }
    shell_print("Dumping %s.%s  size=%lu bytes  blocks=%lu\r\n", base, ext, (unsigned long)img_size, (unsigned long)(img_size / 512));
    
    uint8_t buf[512];
    for (uint32_t i = 0; i < count; i++) {
        uint32_t blk = start_block + i;
        if (blk * 512 >= img_size) break;
        if (!app_fat_read_image_block(base, ext, blk, buf)) {
            shell_print("  Error leyendo bloque %lu\r\n", (unsigned long)blk);
            break;
        }
        shell_print("  Block %04lu: ", (unsigned long)blk);
        for (int j = 0; j < 32; j++) {
            shell_print("%02x ", buf[j]);
        }
        shell_print("...\r\n");
    }
}

static void shell_hex_cmd(int argc, char **argv) {
    if (argc < 3) {
        shell_print("Uso: hex <base8> <ext> <offset> [len]\r\n");
        shell_print("Ej:  hex DISK0001 IMA 0 256\r\n");
        return;
    }
    const char *base = argv[1];
    const char *ext = argv[2];
    uint32_t offset = strtoul(argv[3], NULL, 0);
    uint32_t len = 256;
    if (argc > 4) len = strtoul(argv[4], NULL, 0);
    
    uint32_t img_size = app_fat_image_size(base, ext);
    if (img_size == 0) {
        shell_print("Imagen no encontrada: %s.%s\r\n", base, ext);
        return;
    }
    if (offset >= img_size) {
        shell_print("Offset fuera de rango\r\n");
        return;
    }
    if (offset + len > img_size) len = img_size - offset;
    
    shell_print("Hex dump %s.%s  offset=%lu  len=%lu\r\n", base, ext, (unsigned long)offset, (unsigned long)len);
    
    uint8_t buf[512];
    uint32_t start_block = offset / 512;
    uint32_t end_block = (offset + len + 511) / 512;
    uint32_t pos = 0;
    
    for (uint32_t blk = start_block; blk < end_block; blk++) {
        if (!app_fat_read_image_block(base, ext, blk, buf)) break;
        uint32_t start = (blk == start_block) ? (offset % 512) : 0;
        uint32_t end = (blk == end_block - 1) ? ((offset + len) % 512) : 512;
        if (end == 0) end = 512;
        
        for (uint32_t i = start; i < end && pos < len; i++, pos++) {
            if (pos % 16 == 0) shell_print("\r\n%04lx: ", (unsigned long)(offset + pos));
            shell_print("%02x ", buf[i]);
        }
    }
    shell_print("\r\n");
}

static void shell_sdtest_cmd(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_print("SD SPI Test...\r\n");
    shell_print("  Pins: SCK=GP10 MOSI=GP11 MISO=GP12 CS=GP13\r\n");
    
    bool ok = sd_init();
    if (!ok) {
        shell_print("  sd_init() FALLO\r\n");
        return;
    }
    shell_print("  sd_init() OK\r\n");
    
    uint32_t blocks = sd_card_capacity();
    if (blocks > 0) {
        shell_print("  Capacidad: %lu bloques = %lu MB\r\n", (unsigned long)blocks, (unsigned long)(blocks / 2048));
    } else {
        shell_print("  Capacidad: desconocida (fallo CSD)\r\n");
    }
    
    uint8_t buf[512];
    char debug[256];
    sd_test_read_block0(buf, debug, sizeof(debug));
    shell_print("  %s\r\n", debug);
    
    if (strstr(debug, "OK")) {
        shell_print("  Bloque 0 (MBR) leido OK:\r\n");
        for (int i = 0; i < 64; i++) {
            if (i % 16 == 0) shell_print("\r\n  %04x: ", i);
            shell_print("%02x ", buf[i]);
        }
        shell_print("\r\n");
    }
}

void shell_register_cmd(const char *name, void (*fn)(int argc, char **argv), const char *help) {
    if (s_cmd_count >= SHELL_MAX_CMDS) return;
    s_cmds[s_cmd_count].name = name;
    s_cmds[s_cmd_count].fn = fn;
    s_cmds[s_cmd_count].help = help;
    s_cmd_count++;
}

static void shell_sdinit_cmd(int argc, char **argv) {
    (void)argc; (void)argv;
    shell_print("SD SPI Init Debug...\r\n");
    shell_print("  Pins: SCK=GP10 MOSI=GP11 MISO=GP12 CS=GP13\r\n");
    
    shell_print("  spi_init(SD_SPI, 400000)...\r\n");
    spi_init(SD_SPI, 400000);
    gpio_set_function(10, GPIO_FUNC_SPI);
    gpio_set_function(11, GPIO_FUNC_SPI);
    gpio_set_function(12, GPIO_FUNC_SPI);
    gpio_init(13);
    gpio_set_dir(13, GPIO_OUT);
    gpio_put(13, 1);
    shell_print("  GPIO configured\r\n");
    
    shell_print("  Dummy clocks (80)...\r\n");
    uint8_t dummy = 0xFF;
    for (int i = 0; i < 10; i++) {
        spi_write_read_blocking(SD_SPI, &dummy, &dummy, 1);
    }
    gpio_put(13, 0);
    spi_write_read_blocking(SD_SPI, &dummy, &dummy, 1);
    gpio_put(13, 1);
    shell_print("  CS toggled\r\n");
    
    shell_print("  CMD0 (GO_IDLE_STATE)...\r\n");
    uint8_t cmd0_buf[6] = {0x40, 0,0,0,0, 0x95};
    for (int attempt = 0; attempt < 10; attempt++) {
        gpio_put(13, 0);
        spi_write_blocking(SD_SPI, cmd0_buf, 6);
        for (int i = 0; i < 8; i++) {
            uint8_t r;
            spi_write_read_blocking(SD_SPI, &dummy, &r, 1);
            if ((r & 0x80) == 0) {
                shell_print("  CMD0 OK on attempt %d, R1=0x%02x\r\n", attempt+1, r);
                gpio_put(13, 1);
                goto cmd0_done;
            }
        }
        gpio_put(13, 1);
        busy_wait_ms(10);
    }
    shell_print("  CMD0 FAILED\r\n");
    return;
cmd0_done:
    
    shell_print("  CMD8 (SEND_IF_COND)...\r\n");
    uint8_t cmd8_buf[6] = {0x48, 0,0,0x01,0xAA, 0x87};
    gpio_put(13, 0);
    spi_write_blocking(SD_SPI, cmd8_buf, 6);
    for (int i = 0; i < 8; i++) {
        uint8_t r;
        spi_write_read_blocking(SD_SPI, &dummy, &r, 1);
        if ((r & 0x80) == 0) {
            uint8_t r7[4];
            for (int j = 0; j < 4; j++) spi_write_read_blocking(SD_SPI, &dummy, &r7[j], 1);
            shell_print("  CMD8 OK, R1=0x%02x, R7=0x%02x%02x%02x%02x\r\n", r, r7[0], r7[1], r7[2], r7[3]);
            gpio_put(13, 1);
            goto cmd8_done;
        }
    }
    gpio_put(13, 1);
    shell_print("  CMD8 FAILED (not v2 SD)\r\n");
cmd8_done:
    
    shell_print("  ACMD41 (APP_SEND_OP_COND) with HCS=1...\r\n");
    for (int attempt = 0; attempt < 100; attempt++) {
        gpio_put(13, 0);
        uint8_t cmd55_buf[6] = {0x77, 0,0,0,0, 0xFF};
        spi_write_blocking(SD_SPI, cmd55_buf, 6);
        for (int i = 0; i < 8; i++) {
            spi_write_read_blocking(SD_SPI, &dummy, &dummy, 1);
        }
        uint8_t acmd41_buf[6] = {0x69, 0x40,0,0,0, 0xFF};
        spi_write_blocking(SD_SPI, acmd41_buf, 6);
        uint8_t r1;
        for (int i = 0; i < 8; i++) {
            spi_write_read_blocking(SD_SPI, &dummy, &r1, 1);
            if ((r1 & 0x80) == 0) break;
        }
        gpio_put(13, 1);
        if ((r1 & 0x80) == 0) {
            shell_print("  ACMD41 OK on attempt %d, R1=0x%02x\r\n", attempt+1, r1);
            break;
        }
        busy_wait_ms(10);
    }
    shell_print("  Init sequence done\r\n");
}

void shell_init(void) {
    (void)argc; (void)argv;
    shell_print("SD SPI Init Debug...\r\n");
    shell_print("  Pins: SCK=GP10 MOSI=GP11 MISO=GP12 CS=GP13\r\n");
    
    shell_print("  spi_init(SD_SPI, 400000)...\r\n");
    spi_init(SD_SPI, 400000);
    gpio_set_function(10, GPIO_FUNC_SPI);
    gpio_set_function(11, GPIO_FUNC_SPI);
    gpio_set_function(12, GPIO_FUNC_SPI);
    gpio_init(13);
    gpio_set_dir(13, GPIO_OUT);
    gpio_put(13, 1);
    shell_print("  GPIO configured\r\n");
    
    shell_print("  Dummy clocks (80)...\r\n");
    uint8_t dummy = 0xFF;
    for (int i = 0; i < 10; i++) {
        spi_write_read_blocking(SD_SPI, &dummy, &dummy, 1);
    }
    gpio_put(13, 0);
    spi_write_read_blocking(SD_SPI, &dummy, &dummy, 1);
    gpio_put(13, 1);
    shell_print("  CS toggled\r\n");
    
    shell_print("  CMD0 (GO_IDLE_STATE)...\r\n");
    uint8_t cmd0_buf[6] = {0x40, 0,0,0,0, 0x95};
    for (int attempt = 0; attempt < 10; attempt++) {
        gpio_put(13, 0);
        spi_write_blocking(SD_SPI, cmd0_buf, 6);
        for (int i = 0; i < 8; i++) {
            uint8_t r;
            spi_write_read_blocking(SD_SPI, &dummy, &r, 1);
            if ((r & 0x80) == 0) {
                shell_print("  CMD0 OK on attempt %d, R1=0x%02x\r\n", attempt+1, r);
                gpio_put(13, 1);
                goto cmd0_done;
            }
        }
        gpio_put(13, 1);
        busy_wait_ms(10);
    }
    shell_print("  CMD0 FAILED\r\n");
    return;
cmd0_done:
    
    shell_print("  CMD8 (SEND_IF_COND)...\r\n");
    uint8_t cmd8_buf[6] = {0x48, 0,0,0x01,0xAA, 0x87};
    gpio_put(13, 0);
    spi_write_blocking(SD_SPI, cmd8_buf, 6);
    for (int i = 0; i < 8; i++) {
        uint8_t r;
        spi_write_read_blocking(SD_SPI, &dummy, &r, 1);
        if ((r & 0x80) == 0) {
            uint8_t r7[4];
            for (int j = 0; j < 4; j++) spi_write_read_blocking(SD_SPI, &dummy, &r7[j], 1);
            shell_print("  CMD8 OK, R1=0x%02x, R7=0x%02x%02x%02x%02x\r\n", r, r7[0], r7[1], r7[2], r7[3]);
            gpio_put(13, 1);
            goto cmd8_done;
        }
    }
    gpio_put(13, 1);
    shell_print("  CMD8 FAILED (not v2 SD)\r\n");
cmd8_done:
    
    shell_print("  ACMD41 (APP_SEND_OP_COND) with HCS=1...\r\n");
    for (int attempt = 0; attempt < 100; attempt++) {
        gpio_put(13, 0);
        uint8_t cmd55_buf[6] = {0x77, 0,0,0,0, 0xFF};
        spi_write_blocking(SD_SPI, cmd55_buf, 6);
        for (int i = 0; i < 8; i++) {
            spi_write_read_blocking(SD_SPI, &dummy, &dummy, 1);
        }
        uint8_t acmd41_buf[6] = {0x69, 0x40,0,0,0, 0xFF};
        spi_write_blocking(SD_SPI, acmd41_buf, 6);
        uint8_t r1;
        for (int i = 0; i < 8; i++) {
            spi_write_read_blocking(SD_SPI, &dummy, &r1, 1);
            if ((r1 & 0x80) == 0) break;
        }
        gpio_put(13, 1);
        if ((r1 & 0x80) == 0) {
            shell_print("  ACMD41 OK on attempt %d, R1=0x%02x\r\n", attempt+1, r1);
            break;
        }
        busy_wait_ms(10);
    }
    shell_print("  Init sequence done\r\n");
}

static int shell_tokenize(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max_args) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    return argc;
}

void shell_task(void) {
    int c = getchar_timeout_us(0);
    if (c == PICO_ERROR_TIMEOUT) return;

    if (c == '\r' || c == '\n') {
        if (s_line_len > 0) {
            s_line[s_line_len] = '\0';
            if (s_echo) shell_putc('\r'), shell_putc('\n');

            char *argv[SHELL_MAX_ARGS];
            int argc = shell_tokenize(s_line, argv, SHELL_MAX_ARGS);

            bool found = false;
            for (int i = 0; i < s_cmd_count; i++) {
                if (strcmp(argv[0], s_cmds[i].name) == 0) {
                    s_cmds[i].fn(argc, argv);
                    found = true;
                    break;
                }
            }
            if (!found && argc > 0) {
                shell_print("Comando desconocido: '%s'\r\n", argv[0]);
            }
            s_line_len = 0;
        }
        shell_prompt();
    } else if (c == 127 || c == 8) { /* backspace */
        if (s_line_len > 0) {
            s_line_len--;
            if (s_echo) shell_puts("\b \b");
        }
    } else if (c >= 32 && c < 127 && s_line_len < SHELL_MAX_LINE - 1) {
        s_line[s_line_len++] = (char)c;
        if (s_echo) shell_putc(c);
    }
}