/*
 * mv - move/rename files or directories on the SD card (FatFs).
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

int saramos_mv(int argc, char *argv[])
{
    FRESULT res;

    if (argc < 3) {
        hal_uart_puts("mv: missing operand\r\n");
        return 1;
    }

    const char *src = argv[argc - 2];
    const char *dst = argv[argc - 1];

    res = f_rename(src, dst);
    if (res != FR_OK) {
        hal_uart_puts("mv: failed\r\n");
        return 1;
    }

    return 0;
}
