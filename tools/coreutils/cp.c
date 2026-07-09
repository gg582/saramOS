/*
 * cp - copy files on the SD card (FatFs).
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

int saramos_cp(int argc, char *argv[])
{
    FIL src, dst;
    FRESULT res;
    UINT br, bw;
    char buf[256];

    if (argc < 3) {
        hal_uart_puts("cp: missing operand\r\n");
        return 1;
    }

    const char *srcpath = argv[argc - 2];
    const char *dstpath = argv[argc - 1];

    res = f_open(&src, srcpath, FA_READ);
    if (res != FR_OK) {
        hal_uart_puts("cp: cannot open source\r\n");
        return 1;
    }

    res = f_open(&dst, dstpath, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        f_close(&src);
        hal_uart_puts("cp: cannot create destination\r\n");
        return 1;
    }

    while ((res = f_read(&src, buf, sizeof(buf), &br)) == FR_OK && br > 0) {
        const char *p = buf;
        while (br > 0) {
            res = f_write(&dst, p, br, &bw);
            if (res != FR_OK || bw == 0)
                break;
            p += bw;
            br -= bw;
        }
        if (res != FR_OK)
            break;
    }

    f_close(&dst);
    f_close(&src);

    if (res != FR_OK) {
        hal_uart_puts("cp: copy failed\r\n");
        return 1;
    }

    return 0;
}
