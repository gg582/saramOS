/*
 * uniq - report or filter out repeated lines.
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

#define UNIQ_MAX_LINES 128
#define UNIQ_LINE_SIZE 128

static int read_lines(const char *path, char lines[][UNIQ_LINE_SIZE], int max)
{
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK)
        return -1;

    int count = 0;
    size_t li = 0;
    UINT br;

    while (count < max && f_read(&fil, &lines[count][li], 1, &br) == FR_OK && br == 1) {
        if (lines[count][li] == '\n') {
            lines[count][li] = '\0';
            count++;
            li = 0;
        } else if (li < UNIQ_LINE_SIZE - 2) {
            li++;
        }
    }

    if (count < max && li > 0) {
        lines[count][li] = '\0';
        count++;
    }

    f_close(&fil);
    return count;
}

int saramos_uniq(int argc, char *argv[])
{
    int count_repeats = 0;
    int file_idx = 1;

    if (argc > 1 && strcmp(argv[1], "-c") == 0) {
        count_repeats = 1;
        file_idx = 2;
    }

    if (file_idx >= argc) {
        hal_uart_puts("uniq: missing file\r\n");
        return 1;
    }

    char lines[UNIQ_MAX_LINES][UNIQ_LINE_SIZE];
    int count = read_lines(argv[file_idx], lines, UNIQ_MAX_LINES);
    if (count < 0) {
        hal_uart_puts("uniq: cannot open '\"");
        hal_uart_puts(argv[file_idx]);
        hal_uart_puts("\"\r\n");
        return 1;
    }

    int i = 0;
    while (i < count) {
        int run = 1;
        while (i + run < count && strcmp(lines[i], lines[i + run]) == 0)
            run++;

        if (count_repeats) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%4d ", run);
            hal_uart_puts(buf);
        }
        hal_uart_puts(lines[i]);
        hal_uart_puts("\r\n");
        i += run;
    }

    return 0;
}
