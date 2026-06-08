#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <hal/stm32f769i-disc1.h>
#include <ttak/mem/mem.h>
#include <ttak/timing/timing.h>
#include <ttak/async/function.h>
#include <ttak/async/sched.h>
#include <os/saramos_arena.h>
#include <os/saramos_kernel.h>
#include <os/saramos_owner.h>
#include "game.h"

#define CMD_BUF_SIZE 64
#define PROGRAM_MAX_COUNT 8
#define PROGRAM_NAME_SIZE 16
#define PROGRAM_MAX_LINES 16
#define PROGRAM_LINE_SIZE 32

static saramos_arena_t sys_arena;
static saramos_owner_t sys_owner;
static int arena_initialized = 0;
static int owner_initialized = 0;

typedef struct {
    int used;
    char name[PROGRAM_NAME_SIZE];
    unsigned int line_count;
    char lines[PROGRAM_MAX_LINES][PROGRAM_LINE_SIZE];
} cli_program_t;

static cli_program_t programs[PROGRAM_MAX_COUNT];

static void cli_read_line(char *buf, size_t size);

static void cli_prompt(void)
{
    hal_uart_puts("saramOS> ");
}

static void cli_help(void)
{
    hal_uart_puts(
        "Available commands:\r\n"
        "  help       - Show this help message\r\n"
        "  status     - Show OS status (arena, owner, ticks)\r\n"
        "  alloc <n>  - Allocate n bytes from arena\r\n"
        "  reset      - Reset current arena generation\r\n"
        "  rotate     - Rotate arena to new generation\r\n"
        "  hello      - Print hello message\r\n"
        "  heartbeat  - Print heartbeat once\r\n"
        "  program    - Create/list/run calculator programs\r\n"
        "  game       - Play mini Sudoku\r\n"
        "  clear      - Clear screen\r\n"
    );
}

static void cli_status(void)
{
    char buf[64];

    hal_uart_puts("--- OS Status ---\r\n");

    if (arena_initialized) {
        size_t rem = saramos_arena_remaining(&sys_arena);
        snprintf(buf, sizeof(buf), "Arena: initialized, remaining=%u bytes\r\n", (unsigned)rem);
        hal_uart_puts(buf);
    } else {
        hal_uart_puts("Arena: not initialized\r\n");
    }

    if (owner_initialized) {
        hal_uart_puts("Owner: initialized (kernel)\r\n");
    } else {
        hal_uart_puts("Owner: not initialized\r\n");
    }

    snprintf(buf, sizeof(buf), "Tick count: %lu\r\n", (unsigned long)ttak_get_tick_count());
    hal_uart_puts(buf);

    hal_uart_puts("-----------------\r\n");
}

static unsigned int parse_uint(const char *s)
{
    unsigned int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (unsigned int)(*s - '0');
        s++;
    }
    return n;
}

static void cli_alloc(const char *arg)
{
    if (!arena_initialized) {
        hal_uart_puts("Arena not initialized.\r\n");
        return;
    }

    if (!arg || *arg == '\0') {
        hal_uart_puts("Usage: alloc <size>\r\n");
        return;
    }

    unsigned int n = parse_uint(arg);
    if (n == 0) {
        hal_uart_puts("Invalid size.\r\n");
        return;
    }

    void *p = saramos_arena_alloc(&sys_arena, (size_t)n);
    if (p) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Allocated %u bytes at %p\r\n", n, p);
        hal_uart_puts(buf);
    } else {
        hal_uart_puts("Allocation failed.\r\n");
    }
}

static void cli_reset(void)
{
    if (!arena_initialized) {
        hal_uart_puts("Arena not initialized.\r\n");
        return;
    }
    saramos_arena_reset(&sys_arena);
    hal_uart_puts("Arena reset OK\r\n");
}

static void cli_rotate(void)
{
    if (!arena_initialized) {
        hal_uart_puts("Arena not initialized.\r\n");
        return;
    }
    saramos_arena_rotate(&sys_arena);
    hal_uart_puts("Arena rotate OK\r\n");
}

static void cli_clear(void)
{
    hal_uart_puts("\x1B[2J\x1B[H");
}

static void cli_hello(void)
{
    hal_uart_puts("Hello from saramOS!\r\n");
}

static void cli_heartbeat(void)
{
    hal_uart_puts("Heartbeat from saramOS\r\n");
}

static void skip_spaces(const char **s)
{
    while (**s == ' ' || **s == '\t')
        (*s)++;
}

