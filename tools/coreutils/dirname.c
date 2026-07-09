/*
 * dirname - strip last component from file name.
 */

#include <string.h>
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

int saramos_dirname(int argc, char *argv[])
{
    if (argc < 2) {
        hal_uart_puts("dirname: missing operand\r\n");
        return 1;
    }

    const char *path = argv[1];
    size_t len = strlen(path);

    while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\'))
        len--;

    while (len > 0 && path[len - 1] != '/' && path[len - 1] != '\\')
        len--;

    if (len == 0) {
        hal_uart_puts(".\r\n");
    } else {
        char out[256];
        if (len >= sizeof(out))
            len = sizeof(out) - 1;
        strncpy(out, path, len);
        out[len] = '\0';
        hal_uart_puts(out);
        hal_uart_puts("\r\n");
    }

    return 0;
}
