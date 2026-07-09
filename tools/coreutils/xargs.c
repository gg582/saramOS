/*
 * xargs - build and execute commands from standard input (via file).
 *
 * In saramOS the shell has no stdin pipe to xargs, so this implementation
 * reads argument words from a file and appends them to a base command,
 * executing the command once per batch.
 *
 * Usage: xargs [-n MAX] COMMAND [INITIAL_ARGS...] < file
 *
 * Because the shell does not yet support redirection into built-ins, this
 * variant accepts an explicit input file via -I flag:
 *   xargs [-n MAX] [-f FILE] COMMAND [INITIAL_ARG...]
 *
 * Without -f it prints usage guidance.
 */

#include <string.h>
#include <stdlib.h>
#include "ff.h"
#include "coreutils.h"

extern void hal_uart_puts(const char *s);

/* Forward declaration: the shell exposes this for recursive execution. */
extern int shell_execute(int argc, char *argv[]);

#define XARGS_MAX_ARGS  32
#define XARGS_ARG_LEN  128

int saramos_xargs(int argc, char *argv[])
{
    int max_per_run = 1;
    const char *infile = NULL;
    int cmd_start = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_per_run = (int)atoi(argv[++i]);
            cmd_start = i + 1;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            infile = argv[++i];
            cmd_start = i + 1;
        } else if (argv[i][0] != '-') {
            cmd_start = i;
            break;
        }
    }

    if (infile == NULL || cmd_start >= argc) {
        hal_uart_puts("usage: xargs [-n MAX] -f FILE COMMAND [ARGS...]\r\n");
        return 1;
    }

    if (max_per_run < 1)
        max_per_run = 1;

    FIL fil;
    if (f_open(&fil, infile, FA_READ) != FR_OK) {
        hal_uart_puts("xargs: cannot open '\"");
        hal_uart_puts(infile);
        hal_uart_puts("\"\r\n");
        return 1;
    }

    /* Static storage for the composed argv. */
    static char argbuf[XARGS_MAX_ARGS][XARGS_ARG_LEN];
    static char *eargv[XARGS_MAX_ARGS];

    /* Copy base command + initial args into argbuf. */
    int base_argc = argc - cmd_start;
    if (base_argc > XARGS_MAX_ARGS - 1)
        base_argc = XARGS_MAX_ARGS - 1;
    for (int i = 0; i < base_argc; i++) {
        strncpy(argbuf[i], argv[cmd_start + i], XARGS_ARG_LEN - 1);
        argbuf[i][XARGS_ARG_LEN - 1] = '\0';
        eargv[i] = argbuf[i];
    }

    /* Read words from the file. */
    char word[XARGS_ARG_LEN];
    int wi = 0;
    int batch = 0;
    int total_argc = base_argc;
    char c;
    UINT br;

    while (f_read(&fil, &c, 1, &br) == FR_OK && br == 1) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (wi > 0) {
                word[wi] = '\0';
                if (total_argc < XARGS_MAX_ARGS) {
                    strncpy(argbuf[total_argc], word, XARGS_ARG_LEN - 1);
                    argbuf[total_argc][XARGS_ARG_LEN - 1] = '\0';
                    eargv[total_argc] = argbuf[total_argc];
                    total_argc++;
                    batch++;
                }
                wi = 0;
                if (batch >= max_per_run) {
                    shell_execute(total_argc, eargv);
                    total_argc = base_argc;
                    batch = 0;
                }
            }
        } else {
            if (wi < XARGS_ARG_LEN - 1)
                word[wi++] = c;
        }
    }
    /* Flush last partial word. */
    if (wi > 0) {
        word[wi] = '\0';
        if (total_argc < XARGS_MAX_ARGS) {
            strncpy(argbuf[total_argc], word, XARGS_ARG_LEN - 1);
            argbuf[total_argc][XARGS_ARG_LEN - 1] = '\0';
            eargv[total_argc] = argbuf[total_argc];
            total_argc++;
            batch++;
        }
    }
    if (batch > 0)
        shell_execute(total_argc, eargv);

    f_close(&fil);
    return 0;
}
