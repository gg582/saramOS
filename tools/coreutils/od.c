/*
 * od - octal/hex dump of files.
 *
 * Supports: -x (hex words), -c (char), -b (octal bytes), default octal.
 */

#include <string.h>
#include <stdio.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

#define OD_ROW 16

int saramos_od(int argc, char *argv[])
{
    int fmt_hex  = 0;  /* -x  hex 16-bit words */
    int fmt_char = 0;  /* -c  named chars      */
    int fmt_oct  = 1;  /* default octal bytes  */
    int file_start = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-x") == 0) {
            fmt_hex = 1; fmt_oct = 0; file_start = i + 1;
        } else if (strcmp(argv[i], "-c") == 0) {
            fmt_char = 1; fmt_oct = 0; file_start = i + 1;
        } else if (strcmp(argv[i], "-b") == 0) {
            fmt_oct = 1; file_start = i + 1;
        } else if (argv[i][0] != '-') {
            file_start = i;
            break;
        }
    }
    (void)fmt_char; /* suppress unused-variable warning */

    if (file_start >= argc) {
        hal_uart_puts("od: missing file\r\n");
        return 1;
    }

    for (int fi = file_start; fi < argc; fi++) {
        FIL fil;
        if (f_open(&fil, argv[fi], FA_READ) != FR_OK) {
            hal_uart_puts("od: cannot open '\"");
            hal_uart_puts(argv[fi]);
            hal_uart_puts("\"\r\n");
            continue;
        }

        unsigned char row[OD_ROW];
        unsigned long offset = 0;
        UINT br;
        char buf[64];

        while (f_read(&fil, row, OD_ROW, &br) == FR_OK && br > 0) {
            /* Address. */
            snprintf(buf, sizeof(buf), "%07lo ", offset);
            hal_uart_puts(buf);
            offset += br;

            if (fmt_hex) {
                /* 16-bit little-endian words. */
                for (UINT i = 0; i < br; i += 2) {
                    unsigned int w = row[i];
                    if (i + 1 < br)
                        w |= (unsigned int)row[i + 1] << 8;
                    snprintf(buf, sizeof(buf), " %04x", w);
                    hal_uart_puts(buf);
                }
            } else {
                /* Octal bytes (default). */
                for (UINT i = 0; i < br; i++) {
                    snprintf(buf, sizeof(buf), " %03o", row[i]);
                    hal_uart_puts(buf);
                }
            }
            hal_uart_puts("\r\n");
        }

        /* Final address. */
        snprintf(buf, sizeof(buf), "%07lo\r\n", offset);
        hal_uart_puts(buf);

        f_close(&fil);
    }

    return 0;
}
