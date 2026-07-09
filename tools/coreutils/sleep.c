/*
 * sleep - delay for a specified amount of time.
 */

#include <ttak/timing/timing.h>
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

static unsigned int parse_uint(const char *s)
{
    unsigned int n = 0;
    while (*s >= '0' && *s <= '9')
        n = n * 10 + (unsigned int)(*s++ - '0');
    return n;
}

int saramos_sleep(int argc, char *argv[])
{
    if (argc < 2) {
        hal_uart_puts("sleep: missing operand\r\n");
        return 1;
    }

    unsigned int sec = parse_uint(argv[1]);
    unsigned long start = (unsigned long)ttak_get_tick_count();
    unsigned long target = start + sec * 1000;

    while ((unsigned long)ttak_get_tick_count() < target)
        ;

    return 0;
}
