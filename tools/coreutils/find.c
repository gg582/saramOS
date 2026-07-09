/*
 * find - simple recursive directory search.
 *
 * Usage: find [path] [-name pattern]
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

static int match_pattern(const char *name, const char *pat)
{
    if (pat == NULL || *pat == '\0')
        return 1;

    while (*pat) {
        if (*pat == '*') {
            pat++;
            if (!*pat)
                return 1;
            while (*name) {
                if (match_pattern(name, pat))
                    return 1;
                name++;
            }
            return 0;
        } else if (*pat == '?') {
            if (!*name)
                return 0;
            pat++;
            name++;
        } else {
            if (*name != *pat)
                return 0;
            pat++;
            name++;
        }
    }
    return *name == '\0';
}

static void list_entry(const char *path, const char *name)
{
    hal_uart_puts(path);
    size_t len = strlen(path);
    if (len == 0 || path[len - 1] != '/')
        hal_uart_puts("/");
    hal_uart_puts(name);
    hal_uart_puts("\r\n");
}

static void find_recursive(const char *base, const char *pat)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;

    res = f_opendir(&dir, base);
    if (res != FR_OK)
        return;

    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break;
        if (strcmp(fno.fname, ".") == 0 || strcmp(fno.fname, "..") == 0)
            continue;

        if (match_pattern(fno.fname, pat))
            list_entry(base, fno.fname);

        if (fno.fattrib & AM_DIR) {
            char sub[256];
            snprintf(sub, sizeof(sub), "%s%s%s", base,
                     (base[strlen(base) - 1] == '/') ? "" : "/",
                     fno.fname);
            find_recursive(sub, pat);
        }
    }

    f_closedir(&dir);
}

int saramos_find(int argc, char *argv[])
{
    const char *path = "/";
    const char *pat = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-name") == 0 && i + 1 < argc) {
            pat = argv[i + 1];
            i++;
        } else if (argv[i][0] != '-') {
            path = argv[i];
        }
    }

    find_recursive(path, pat);
    return 0;
}
