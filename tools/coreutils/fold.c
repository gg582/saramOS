/*
 * fold - wrap long input lines to a specified width.
 */

#include <string.h>
#include <stdlib.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);
extern void hal_uart_putc(char c);

static void fold_file(const char *path, int width, int at_space)
{
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK) {
        hal_uart_puts("fold: cannot open '\"");
        hal_uart_puts(path);
        hal_uart_puts("\"\r\n");
        return;
    }

    char linebuf[512];
    int col = 0;
    char c;
    UINT br;

    while (f_read(&fil, &c, 1, &br) == FR_OK && br == 1) {
        if (c == '\n') {
            hal_uart_putc('\r');
            hal_uart_putc('\n');
            col = 0;
            continue;
        }
        if (c == '\r')
            continue;

        if (at_space && col >= width) {
            /* Scan backwards in linebuf to find last space. */
            int found = 0;
            for (int i = col - 1; i >= 0; i--) {
                if (linebuf[i] == ' ') {
                    /* Output up to and including the space. */
                    for (int j = 0; j <= i; j++)
                        hal_uart_putc(linebuf[j]);
                    hal_uart_puts("\r\n");
                    /* Shift remaining. */
                    int rem = col - i - 1;
                    for (int j = 0; j < rem; j++)
                        linebuf[j] = linebuf[i + 1 + j];
                    col = rem;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                /* No space found; hard break. */
                for (int j = 0; j < col; j++)
                    hal_uart_putc(linebuf[j]);
                hal_uart_puts("\r\n");
                col = 0;
            }
        }

        if (!at_space && col >= width) {
            hal_uart_puts("\r\n");
            col = 0;
        }

        if (col < (int)sizeof(linebuf) - 1)
            linebuf[col] = c;
        hal_uart_putc(c);
        col++;
    }

    if (col > 0)
        hal_uart_puts("\r\n");

    f_close(&fil);
}

int saramos_fold(int argc, char *argv[])
{
    int width = 80;
    int at_space = 0;
    int file_start = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            at_space = 1;
            file_start = i + 1;
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            width = (int)atoi(argv[++i]);
            file_start = i + 1;
        } else if (argv[i][0] == '-' && argv[i][1] == 'w') {
            width = (int)atoi(argv[i] + 2);
            file_start = i + 1;
        } else if (argv[i][0] != '-') {
            file_start = i;
            break;
        }
    }

    if (width < 1)
        width = 80;

    if (file_start >= argc) {
        hal_uart_puts("fold: missing file\r\n");
        return 1;
    }

    for (int i = file_start; i < argc; i++)
        fold_file(argv[i], width, at_space);

    return 0;
}
