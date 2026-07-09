/*
 * Main interactive loop for the enhanced saramOS shell.
 */

#include <string.h>
#include <stdio.h>
#include "shell.h"

extern void hal_uart_puts(const char *s);
extern void hal_uart_putc(char c);
extern int hal_uart_try_getc(void);
extern int hal_uart_readable(void);

void shell_env_init(void);

int shell_interactive_process(saramos_process_t *p)
{
    static char line[SHELL_LINE_SIZE];
    static char *tokens[SHELL_MAX_TOKENS];
    static size_t i;
    static int count;
    int c;

    PROC_BEGIN(p);

    shell_env_init();
    shell_clear_exit();

    proc_puts("mountfs: entering enhanced shell (type 'exit' to leave)\r\n");

    while (!shell_should_exit()) {
        i = 0;
        proc_puts("$ ");
        while (1) {
            PROC_WAIT(p, hal_uart_readable());
            c = hal_uart_try_getc();
            if (c == '\r' || c == '\n') {
                proc_puts("\r\n");
                line[i] = '\0';
                break;
            } else if (c == '\b' || c == 127) {
                if (i > 0) {
                    i--;
                    proc_puts("\b \b");
                }
            } else if (c >= 32 && c < 127) {
                if (i + 1 < sizeof(line)) {
                    line[i++] = (char)c;
                    proc_putc((char)c);
                }
            }
        }

        count = shell_tokenize(line, tokens, SHELL_MAX_TOKENS);
        if (count > 0) {
            shell_execute(count, tokens);
            PROC_WAIT(p, !saramos_proc_has_children(p));
            saramos_proc_wait_children(p);
        }
    }

    proc_puts("mountfs: leaving shell\r\n");
    PROC_END(p);
}
