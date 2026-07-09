/*
 * tee - copy arguments to both the console and a file on the SD card.
 *
 * Usage: tee <file> <text>...
 */

#include <string.h>
#include "ff.h"
#include "fsutils.h"

extern void hal_uart_puts(const char *s);

int saramos_tee(int argc, char *argv[])
{
    if (argc < 3) {
        hal_uart_puts("tee: usage: tee <file> <text>...\r\n");
        return 1;
    }

    FIL fil;
    FRESULT res = f_open(&fil, argv[1], FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        hal_uart_puts("tee: cannot open '\"");
        hal_uart_puts(argv[1]);
        hal_uart_puts("\"\r\n");
        return 1;
    }

    for (int i = 2; i < argc; i++) {
        hal_uart_puts(argv[i]);
        UINT bw;
        f_write(&fil, argv[i], strlen(argv[i]), &bw);
        if (i + 1 < argc) {
            hal_uart_puts(" ");
            f_write(&fil, " ", 1, &bw);
        }
    }

    hal_uart_puts("\r\n");
    UINT bw;
    f_write(&fil, "\r\n", 2, &bw);

    f_close(&fil);
    return 0;
}
