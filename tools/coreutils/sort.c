/*
 * sort - sort lines of text files.
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

#define SORT_MAX_LINES 64
#define SORT_LINE_SIZE 128

static int read_lines(const char *path, char lines[][SORT_LINE_SIZE], int max)
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
        } else if (li < SORT_LINE_SIZE - 2) {
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

static void sort_lines(char lines[][SORT_LINE_SIZE], int count)
{
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (strcmp(lines[i], lines[j]) > 0) {
                char tmp[SORT_LINE_SIZE];
                strcpy(tmp, lines[i]);
                strcpy(lines[i], lines[j]);
                strcpy(lines[j], tmp);
            }
        }
    }
}

static void sort_file(const char *path)
{
    char lines[SORT_MAX_LINES][SORT_LINE_SIZE];
    int count = read_lines(path, lines, SORT_MAX_LINES);
    if (count < 0) {
        hal_uart_puts("sort: cannot open '\"");
        hal_uart_puts(path);
        hal_uart_puts("\"\r\n");
        return;
    }

    sort_lines(lines, count);
    for (int i = 0; i < count; i++) {
        hal_uart_puts(lines[i]);
        hal_uart_puts("\r\n");
    }
}

int saramos_sort(int argc, char *argv[])
{
    if (argc < 2) {
        hal_uart_puts("sort: missing file\r\n");
        return 1;
    }

    for (int i = 1; i < argc; i++)
        sort_file(argv[i]);

    return 0;
}
