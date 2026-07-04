#include "program.h"
#include <string.h>

void program_init_builtins(void)
{
    static const char *const arith_lines[] = {
        "set 12",
        "add 8",
        "mul 3",
        "sub 10",
        "div 5",
        "print"
    };
    static const char *const modulo_lines[] = {
        "set 29",
        "mod 5",
        "print"
    };

    cli_program_t *prog;

    prog = program_get_or_alloc("arith");
    if (prog) {
        prog->line_count = 0;
        for (unsigned int i = 0; i < sizeof(arith_lines) / sizeof(arith_lines[0]) && i < PROGRAM_MAX_LINES; i++) {
            strncpy(prog->lines[i], arith_lines[i], PROGRAM_LINE_SIZE - 1);
            prog->lines[i][PROGRAM_LINE_SIZE - 1] = '\0';
            prog->line_count++;
        }
    }

    prog = program_get_or_alloc("modulo");
    if (prog) {
        prog->line_count = 0;
        for (unsigned int i = 0; i < sizeof(modulo_lines) / sizeof(modulo_lines[0]) && i < PROGRAM_MAX_LINES; i++) {
            strncpy(prog->lines[i], modulo_lines[i], PROGRAM_LINE_SIZE - 1);
            prog->lines[i][PROGRAM_LINE_SIZE - 1] = '\0';
            prog->line_count++;
        }
    }

}
