/*
 * chmod - change file attributes on the SD card (FatFs).
 *
 * Only FAT attributes are supported (RDO, HID, SYS, ARC). Syntax:
 *   chmod +r file   set read-only
 *   chmod -r file   clear read-only
 *   chmod +h file   set hidden
 *   chmod -h file   clear hidden
 */

#include <string.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

int saramos_chmod(int argc, char *argv[])
{
    if (argc < 3) {
        hal_uart_puts("chmod: missing operand\r\n");
        return 1;
    }

    const char *mode = argv[1];
    BYTE set = 0, clr = 0;

    if (strcmp(mode, "+r") == 0)
        set = AM_RDO;
    else if (strcmp(mode, "-r") == 0)
        clr = AM_RDO;
    else if (strcmp(mode, "+h") == 0)
        set = AM_HID;
    else if (strcmp(mode, "-h") == 0)
        clr = AM_HID;
    else if (strcmp(mode, "+s") == 0)
        set = AM_SYS;
    else if (strcmp(mode, "-s") == 0)
        clr = AM_SYS;
    else if (strcmp(mode, "+a") == 0)
        set = AM_ARC;
    else if (strcmp(mode, "-a") == 0)
        clr = AM_ARC;
    else {
        hal_uart_puts("chmod: use +r/-r, +h/-h, +s/-s, +a/-a\r\n");
        return 1;
    }

    for (int i = 2; i < argc; i++) {
        if (f_chmod(argv[i], set, set | clr) != FR_OK) {
            hal_uart_puts("chmod: failed '\"");
            hal_uart_puts(argv[i]);
            hal_uart_puts("\"\r\n");
        }
    }

    return 0;
}
