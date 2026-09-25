/* shell.c - USB/UART command shell implementation */
#include "shell.h"
#include "app.h"

#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
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

void shell_register_cmd(const char *name, void (*fn)(int argc, char **argv), const char *help) {
    if (s_cmd_count >= SHELL_MAX_CMDS) return;
    s_cmds[s_cmd_count].name = name;
    s_cmds[s_cmd_count].fn = fn;
    s_cmds[s_cmd_count].help = help;
    s_cmd_count++;
}

void shell_init(void) {
    stdio_init_all();
    shell_register_cmd("help", shell_help, "Mostrar esta ayuda");
    shell_register_cmd("echo", shell_echo_cmd, "Activar/desactivar echo (on|off)");
    shell_register_cmd("reboot", shell_reboot_cmd, "Reiniciar la Pico");
    shell_register_cmd("version", shell_version_cmd, "Mostrar version y build");
    shell_prompt();
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