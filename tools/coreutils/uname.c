/*
 * uname - print system information.
 */

#include <string.h>
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

int saramos_uname(int argc, char *argv[])
{
    int all = 0, kernel = 0, machine = 0;

    if (argc == 1)
        kernel = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0)
            all = 1;
        else if (strcmp(argv[i], "-s") == 0)
            kernel = 1;
        else if (strcmp(argv[i], "-m") == 0)
            machine = 1;
    }

    if (all) {
        hal_uart_puts("saramOS STM32F769I-DISCO armv7e-m\r\n");
    } else {
        if (kernel)
            hal_uart_puts("saramOS ");
        if (machine)
            hal_uart_puts("armv7e-m");
        hal_uart_puts("\r\n");
    }

    return 0;
}
