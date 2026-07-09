/*
 * tr - translate or delete characters.
 *
 * Supports simple sets like tr 'a-z' 'A-Z' and tr -d 'x'.
 */

#include <string.h>
#include <ctype.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);
extern void hal_uart_putc(char c);

static int in_set(char c, const char *set)
{
    size_t len = strlen(set);
    for (size_t i = 0; i < len; i++) {
        if (set[i] == '-' && i > 0 && i + 1 < len) {
            if (c >= set[i - 1] && c <= set[i + 1])
                return 1;
        }
        if (set[i] == c)
            return 1;
    }
    return 0;
}

static char translate(char c, const char *from, const char *to)
{
    size_t flen = strlen(from);
    size_t tlen = strlen(to);
    for (size_t i = 0; i < flen && i < tlen; i++) {
        if (from[i] == c)
            return to[i];
        if (from[i] == '-' && i > 0 && i + 1 < flen) {
            char start = from[i - 1];
            char end = from[i + 1];
            if (c >= start && c <= end) {
                size_t off = (size_t)(unsigned char)(c - start);
                if (off < tlen)
                    return to[off];
            }
        }
    }
    return c;
}

static void tr_file(const char *path, const char *from, const char *to, int delete)
{
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK) {
        hal_uart_puts("tr: cannot open '\"");
        hal_uart_puts(path);
        hal_uart_puts("\"\r\n");
        return;
    }

    char c;
    UINT br;
    while (f_read(&fil, &c, 1, &br) == FR_OK && br == 1) {
        if (delete) {
            if (!in_set(c, from))
                hal_uart_putc(c);
        } else {
            hal_uart_putc(translate(c, from, to));
        }
    }

    f_close(&fil);
}

int saramos_tr(int argc, char *argv[])
{
    int delete = 0;
    int arg = 1;

    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        delete = 1;
        arg = 2;
    }

    if ((delete && argc < arg + 2) || (!delete && argc < arg + 3)) {
        hal_uart_puts("tr: usage: tr [-d] <set1> [set2] <file>\r\n");
        return 1;
    }

    const char *from = argv[arg];
    const char *to = delete ? "" : argv[arg + 1];
    const char *file = delete ? argv[arg + 1] : argv[arg + 2];

    tr_file(file, from, to, delete);
    return 0;
}
