/*
 * strings - extract printable character sequences from a file.
 *
 * Usage: strings [-n MIN] file ...
 *   -n MIN   Minimum sequence length (default 4).
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);
extern void hal_uart_putc(char c);

#define STRINGS_MAXLEN 256

static void strings_file(const char *path, int min_len)
{
    FIL fil;
    if (f_open(&fil, path, FA_READ) != FR_OK) {
        hal_uart_puts("strings: cannot open '\"");
        hal_uart_puts(path);
        hal_uart_puts("\"\r\n");
        return;
    }

    char seq[STRINGS_MAXLEN + 1];
    int seqlen = 0;
    unsigned char c;
    UINT br;

    while (f_read(&fil, &c, 1, &br) == FR_OK && br == 1) {
        /* A "printable" byte: printable ASCII or tab. */
        if (isprint((int)c) || c == '\t') {
            if (seqlen < STRINGS_MAXLEN)
                seq[seqlen++] = (char)c;
        } else {
            if (seqlen >= min_len) {
                seq[seqlen] = '\0';
                hal_uart_puts(seq);
                hal_uart_puts("\r\n");
            }
            seqlen = 0;
        }
    }
    /* Flush any remaining sequence. */
    if (seqlen >= min_len) {
        seq[seqlen] = '\0';
        hal_uart_puts(seq);
        hal_uart_puts("\r\n");
    }

    f_close(&fil);
}

int saramos_strings(int argc, char *argv[])
{
    int min_len = 4;
    int file_start = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            min_len = (int)atoi(argv[++i]);
            file_start = i + 1;
        } else if (argv[i][0] == '-' && argv[i][1] == 'n') {
            min_len = (int)atoi(argv[i] + 2);
            file_start = i + 1;
        } else if (argv[i][0] != '-') {
            file_start = i;
            break;
        }
    }

    if (min_len < 1)
        min_len = 1;

    if (file_start >= argc) {
        hal_uart_puts("strings: missing file\r\n");
        return 1;
    }

    for (int i = file_start; i < argc; i++)
        strings_file(argv[i], min_len);

    return 0;
}
