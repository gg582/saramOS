/*
 * basename - strip directory and suffix from filenames.
 */

#include <string.h>
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

int saramos_basename(int argc, char *argv[])
{
    if (argc < 2) {
        hal_uart_puts("basename: missing operand\r\n");
        return 1;
    }

    const char *path = argv[1];
    const char *suffix = (argc > 2) ? argv[2] : NULL;

    const char *base = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }

    char out[256];
    strncpy(out, base, sizeof(out) - 1);
    out[sizeof(out) - 1] = '\0';

    if (suffix) {
        size_t slen = strlen(suffix);
        size_t olen = strlen(out);
        if (slen > 0 && olen > slen && strcmp(out + olen - slen, suffix) == 0)
            out[olen - slen] = '\0';
    }

    hal_uart_puts(out);
    hal_uart_puts("\r\n");
    return 0;
}
