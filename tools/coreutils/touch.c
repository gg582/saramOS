/*
 * touch - create empty files or update timestamps on the SD card (FatFs).
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

int saramos_touch(int argc, char *argv[])
{
    FIL fil;
    FRESULT res;

    if (argc < 2) {
        hal_uart_puts("touch: missing operand\r\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        FILINFO info;
        if (f_stat(argv[i], &info) == FR_OK) {
            /* File exists; update timestamp if RTC is available. */
#if FF_FS_NORTC == 0
            FILINFO newinfo;
            newinfo.fdate = (WORD)((2022 - 1980) << 9 | 1 << 5 | 1);
            newinfo.ftime = 0;
            f_utime(argv[i], &newinfo);
#endif
            continue;
        }

        res = f_open(&fil, argv[i], FA_WRITE | FA_CREATE_ALWAYS);
        if (res != FR_OK) {
            hal_uart_puts("touch: cannot create '\"");
            hal_uart_puts(argv[i]);
            hal_uart_puts("\"\r\n");
            return 1;
        }
        f_close(&fil);
    }

    return 0;
}
