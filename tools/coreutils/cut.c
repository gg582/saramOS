/*
 * cut - extract selected portions of each line.
 *
 * Supports: cut -d'<delim>' -f<field> <file>
 * Field numbers are 1-based.
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

static unsigned int parse_uint(const char *s)
{
    unsigned int n = 0;
    while (*s >= '0' && *s <= '9')
        n = n * 10 + (unsigned int)(*s++ - '0');
    return n;
}

static void cut_file(const char *path, char delim, unsigned int field)
{
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK) {
        hal_uart_puts("cut: cannot open '\"");
        hal_uart_puts(path);
        hal_uart_puts("\"\r\n");
        return;
    }

    char line[256];
    size_t li = 0;
    UINT br;

    while (f_read(&fil, &line[li], 1, &br) == FR_OK && br == 1) {
        if (line[li] == '\n' || li >= sizeof(line) - 2) {
            line[li] = '\0';

            unsigned int f = 1;
            const char *p = line;
            while (*p && f < field) {
                if (*p == delim)
                    f++;
                p++;
            }

            while (*p && *p != '\n' && *p != delim) {
                hal_uart_putc(*p);
                p++;
            }
            hal_uart_puts("\r\n");

            li = 0;
        } else {
            li++;
        }
    }

    f_close(&fil);
}

int saramos_cut(int argc, char *argv[])
{
    char delim = '\t';
    unsigned int field = 1;
    int file_idx = 1;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-d", 2) == 0 && argv[i][2] != '\0') {
            delim = argv[i][2];
            file_idx = i + 1;
        } else if (strncmp(argv[i], "-f", 2) == 0 && argv[i][2] != '\0') {
            field = parse_uint(argv[i] + 2);
            file_idx = i + 1;
        }
    }

    if (file_idx >= argc || field == 0) {
        hal_uart_puts("cut: usage: cut -d<delim> -f<field> <file>\r\n");
        return 1;
    }

    cut_file(argv[file_idx], delim, field);
    return 0;
}
