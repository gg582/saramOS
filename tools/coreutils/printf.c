/*
 * printf - formatted output (shell built-in style, no file I/O).
 *
 * Supports: %s %d %i %o %x %X %c %% and escape sequences \n \t \r \\.
 * Usage: printf FORMAT [ARG ...]
 */

#include "coreutils.h"

extern void hal_uart_puts(const char *s);
extern void hal_uart_putc(char c);

/* Local helper: avoids relying on atol/strtol availability in newlib nano. */
static long str_to_long(const char *s)
{
    long result = 0;
    int neg = 0;
    if (!s) return 0;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return neg ? -result : result;
}

int saramos_printf(int argc, char *argv[])
{
    if (argc < 2) {
        hal_uart_puts("printf: missing format\r\n");
        return 1;
    }

    const char *fmt = argv[1];
    int argi = 2;
    char numbuf[32];

    for (const char *p = fmt; *p; p++) {
        if (*p == '\\') {
            p++;
            switch (*p) {
            case 'n': hal_uart_puts("\r\n"); break;
            case 't': hal_uart_putc('\t');   break;
            case 'r': hal_uart_putc('\r');   break;
            case '\\': hal_uart_putc('\\'); break;
            case '0': hal_uart_putc('\0');  break;
            case 'a': hal_uart_putc('\a');  break;
            case 'b': hal_uart_putc('\b');  break;
            default:
                hal_uart_putc('\\');
                hal_uart_putc(*p);
                break;
            }
        } else if (*p == '%') {
            p++;
            const char *arg = (argi < argc) ? argv[argi++] : "";
            switch (*p) {
            case 's':
                hal_uart_puts(arg);
                break;
            case 'd':
            case 'i':
                snprintf(numbuf, sizeof(numbuf), "%ld", str_to_long(arg));
                hal_uart_puts(numbuf);
                break;
            case 'u':
                snprintf(numbuf, sizeof(numbuf), "%lu",
                         (unsigned long)str_to_long(arg));
                hal_uart_puts(numbuf);
                break;
            case 'o':
                snprintf(numbuf, sizeof(numbuf), "%lo",
                         (unsigned long)str_to_long(arg));
                hal_uart_puts(numbuf);
                break;
            case 'x':
                snprintf(numbuf, sizeof(numbuf), "%lx",
                         (unsigned long)str_to_long(arg));
                hal_uart_puts(numbuf);
                break;
            case 'X':
                snprintf(numbuf, sizeof(numbuf), "%lX",
                         (unsigned long)str_to_long(arg));
                hal_uart_puts(numbuf);
                break;
            case 'c':
                hal_uart_putc(arg[0]);
                break;
            case '%':
                hal_uart_putc('%');
                argi--; /* '%' does not consume an argument */
                break;
            default:
                hal_uart_putc('%');
                hal_uart_putc(*p);
                break;
            }
        } else {
            hal_uart_putc(*p);
        }
    }

    return 0;
}
