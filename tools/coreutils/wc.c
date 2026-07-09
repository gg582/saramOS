/*
 * wc - count lines, words, and bytes in files.
 */

#include <string.h>
#include <ctype.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

static void count_file(const char *path, unsigned long *out_lines,
                       unsigned long *out_words, unsigned long *out_bytes)
{
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK) {
        hal_uart_puts("wc: cannot open '\"");
        hal_uart_puts(path);
        hal_uart_puts("\"\r\n");
        return;
    }

    unsigned long lines = 0, words = 0, bytes = 0;
    char buf[64];
    UINT br;
    int in_word = 0;

    while ((res = f_read(&fil, buf, sizeof(buf), &br)) == FR_OK && br > 0) {
        for (UINT i = 0; i < br; i++) {
            unsigned char c = (unsigned char)buf[i];
            bytes++;
            if (c == '\n')
                lines++;
            if (isspace(c)) {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                words++;
            }
        }
    }

    f_close(&fil);

    char out[64];
    snprintf(out, sizeof(out), "%8lu %7lu %7lu %s\r\n",
             lines, words, bytes, path);
    hal_uart_puts(out);

    if (out_lines)
        *out_lines += lines;
    if (out_words)
        *out_words += words;
    if (out_bytes)
        *out_bytes += bytes;
}

int saramos_wc(int argc, char *argv[])
{
    int show_lines = 1, show_words = 1, show_bytes = 1;
    int file_start = 1;

    for (int i = 1; i < argc && argv[i][0] == '-'; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            show_words = show_bytes = 0;
            file_start = i + 1;
        } else if (strcmp(argv[i], "-w") == 0) {
            show_lines = show_bytes = 0;
            file_start = i + 1;
        } else if (strcmp(argv[i], "-c") == 0) {
            show_lines = show_words = 0;
            file_start = i + 1;
        }
    }

    if (file_start >= argc) {
        hal_uart_puts("wc: missing file\r\n");
        return 1;
    }

    unsigned long total_lines = 0, total_words = 0, total_bytes = 0;

    for (int i = file_start; i < argc; i++) {
        unsigned long l = 0, w = 0, b = 0;
        count_file(argv[i], &l, &w, &b);
        total_lines += l;
        total_words += w;
        total_bytes += b;
    }

    if (argc - file_start > 1) {
        char out[64];
        if (show_lines && show_words && show_bytes)
            snprintf(out, sizeof(out), "%8lu %7lu %7lu total\r\n",
                     total_lines, total_words, total_bytes);
        else if (show_lines)
            snprintf(out, sizeof(out), "%8lu total\r\n", total_lines);
        else if (show_words)
            snprintf(out, sizeof(out), "%7lu total\r\n", total_words);
        else
            snprintf(out, sizeof(out), "%7lu total\r\n", total_bytes);
        hal_uart_puts(out);
    }

    return 0;
}
