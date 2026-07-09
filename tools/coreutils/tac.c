/*
 * tac - concatenate and print files in reverse line order.
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);
extern void hal_uart_putc(char c);

#define TAC_MAX_LINES 512
#define TAC_LINE_BUF  4096

static char tac_buf[TAC_LINE_BUF];

static void tac_file(const char *path)
{
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK) {
        hal_uart_puts("tac: cannot open '\"");
        hal_uart_puts(path);
        hal_uart_puts("\"\r\n");
        return;
    }

    /* Offsets of each line start within tac_buf. */
    static unsigned short line_off[TAC_MAX_LINES + 1];
    int nlines = 0;
    unsigned int pos = 0;
    char c;
    UINT br;
    int line_start = 0;

    while (pos < sizeof(tac_buf) - 1 &&
           f_read(&fil, &c, 1, &br) == FR_OK && br == 1) {
        tac_buf[pos++] = c;
        if (c == '\n') {
            if (nlines < TAC_MAX_LINES) {
                line_off[nlines++] = (unsigned short)line_start;
            }
            line_start = (int)pos;
        }
    }
    /* Last line with no trailing newline. */
    if (pos > (unsigned int)line_start && nlines < TAC_MAX_LINES) {
        tac_buf[pos++] = '\0';
        line_off[nlines++] = (unsigned short)line_start;
    }
    line_off[nlines] = (unsigned short)pos;
    f_close(&fil);

    for (int i = nlines - 1; i >= 0; i--) {
        int start = line_off[i];
        int end   = line_off[i + 1];
        for (int j = start; j < end; j++)
            hal_uart_putc(tac_buf[j]);
        /* Ensure a newline terminates the last original line if missing. */
        if (end > start && tac_buf[end - 1] != '\n')
            hal_uart_puts("\r\n");
    }
}

int saramos_tac(int argc, char *argv[])
{
    if (argc < 2) {
        hal_uart_puts("tac: missing file\r\n");
        return 1;
    }
    for (int i = 1; i < argc; i++)
        tac_file(argv[i]);
    return 0;
}
