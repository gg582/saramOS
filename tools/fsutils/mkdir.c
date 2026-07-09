/*
 * mkdir - create directories on the SD card (FatFs).
 */

#include "ff.h"
#include "fsutils.h"

extern void hal_uart_puts(const char *s);

int saramos_mkdir(int argc, char *argv[])
{
    if (argc < 2) {
        hal_uart_puts("mkdir: missing operand\r\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        FRESULT res = f_mkdir(argv[i]);
        if (res != FR_OK) {
            hal_uart_puts("mkdir: cannot create directory '\"");
            hal_uart_puts(argv[i]);
            hal_uart_puts("\"\r\n");
        }
    }

    return 0;
}