static int program_name_is_valid(const char *name)
{
    unsigned int len = 0;

    if (!name || *name == '\0')
        return 0;

    while (name[len] != '\0') {
        char c = name[len];
        int ok = (c >= 'a' && c <= 'z') ||
                 (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') ||
                 c == '_';
        if (!ok)
            return 0;
        len++;
    }

    return len < PROGRAM_NAME_SIZE;
}

static cli_program_t *program_find(const char *name)
{
    for (unsigned int i = 0; i < PROGRAM_MAX_COUNT; i++) {
        if (programs[i].used && strcmp(programs[i].name, name) == 0)
            return &programs[i];
    }

    return NULL;
}

static cli_program_t *program_get_or_alloc(const char *name)
{
    cli_program_t *prog = program_find(name);

    if (prog)
        return prog;

    for (unsigned int i = 0; i < PROGRAM_MAX_COUNT; i++) {
        if (!programs[i].used) {
            memset(&programs[i], 0, sizeof(programs[i]));
            programs[i].used = 1;
            strncpy(programs[i].name, name, PROGRAM_NAME_SIZE - 1);
            return &programs[i];
        }
    }

    return NULL;
}

static int parse_int32(const char *s, int *out)
{
    int sign = 1;
    int value = 0;
    int digits = 0;

    skip_spaces(&s);

    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        s++;
        digits++;
    }

    skip_spaces(&s);

    if (digits == 0 || *s != '\0')
        return 0;

    *out = value * sign;
    return 1;
}

static void program_load_builtin(const char *name, const char *const *lines, unsigned int line_count)
{
    cli_program_t *prog = program_get_or_alloc(name);

    if (!prog)
        return;

    prog->line_count = 0;
    for (unsigned int i = 0; i < line_count && i < PROGRAM_MAX_LINES; i++) {
        strncpy(prog->lines[i], lines[i], PROGRAM_LINE_SIZE - 1);
        prog->lines[i][PROGRAM_LINE_SIZE - 1] = '\0';
        prog->line_count++;
    }
}

static void program_init_builtins(void)
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

    program_load_builtin("arith", arith_lines, sizeof(arith_lines) / sizeof(arith_lines[0]));
    program_load_builtin("modulo", modulo_lines, sizeof(modulo_lines) / sizeof(modulo_lines[0]));
}

static void program_list(void)
{
    char buf[64];
    int any = 0;

    hal_uart_puts("Programs:\r\n");
    for (unsigned int i = 0; i < PROGRAM_MAX_COUNT; i++) {
        if (programs[i].used) {
            snprintf(buf, sizeof(buf), "  %s (%u lines)\r\n", programs[i].name, programs[i].line_count);
            hal_uart_puts(buf);
            any = 1;
        }
    }

    if (!any)
        hal_uart_puts("  <none>\r\n");
}

static int program_exec_line(const char *line, int *acc)
{
    char op[8];
    const char *arg;
    int value;
    char buf[48];
    unsigned int op_len = 0;

    skip_spaces(&line);
    if (*line == '\0')
        return 1;

    while (line[op_len] != '\0' && line[op_len] != ' ' && line[op_len] != '\t') {
        if (op_len + 1 >= sizeof(op))
            return 0;
        op[op_len] = line[op_len];
        op_len++;
    }
    op[op_len] = '\0';

    arg = line + op_len;

    if (strcmp(op, "print") == 0) {
        skip_spaces(&arg);
        if (*arg != '\0')
            return 0;
        snprintf(buf, sizeof(buf), "ACC=%d\r\n", *acc);
        hal_uart_puts(buf);
        return 1;
    }

    if (!parse_int32(arg, &value))
        return 0;

    if (strcmp(op, "set") == 0) {
        *acc = value;
    } else if (strcmp(op, "add") == 0) {
        *acc += value;
    } else if (strcmp(op, "sub") == 0) {
        *acc -= value;
    } else if (strcmp(op, "mul") == 0) {
        *acc *= value;
    } else if (strcmp(op, "div") == 0) {
        if (value == 0)
            return 0;
        *acc /= value;
    } else if (strcmp(op, "mod") == 0) {
        if (value == 0)
            return 0;
        *acc %= value;
    } else {
        return 0;
    }

    return 1;
}

static void program_run(const char *name)
{
    cli_program_t *prog = program_find(name);
    int acc = 0;
    char buf[80];

    if (!prog) {
        hal_uart_puts("Program not found.\r\n");
        return;
    }

    snprintf(buf, sizeof(buf), "Running program '%s'\r\n", prog->name);
    hal_uart_puts(buf);

    for (unsigned int i = 0; i < prog->line_count; i++) {
        if (!program_exec_line(prog->lines[i], &acc)) {
            snprintf(buf, sizeof(buf), "Program error at line %u: %s\r\n", i + 1, prog->lines[i]);
            hal_uart_puts(buf);
            return;
        }
    }

    snprintf(buf, sizeof(buf), "Done. ACC=%d\r\n", acc);
    hal_uart_puts(buf);
}

