/*
 * which - locate a command.
 *
 * Searches a simple PATH of /bin:/usr/bin on the SD card.
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

static const char *path_dirs[] = { "/bin", "/usr/bin", NULL };

static int file_exists(const char *path)
{
    FILINFO info;
    return (f_stat(path, &info) == FR_OK && !(info.fattrib & AM_DIR));
}

int saramos_which(int argc, char *argv[])
{
    if (argc < 2) {
        hal_uart_puts("which: missing argument\r\n");
        return 1;
    }

    int found_any = 0;

    for (int i = 1; i < argc; i++) {
        int found = 0;
        for (int d = 0; path_dirs[d]; d++) {
            char full[128];
            snprintf(full, sizeof(full), "%s/%s", path_dirs[d], argv[i]);
            if (file_exists(full)) {
                hal_uart_puts(full);
                hal_uart_puts("\r\n");
                found = 1;
                found_any = 1;
                break;
            }
        }
        if (!found) {
            hal_uart_puts("which: no ");
            hal_uart_puts(argv[i]);
            hal_uart_puts(" in /bin:/usr/bin\r\n");
        }
    }

    return found_any ? 0 : 1;
}
