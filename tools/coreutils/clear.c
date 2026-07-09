/*
 * clear - clear the terminal screen.
 */

#include "coreutils.h"

extern void hal_uart_puts(const char *s);

int saramos_clear(int argc, char *argv[])
{
    hal_uart_puts("\x1B[2J\x1B[H");
    return 0;
}
