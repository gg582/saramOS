/*
 * Built-in commands for the enhanced saramOS shell.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "shell.h"
#include "ff.h"

extern void hal_uart_puts(const char *s);
extern void hal_uart_putc(char c);
extern int hal_uart_try_getc(void);

static int shell_exit_requested = 0;

int shell_should_exit(void)
{
    return shell_exit_requested;
}

void shell_request_exit(void)
{
    shell_exit_requested = 1;
}

void shell_clear_exit(void)
{
    shell_exit_requested = 0;
}

static int parse_int(const char *s)
{
    int sign = 1;
    int n = 0;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (*s >= '0' && *s <= '9')
        n = n * 10 + (*s++ - '0');
    return n * sign;
}

int shell_builtin_cd(int argc, char *argv[])
{
    const char *path = (argc > 1) ? argv[1] : "/";
    if (f_chdir(path) != FR_OK) {
        hal_uart_puts("cd: failed\r\n");
        return 1;
    }
    return 0;
}

int shell_builtin_pwd(int argc, char *argv[])
{
    char buf[256];
    if (f_getcwd(buf, sizeof(buf)) == FR_OK) {
        hal_uart_puts(buf);
        hal_uart_puts("\r\n");
    } else {
        hal_uart_puts("pwd: failed\r\n");
        return 1;
    }
    return 0;
}

int shell_builtin_export(int argc, char *argv[])
{
    if (argc < 2) {
        shell_env_print();
        return 0;
    }

    char *eq = strchr(argv[1], '=');
    if (eq) {
        *eq = '\0';
        if (!shell_env_set(argv[1], eq + 1)) {
            hal_uart_puts("export: failed\r\n");
            return 1;
        }
    } else {
        char value[SHELL_ENV_SIZE];
        if (shell_env_get(argv[1], value, sizeof(value)))
            shell_env_set(argv[1], value);
        else
            shell_env_set(argv[1], "");
    }
    return 0;
}

int shell_builtin_unset(int argc, char *argv[])
{
    if (argc < 2) {
        hal_uart_puts("unset: missing operand\r\n");
        return 1;
    }
    shell_env_unset(argv[1]);
    return 0;
}

int shell_builtin_env(int argc, char *argv[])
{
    shell_env_print();
    return 0;
}

int shell_builtin_exit(int argc, char *argv[])
{
    shell_request_exit();
    return 0;
}

int shell_builtin_clear(int argc, char *argv[])
{
    hal_uart_puts("\x1B[2J\x1B[H");
    return 0;
}

static int test_op(const char *a, const char *op, const char *b)
{
    if (strcmp(op, "=") == 0)
        return strcmp(a, b) == 0;
    if (strcmp(op, "!=") == 0)
        return strcmp(a, b) != 0;
    if (strcmp(op, "-eq") == 0)
        return parse_int(a) == parse_int(b);
    if (strcmp(op, "-ne") == 0)
        return parse_int(a) != parse_int(b);
    if (strcmp(op, "-lt") == 0)
        return parse_int(a) < parse_int(b);
    if (strcmp(op, "-le") == 0)
        return parse_int(a) <= parse_int(b);
    if (strcmp(op, "-gt") == 0)
        return parse_int(a) > parse_int(b);
    if (strcmp(op, "-ge") == 0)
        return parse_int(a) >= parse_int(b);
    if (strcmp(op, "-z") == 0)
        return strlen(a) == 0;
    if (strcmp(op, "-n") == 0)
        return strlen(a) != 0;
    if (strcmp(op, "-e") == 0) {
        FILINFO info;
        return f_stat(a, &info) == FR_OK;
    }
    if (strcmp(op, "-d") == 0) {
        FILINFO info;
        return f_stat(a, &info) == FR_OK && (info.fattrib & AM_DIR);
    }
    if (strcmp(op, "-f") == 0) {
        FILINFO info;
        return f_stat(a, &info) == FR_OK && !(info.fattrib & AM_DIR);
    }
    return 0;
}

int shell_builtin_test(int argc, char *argv[])
{
    if (argc < 2)
        return 1;

    if (strcmp(argv[0], "[") == 0) {
        if (argc < 3 || strcmp(argv[argc - 1], "]") != 0)
            return 1;
        argc--;
    }

    if (argc == 2)
        return strlen(argv[1]) == 0 ? 1 : 0;

    if (argc == 3)
        return test_op(argv[1], argv[2], "") ? 0 : 1;

    if (argc == 4)
        return test_op(argv[1], argv[2], argv[3]) ? 0 : 1;

    return 1;
}

int shell_builtin_echo(int argc, char *argv[])
{
    int newline = 1;
    int i = 1;
    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        newline = 0;
        i = 2;
    }

    for (; i < argc; i++) {
        hal_uart_puts(argv[i]);
        if (i + 1 < argc)
            hal_uart_puts(" ");
    }

    if (newline)
        hal_uart_puts("\r\n");
    return 0;
}

int shell_builtin_source(int argc, char *argv[])
{
    if (argc < 2) {
        hal_uart_puts("source: missing file\r\n");
        return 1;
    }
    return shell_run_file(argv[1]);
}
