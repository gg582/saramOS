/*
 * Minimal terminal I/O stubs for the saramOS vi port.
 */

#include "saramos_port.h"

extern int proc_try_getc(void);
extern void hal_uart_putc(char c);

int saramos_getchar(void)
{
    int c;
    while ((c = proc_try_getc()) < 0)
        ;
    return c;
}

int saramos_putchar(int c)
{
    hal_uart_putc((char)c);
    return c;
}

int saramos_puts(const char *s)
{
    while (*s)
        hal_uart_putc(*s++);
    return 0;
}
