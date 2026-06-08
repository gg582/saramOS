#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <hal/stm32f769i-disc1.h>
#include <ttak/mem/mem.h>
#include <ttak/timing/timing.h>
#include <ttak/async/function.h>
#include <ttak/async/sched.h>
#include <os/saramos_arena.h>
#include <os/saramos_owner.h>
#include "game.h"

#define CMD_BUF_SIZE 64

static saramos_arena_t sys_arena;
static saramos_owner_t sys_owner;
static int arena_initialized = 0;
static int owner_initialized = 0;

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
