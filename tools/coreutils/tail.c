/*
 * tail - print the last lines of files.
 */

#include <string.h>
#include <stdlib.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);
extern void hal_uart_putc(char c);

static unsigned int parse_uint(const char *s)
{
    unsigned int n = 0;
    while (*s >= '0' && *s <= '9')
        n = n * 10 + (unsigned int)(*s++ - '0');
    return n;
}

static unsigned int count_lines(const char *path)
{
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK)
        return 0;

    unsigned int count = 0;
    char c;
    UINT br;
    while (f_read(&fil, &c, 1, &br) == FR_OK && br == 1) {
        if (c == '\n')
            count++;
    }
    f_close(&fil);
    return count;
}

static void tail_file(const char *path, unsigned int n)
{
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK) {
        hal_uart_puts("tail: cannot open '\"");
        hal_uart_puts(path);
        hal_uart_puts("\"\r\n");
        return;
    }

    unsigned int total = count_lines(path);
    unsigned int skip = (total > n) ? (total - n) : 0;
    unsigned int current = 0;

    char c;
    UINT br;
    while (f_read(&fil, &c, 1, &br) == FR_OK && br == 1) {
        if (current >= skip)
            hal_uart_putc(c);
        if (c == '\n')
            current++;
    }

    f_close(&fil);
}

int saramos_tail(int argc, char *argv[])
{
    unsigned int n = 10;
    int file_start = 1;

    if (argc > 2 && strcmp(argv[1], "-n") == 0) {
        n = parse_uint(argv[2]);
        file_start = 3;
    } else if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'n') {
        n = parse_uint(argv[1] + 2);
        file_start = 2;
    }

    if ((unsigned int)argc <= file_start) {
        hal_uart_puts("tail: missing file\r\n");
        return 1;
    }

    for (int i = file_start; i < argc; i++)
        tail_file(argv[i], n);

    return 0;
}
