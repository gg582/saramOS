/*
 * echo - print arguments to the console.
 */

#include <string.h>
#include "fsutils.h"

extern void hal_uart_puts(const char *s);

int saramos_echo(int argc, char *argv[])
{
    int nflag = 0;
    int i = 1;

    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        nflag = 1;
        i = 2;
    }

    for (; i < argc; i++) {
        hal_uart_puts(argv[i]);
        if (i + 1 < argc)
            hal_uart_puts(" ");
    }

    if (!nflag)
        hal_uart_puts("\r\n");

    return 0;
}
