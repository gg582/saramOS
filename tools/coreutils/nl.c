/*
 * nl - number lines of files.
 */

#include <string.h>
#include <stdio.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);
extern void hal_uart_putc(char c);

static void nl_file(const char *path, int body_only)
{
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK) {
        hal_uart_puts("nl: cannot open '\"");
        hal_uart_puts(path);
        hal_uart_puts("\"\r\n");
        return;
    }

    char linebuf[256];
    unsigned int li = 0;
    long lineno = 1;
    char c;
    UINT br;
    char numbuf[24];

    while (f_read(&fil, &c, 1, &br) == FR_OK && br == 1) {
        if (c == '\n') {
            linebuf[li] = '\0';
            /* Skip blank lines when body_only mode (-b t). */
            int blank = (li == 0);
            if (!body_only || !blank) {
                snprintf(numbuf, sizeof(numbuf), "%6ld\t", lineno++);
                hal_uart_puts(numbuf);
            } else {
                hal_uart_puts("      \t");
            }
            hal_uart_puts(linebuf);
            hal_uart_puts("\r\n");
            li = 0;
        } else {
            if (li < sizeof(linebuf) - 1)
                linebuf[li++] = c;
        }
    }
    /* Last line with no trailing newline. */
    if (li > 0) {
        linebuf[li] = '\0';
        snprintf(numbuf, sizeof(numbuf), "%6ld\t", lineno);
        hal_uart_puts(numbuf);
        hal_uart_puts(linebuf);
        hal_uart_puts("\r\n");
    }

    f_close(&fil);
}

int saramos_nl(int argc, char *argv[])
{
    int body_only = 0;  /* -b t: number only non-empty lines */
    int file_start = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "t") == 0)
                body_only = 1;
            i++;
            file_start = i + 1;
        } else if (strcmp(argv[i], "-bt") == 0) {
            body_only = 1;
            file_start = i + 1;
        } else if (argv[i][0] != '-') {
            file_start = i;
            break;
        }
    }

    if (file_start >= argc) {
        hal_uart_puts("nl: missing file\r\n");
        return 1;
    }

    for (int i = file_start; i < argc; i++)
        nl_file(argv[i], body_only);

    return 0;
}
