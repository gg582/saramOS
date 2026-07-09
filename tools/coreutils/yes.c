/*
 * yes - output a string repeatedly until stopped.
 *
 * Limited to a bounded number of lines for the bare-metal console.
 */

#include <string.h>
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

#define YES_MAX_LINES 1000

int saramos_yes(int argc, char *argv[])
{
    const char *msg = (argc > 1) ? argv[1] : "y";

    for (int i = 0; i < YES_MAX_LINES; i++) {
        hal_uart_puts(msg);
        hal_uart_puts("\r\n");
    }

    return 0;
}