static void program_edit(const char *name)
{
    cli_program_t *prog;
    char line[PROGRAM_LINE_SIZE];
    char buf[80];

    if (!program_name_is_valid(name)) {
        hal_uart_puts("Usage: program <name>  (letters, digits, underscore; max 15 chars)\r\n");
        return;
    }

    prog = program_get_or_alloc(name);
    if (!prog) {
        hal_uart_puts("Program storage full.\r\n");
        return;
    }

    prog->line_count = 0;
    snprintf(buf, sizeof(buf), "Editing '%s'. Commands: set/add/sub/mul/div/mod/print. Type end to save.\r\n", name);
    hal_uart_puts(buf);

    while (prog->line_count < PROGRAM_MAX_LINES) {
        hal_uart_puts("prog> ");
        cli_read_line(line, sizeof(line));

        if (strcmp(line, "end") == 0) {
            snprintf(buf, sizeof(buf), "Saved '%s' (%u lines).\r\n", prog->name, prog->line_count);
            hal_uart_puts(buf);
            return;
        }

        strncpy(prog->lines[prog->line_count], line, PROGRAM_LINE_SIZE - 1);
        prog->lines[prog->line_count][PROGRAM_LINE_SIZE - 1] = '\0';
        prog->line_count++;
    }

    snprintf(buf, sizeof(buf), "Saved '%s' (%u lines, full).\r\n", prog->name, prog->line_count);
    hal_uart_puts(buf);
}

static void cli_program(const char *arg)
{
    const char *name;

    if (!arg || *arg == '\0' || strcmp(arg, "list") == 0) {
        program_list();
        hal_uart_puts("Use: program <name>, program run <name>, program list\r\n");
        return;
    }

    if (strncmp(arg, "run", 3) == 0 && (arg[3] == '\0' || arg[3] == ' ' || arg[3] == '\t')) {
        name = arg + 3;
        skip_spaces(&name);
        if (!program_name_is_valid(name)) {
            hal_uart_puts("Usage: program run <name>\r\n");
            return;
        }
        program_run(name);
        return;
    }

    program_edit(arg);
}

static int cli_game_getc(void)
{
    return (int)hal_uart_getc();
}

static void cli_game(void)
{
    game_run(hal_uart_puts, cli_game_getc);
}

static void cli_unknown(const char *cmd)
{
    hal_uart_puts("Unknown command: ");
    hal_uart_puts(cmd);
    hal_uart_puts("\r\nType 'help' for available commands.\r\n");
}

static void cli_execute(char *line)
{
    while (*line == ' ' || *line == '\t')
        line++;

    if (*line == '\0')
        return;

    char *space = strchr(line, ' ');
    char *arg = NULL;
    if (space) {
        *space = '\0';
        arg = space + 1;
        while (*arg == ' ' || *arg == '\t')
            arg++;
    }

    if (strcmp(line, "help") == 0) {
        cli_help();
    } else if (strcmp(line, "status") == 0) {
        cli_status();
    } else if (strcmp(line, "alloc") == 0) {
        cli_alloc(arg);
    } else if (strcmp(line, "reset") == 0) {
        cli_reset();
    } else if (strcmp(line, "rotate") == 0) {
        cli_rotate();
    } else if (strcmp(line, "hello") == 0) {
        cli_hello();
    } else if (strcmp(line, "heartbeat") == 0) {
        cli_heartbeat();
    } else if (strcmp(line, "program") == 0) {
        cli_program(arg);
    } else if (strcmp(line, "game") == 0) {
        cli_game();
    } else if (strcmp(line, "clear") == 0) {
        cli_clear();
    } else {
        cli_unknown(line);
    }
}

static void cli_read_line(char *buf, size_t size)
{
    size_t i = 0;
    char c;

    while (1) {
        c = hal_uart_getc();

        if (c == '\r' || c == '\n') {
            hal_uart_puts("\r\n");
            buf[i] = '\0';
            return;
        } else if (c == '\b' || c == 127) {
            if (i > 0) {
                i--;
                hal_uart_puts("\b \b");
            }
        } else if (c >= 32 && c < 127) {
            if (i + 1 < size) {
                buf[i++] = c;
                hal_uart_putc(c);
            }
        }
    }
}

int main(void)
{
    hal_uart_init();

    hal_uart_puts("\r\n");
    hal_uart_puts("=== saramOS on STM32F769I-DISC1 ===\r\n");
    hal_uart_puts("Type 'help' for available commands.\r\n\r\n");

    saramos_kernel_init();
    hal_uart_puts("saramOS: resilient kernel core init OK\r\n");

    /* --- init saramOS arena --- */
    if (saramos_arena_init(&sys_arena)) {
        arena_initialized = 1;
        hal_uart_puts("saramOS: arena init OK\r\n");
    } else {
        hal_uart_puts("saramOS: arena init FAILED\r\n");
    }

    /* --- init saramOS owner --- */
    if (saramos_owner_init(&sys_owner, "kernel")) {
        owner_initialized = 1;
        hal_uart_puts("saramOS: owner init OK\r\n");
    } else {
        hal_uart_puts("saramOS: owner init FAILED\r\n");
    }

    /* --- cooperative scheduler init --- */
    ttak_async_init(0);
    hal_uart_puts("libttak: async scheduler init OK\r\n");
    program_init_builtins();
    hal_uart_puts("example: calculator programs loaded (arith, modulo)\r\n");
    hal_uart_puts("===================================\r\n");

    char cmd_buf[CMD_BUF_SIZE];

    while (1) {
        cli_prompt();
        cli_read_line(cmd_buf, sizeof(cmd_buf));
        cli_execute(cmd_buf);

        /* Drive background tasks */
        ttak_cooperative_run_once(ttak_get_tick_count());
    }

    return 0;
}
