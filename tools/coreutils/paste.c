/*
 * paste - merge corresponding lines from multiple files.
 *
 * Usage: paste [-d DELIM] file1 file2 ...
 *   -d DELIM  Field delimiter (default: tab).
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);
extern void hal_uart_putc(char c);

#define PASTE_MAX_FILES 8
#define PASTE_LINE_SIZE 256

int saramos_paste(int argc, char *argv[])
{
    char delim = '\t';
    int file_start = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            delim = argv[++i][0];
            file_start = i + 1;
        } else if (argv[i][0] != '-') {
            file_start = i;
            break;
        }
    }

    int nfiles = argc - file_start;
    if (nfiles < 1) {
        hal_uart_puts("paste: missing file\r\n");
        return 1;
    }
    if (nfiles > PASTE_MAX_FILES)
        nfiles = PASTE_MAX_FILES;

    FIL fils[PASTE_MAX_FILES];
    int open[PASTE_MAX_FILES];

    for (int i = 0; i < nfiles; i++) {
        open[i] = (f_open(&fils[i], argv[file_start + i], FA_READ) == FR_OK);
        if (!open[i]) {
            hal_uart_puts("paste: cannot open '\"");
            hal_uart_puts(argv[file_start + i]);
            hal_uart_puts("\"\r\n");
        }
    }

    /* Read one line at a time from each file and merge. */
    for (;;) {
        char lines[PASTE_MAX_FILES][PASTE_LINE_SIZE];
        int any_data = 0;

        for (int fi = 0; fi < nfiles; fi++) {
            int li = 0;
            if (open[fi]) {
                char c;
                UINT br;
                while (li < PASTE_LINE_SIZE - 1 &&
                       f_read(&fils[fi], &c, 1, &br) == FR_OK && br == 1) {
                    if (c == '\n')
                        break;
                    if (c != '\r')
                        lines[fi][li++] = c;
                }
                if (li > 0 || br > 0)
                    any_data = 1;
                /* If EOF with no characters, mark as empty but still merge. */
                if (br == 0 && li == 0) {
                    /* keep open flag; output empty field */
                }
            }
            lines[fi][li] = '\0';
        }

        if (!any_data)
            break;

        for (int fi = 0; fi < nfiles; fi++) {
            if (fi > 0)
                hal_uart_putc(delim);
            hal_uart_puts(lines[fi]);
        }
        hal_uart_puts("\r\n");
    }

    for (int i = 0; i < nfiles; i++) {
        if (open[i])
            f_close(&fils[i]);
    }

    return 0;
}
