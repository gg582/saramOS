/*
 * rev - reverse lines character-wise.
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);
extern void hal_uart_putc(char c);

static void rev_file(const char *path)
{
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK) {
        hal_uart_puts("rev: cannot open '\"");
        hal_uart_puts(path);
        hal_uart_puts("\"\r\n");
        return;
    }

    char line[256];
    size_t li = 0;
    UINT br;

    while (f_read(&fil, &line[li], 1, &br) == FR_OK && br == 1) {
        if (line[li] == '\n' || li >= sizeof(line) - 2) {
            for (int i = (int)li - 1; i >= 0; i--)
                hal_uart_putc(line[i]);
            hal_uart_puts("\r\n");
            li = 0;
        } else {
            li++;
        }
    }

    f_close(&fil);
}

int saramos_rev(int argc, char *argv[])
{
    if (argc < 2) {
        hal_uart_puts("rev: missing file\r\n");
        return 1;
    }

    for (int i = 1; i < argc; i++)
        rev_file(argv[i]);

    return 0;
}
