/*
 * seq - print a sequence of numbers.
 */

#include "coreutils.h"

extern void hal_uart_puts(const char *s);

static long parse_long(const char *s)
{
    int sign = 1;
    long n = 0;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (*s >= '0' && *s <= '9')
        n = n * 10 + (*s++ - '0');
    return n * sign;
}

int saramos_seq(int argc, char *argv[])
{
    long first = 1, last = 1, step = 1;

    if (argc == 2) {
        last = parse_long(argv[1]);
    } else if (argc == 3) {
        first = parse_long(argv[1]);
        last = parse_long(argv[2]);
    } else if (argc >= 4) {
        first = parse_long(argv[1]);
        step = parse_long(argv[2]);
        last = parse_long(argv[3]);
    } else {
        hal_uart_puts("seq: usage: seq [first] [step] last\r\n");
        return 1;
    }

    if (step == 0)
        step = 1;

    char buf[32];
    if (step > 0) {
        for (long i = first; i <= last; i += step) {
            snprintf(buf, sizeof(buf), "%ld\r\n", i);
            hal_uart_puts(buf);
        }
    } else {
        for (long i = first; i >= last; i += step) {
            snprintf(buf, sizeof(buf), "%ld\r\n", i);
            hal_uart_puts(buf);
        }
    }

    return 0;
}
