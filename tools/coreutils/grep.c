/*
 * grep - simple line filter.
 */

#include <string.h>
#include <ctype.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

static int match(const char *line, const char *pat, int ignore_case)
{
    size_t plen = strlen(pat);
    if (plen == 0)
        return 1;
    for (const char *p = line; *p; p++) {
        if (ignore_case) {
            size_t i;
            for (i = 0; i < plen; i++) {
                if (tolower((unsigned char)p[i]) != tolower((unsigned char)pat[i]))
                    break;
            }
            if (i == plen)
                return 1;
        } else {
            if (strncmp(p, pat, plen) == 0)
                return 1;
        }
    }
    return 0;
}

static void grep_file(const char *path, const char *pat, int ignore_case, int show_line)
{
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK) {
        hal_uart_puts("grep: cannot open '\"");
        hal_uart_puts(path);
        hal_uart_puts("\"\r\n");
        return;
    }

    char line[256];
    unsigned int line_no = 0;
    UINT br;
    size_t li = 0;

    while (f_read(&fil, &line[li], 1, &br) == FR_OK && br == 1) {
        if (line[li] == '\n' || li >= sizeof(line) - 2) {
            line[li] = '\0';
            line_no++;
            if (match(line, pat, ignore_case)) {
                if (show_line) {
                    char prefix[32];
                    snprintf(prefix, sizeof(prefix), "%u:", line_no);
                    hal_uart_puts(prefix);
                }
                hal_uart_puts(line);
                hal_uart_puts("\r\n");
            }
            li = 0;
        } else {
            li++;
        }
    }

    if (li > 0) {
        line[li] = '\0';
        line_no++;
        if (match(line, pat, ignore_case)) {
            if (show_line) {
                char prefix[32];
                snprintf(prefix, sizeof(prefix), "%u:", line_no);
                hal_uart_puts(prefix);
            }
            hal_uart_puts(line);
            hal_uart_puts("\r\n");
        }
    }

    f_close(&fil);
}

int saramos_grep(int argc, char *argv[])
{
    int ignore_case = 0, show_line = 0;
    int pat_idx = 1;

    while (pat_idx < argc && argv[pat_idx][0] == '-') {
        if (strcmp(argv[pat_idx], "-i") == 0)
            ignore_case = 1;
        else if (strcmp(argv[pat_idx], "-n") == 0)
            show_line = 1;
        pat_idx++;
    }

    if (pat_idx >= argc) {
        hal_uart_puts("grep: missing pattern\r\n");
        return 1;
    }

    const char *pat = argv[pat_idx];
    int file_start = pat_idx + 1;

    if (file_start >= argc) {
        hal_uart_puts("grep: missing file\r\n");
        return 1;
    }

    for (int i = file_start; i < argc; i++)
        grep_file(argv[i], pat, ignore_case, show_line);

    return 0;
}
