/*
 * Minimal Debian-like shell for saramOS.
 *
 * Entered via the `sd mountfs` command.  Built-in commands use FatFs
 * directly and reuse the fsutils (and vi, if enabled) entry points.
 */

#include <stdio.h>
#include <string.h>
#include "shell.h"
#include "ff.h"
#include "fsutils.h"
#ifdef ENABLE_TOOL_VI
#include "saramos_port.h"
#endif

extern void hal_uart_puts(const char *s);
extern int hal_uart_try_getc(void);
extern void hal_uart_putc(char c);

static void shell_read_line(char *buf, size_t size)
{
    size_t i = 0;
    int c;

    while (1) {
        c = hal_uart_try_getc();
        if (c < 0)
            continue;

        if (c == '\r' || c == '\n') {
            hal_uart_puts("\r\n");
            buf[i] = '\0';
            return;
        } else if (c == '\b' || c == 127) {
            if (i > 0) {
                i--;
                hal_uart_puts("\b \b");
            }
        } else if (c >= 32 && c < 127) {
            if (i + 1 < size) {
                buf[i++] = (char)c;
                hal_uart_putc((char)c);
            }
        }
    }
}

static void shell_parse(char *line, int *argc, char *argv[], int max_argc)
{
    *argc = 0;
    while (*line && *argc < max_argc) {
        while (*line == ' ' || *line == '\t')
            line++;
        if (!*line)
            break;

        argv[(*argc)++] = line;
        while (*line && *line != ' ' && *line != '\t')
            line++;
        if (*line) {
            *line = '\0';
            line++;
        }
    }
}

static void shell_ls(const char *path)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    const char *p = (path && *path) ? path : ".";

    res = f_opendir(&dir, p);
    if (res != FR_OK) {
        hal_uart_puts("ls: cannot open directory\r\n");
        return;
    }

    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break;
        hal_uart_puts(fno.fname);
        hal_uart_puts((fno.fattrib & AM_DIR) ? "/\r\n" : "\r\n");
    }

    f_closedir(&dir);
}

static void shell_cd(int argc, char *argv[])
{
    const char *path = (argc > 1) ? argv[1] : "/";
    if (f_chdir(path) != FR_OK)
        hal_uart_puts("cd: failed\r\n");
}

static void shell_pwd(void)
{
    char buf[256];
    if (f_getcwd(buf, sizeof(buf)) == FR_OK) {
        hal_uart_puts(buf);
        hal_uart_puts("\r\n");
    } else {
        hal_uart_puts("pwd: failed\r\n");
    }
}

static int shell_exec(int argc, char *argv[])
{
    if (strcmp(argv[0], "cd") == 0) {
        shell_cd(argc, argv);
        return 0;
    }
    if (strcmp(argv[0], "pwd") == 0) {
        shell_pwd();
        return 0;
    }
    if (strcmp(argv[0], "ls") == 0) {
        shell_ls(argc > 1 ? argv[1] : NULL);
        return 0;
    }
    if (strcmp(argv[0], "cat") == 0)
        return saramos_cat(argc, argv);
    if (strcmp(argv[0], "rm") == 0)
        return saramos_rm(argc, argv);
    if (strcmp(argv[0], "mkdir") == 0)
        return saramos_mkdir(argc, argv);
    if (strcmp(argv[0], "echo") == 0)
        return saramos_echo(argc, argv);
    if (strcmp(argv[0], "tee") == 0)
        return saramos_tee(argc, argv);
#ifdef ENABLE_TOOL_VI
    if (strcmp(argv[0], "vi") == 0)
        return saramos_vi(argc, argv);
#endif

    hal_uart_puts("sh: command not found: ");
    hal_uart_puts(argv[0]);
    hal_uart_puts("\r\n");
    return 1;
}

__attribute__((weak)) int shell_run(void)
{
    hal_uart_puts("mountfs: entering minimal shell (type 'exit' to leave)\r\n");

    char line[128];
    while (1) {
        hal_uart_puts("$ ");
        shell_read_line(line, sizeof(line));

        int argc;
        char *argv[16];
        shell_parse(line, &argc, argv, 16);
        if (argc == 0)
            continue;
        if (strcmp(argv[0], "exit") == 0)
            break;

        shell_exec(argc, argv);
    }

    hal_uart_puts("mountfs: leaving shell\r\n");
    return 0;
}
