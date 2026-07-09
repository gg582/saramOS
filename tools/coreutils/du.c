/*
 * du - estimate file space usage.
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

static DWORD sector_size = 512;

static DWORD du_recursive(const char *path)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    DWORD total = 0;

    res = f_opendir(&dir, path);
    if (res != FR_OK)
        return 0;

    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break;
        if (strcmp(fno.fname, ".") == 0 || strcmp(fno.fname, "..") == 0)
            continue;

        DWORD clusters = (fno.fsize + (sector_size * 1) - 1) / (sector_size * 1);
        if (clusters == 0)
            clusters = 1;
        total += clusters * sector_size;

        if (fno.fattrib & AM_DIR) {
            char sub[256];
            snprintf(sub, sizeof(sub), "%s%s%s", path,
                     (path[strlen(path) - 1] == '/') ? "" : "/",
                     fno.fname);
            total += du_recursive(sub);
        }
    }

    f_closedir(&dir);
    return total;
}

int saramos_du(int argc, char *argv[])
{
    const char *path = (argc > 1) ? argv[1] : ".";
    DWORD total = du_recursive(path);

    char buf[64];
    snprintf(buf, sizeof(buf), "%lu\t%s\r\n", (unsigned long)(total / 1024), path);
    hal_uart_puts(buf);
    return 0;
}
