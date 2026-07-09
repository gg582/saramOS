/*
 * diff - compare files line by line.
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

#define DIFF_MAX_LINES 64
#define DIFF_LINE_SIZE 128

static int read_lines(const char *path, char lines[][DIFF_LINE_SIZE])
{
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK)
        return -1;

    int count = 0;
    size_t li = 0;
    UINT br;

    while (count < DIFF_MAX_LINES && f_read(&fil, &lines[count][li], 1, &br) == FR_OK && br == 1) {
        if (lines[count][li] == '\n') {
            lines[count][li] = '\0';
            count++;
            li = 0;
        } else if (li < DIFF_LINE_SIZE - 2) {
            li++;
        }
    }

    if (count < DIFF_MAX_LINES && li > 0) {
        lines[count][li] = '\0';
        count++;
    }

    f_close(&fil);
    return count;
}

int saramos_diff(int argc, char *argv[])
{
    if (argc < 3) {
        hal_uart_puts("diff: missing operand\r\n");
        return 1;
    }

    char a[DIFF_MAX_LINES][DIFF_LINE_SIZE];
    char b[DIFF_MAX_LINES][DIFF_LINE_SIZE];
    int na = read_lines(argv[1], a);
    int nb = read_lines(argv[2], b);

    if (na < 0 || nb < 0) {
        hal_uart_puts("diff: cannot open file\r\n");
        return 1;
    }

    int differ = 0;
    int n = (na > nb) ? na : nb;
    for (int i = 0; i < n; i++) {
        const char *la = (i < na) ? a[i] : "";
        const char *lb = (i < nb) ? b[i] : "";
        if (strcmp(la, lb) != 0) {
            differ = 1;
            char buf[16];
            snprintf(buf, sizeof(buf), "%dc%d\r\n", i + 1, i + 1);
            hal_uart_puts(buf);
            hal_uart_puts("< ");
            hal_uart_puts(la);
            hal_uart_puts("\r\n---\r\n> ");
            hal_uart_puts(lb);
            hal_uart_puts("\r\n");
        }
    }

    return differ ? 1 : 0;
}
