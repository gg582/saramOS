#include "sudoku.h"
#include <hal/board.h>

static int sudoku_getc_cli(void)
{
    return (int)hal_uart_getc();
}

static void cli_sudoku(const char *arg)
{
    (void)arg;
    sudoku_run(hal_uart_puts, sudoku_getc_cli);
}

void app_register_commands(void)
{
    extern void cli_register_command(const char *name,
                                     void (*fn)(const char *arg));
    cli_register_command("sudoku", cli_sudoku);
}
