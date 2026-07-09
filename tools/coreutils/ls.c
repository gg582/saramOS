/*
 * ls - list directory contents.
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

static void ls_simple(const char *path, int show_all)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;

    res = f_opendir(&dir, path);
    if (res != FR_OK) {
        hal_uart_puts("ls: cannot open directory\r\n");
        return;
    }

    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break;
        if (!show_all && fno.fname[0] == '.')
            continue;
        hal_uart_puts(fno.fname);
        hal_uart_puts((fno.fattrib & AM_DIR) ? "/\r\n" : "\r\n");
    }

    f_closedir(&dir);
}

static void ls_long(const char *path, int show_all)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;

    res = f_opendir(&dir, path);
    if (res != FR_OK) {
        hal_uart_puts("ls: cannot open directory\r\n");
        return;
    }

    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break;
        if (!show_all && fno.fname[0] == '.')
            continue;

        char buf[320];
        char attr[8];
        attr[0] = (fno.fattrib & AM_DIR) ? 'd' : '-';
        attr[1] = (fno.fattrib & AM_RDO) ? 'r' : 'w';
        attr[2] = (fno.fattrib & AM_HID) ? 'h' : '-';
        attr[3] = (fno.fattrib & AM_SYS) ? 's' : '-';
        attr[4] = (fno.fattrib & AM_ARC) ? 'a' : '-';
        attr[5] = '\0';

        snprintf(buf, sizeof(buf), "%s %10lu %s\r\n",
                 attr, (unsigned long)fno.fsize, fno.fname);
        hal_uart_puts(buf);
    }

    f_closedir(&dir);
}

int saramos_ls(int argc, char *argv[])
{
    int long_fmt = 0, show_all = 0;
    const char *path = ".";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0)
            long_fmt = 1;
        else if (strcmp(argv[i], "-a") == 0)
            show_all = 1;
        else if (argv[i][0] != '-')
            path = argv[i];
    }

    if (long_fmt)
        ls_long(path, show_all);
    else
        ls_simple(path, show_all);

    return 0;
}
