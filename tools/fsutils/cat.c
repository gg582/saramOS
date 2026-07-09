/*
 * cat - concatenate and print files from the SD card (FatFs).
 */

#include <string.h>
#include "ff.h"
#include "fsutils.h"

extern void hal_uart_putc(char c);
extern void hal_uart_puts(const char *s);

int saramos_cat(int argc, char *argv[])
{
    if (argc < 2) {
        hal_uart_puts("cat: missing operand\r\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        FIL fil;
        FRESULT res = f_open(&fil, argv[i], FA_READ);
        if (res != FR_OK) {
            hal_uart_puts("cat: \"");
            hal_uart_puts(argv[i]);
            hal_uart_puts("\": No such file or directory\r\n");
            continue;
        }

        char buf[64];
        UINT br;
        while ((res = f_read(&fil, buf, sizeof(buf) - 1, &br)) == FR_OK && br > 0) {
            buf[br] = '\0';
            hal_uart_puts(buf);
        }

        f_close(&fil);
    }

    return 0;
}
