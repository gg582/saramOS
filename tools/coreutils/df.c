/*
 * df - report file system disk space usage.
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

int saramos_df(int argc, char *argv[])
{
    const char *path = (argc > 1) ? argv[1] : "/";
    FATFS *fs;
    DWORD free_clusters;

    FRESULT res = f_getfree(path, &free_clusters, &fs);
    if (res != FR_OK) {
        hal_uart_puts("df: cannot get free space\r\n");
        return 1;
    }

    DWORD total_sectors = (fs->n_fatent - 2) * fs->csize;
    DWORD free_sectors = free_clusters * fs->csize;
    DWORD sector_size = 512;

    DWORD total_kb = total_sectors * sector_size / 1024;
    DWORD free_kb = free_sectors * sector_size / 1024;
    DWORD used_kb = total_kb - free_kb;

    char buf[128];
    hal_uart_puts("Filesystem     1K-blocks     Used Available Use% Mounted on\r\n");
    snprintf(buf, sizeof(buf), "%-14s %8lu %8lu %8lu %3lu%% %s\r\n",
             path, total_kb, used_kb, free_kb,
             total_kb ? (used_kb * 100 / total_kb) : 0, path);
    hal_uart_puts(buf);

    return 0;
}
