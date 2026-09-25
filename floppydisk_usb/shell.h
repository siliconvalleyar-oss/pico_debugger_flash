/* shell.h - USB/UART command shell for floppydisk debug */
#ifndef SHELL_H
#define SHELL_H

#ifdef __cplusplus
extern "C" {
#endif

void shell_init(void);
void shell_task(void);
void shell_register_cmd(const char *name, void (*fn)(int argc, char **argv), const char *help);
void shell_print(const char *fmt, ...);
void shell_error(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* SHELL_H */