/*
 * Graphical shell application for saramOS.
 * Initializes the STM32F769I-DISCO display and mirrors UART output onto the
 * on-screen console.
 */
#include <hal/board.h>
#include <os/saramos_process.h>
#include <os/saramos_scheduler.h>
#include "lvgl_port.h"
#include <string.h>
#include <stdio.h>

static volatile uint8_t g_gfx_active = 0;

/* --------------------------------------------------------------------------
 * UART output hook -> ring buffer.
 *
 * hal_uart_putc() can be called from any process, so the hook parks bytes in
 * a ring buffer.  The render process drains it and draws to the framebuffer.
 * -------------------------------------------------------------------------- */
#define GFX_RB_SIZE 1024U

static volatile uint8_t g_rb[GFX_RB_SIZE];
static volatile uint32_t g_rb_head = 0;
static volatile uint32_t g_rb_tail = 0;

static inline void irq_disable(void)
{
    __asm volatile ("cpsid i" ::: "memory");
}

static inline void irq_enable(void)
{
    __asm volatile ("cpsie i" ::: "memory");
}

static int rb_put(char c)
{
    irq_disable();
    uint32_t next = (g_rb_head + 1U) & (GFX_RB_SIZE - 1U);
    if (next == g_rb_tail) {
        irq_enable();
        return -1; /* drop on overflow */
    }
    g_rb[g_rb_head] = (uint8_t)c;
    g_rb_head = next;
    irq_enable();
    return 0;
}

static int rb_get(char *out)
{
    irq_disable();
    if (g_rb_head == g_rb_tail) {
        irq_enable();
        return -1;
    }
    *out = (char)g_rb[g_rb_tail];
    g_rb_tail = (g_rb_tail + 1U) & (GFX_RB_SIZE - 1U);
    irq_enable();
    return 0;
}

/* Hook called from hal_uart_putc() for every transmitted character. */
void hal_uart_output_hook(char c)
{
    if (!g_gfx_active)
        return;

    if (c == '\r')
        return;

    rb_put(c);
}

static void gfxshell_drain_rb(void)
{
    char c;
    while (rb_get(&c) == 0)
        gfx_console_putc(c);
}

static int gfxshell_render_process(saramos_process_t *p)
{
    PROC_BEGIN(p);
    for (;;) {
        gfxshell_drain_rb();
        PROC_SLEEP_MS(p, 16);
    }
    PROC_END(p);
}

static void cli_gfxshell(const char *arg)
{
    (void)arg;

    if (g_gfx_active) {
        hal_uart_puts("gfxshell: already active\r\n");
        return;
    }

    hal_uart_puts("gfxshell: init start\r\n");
    lvgl_port_init();
    hal_uart_puts("gfxshell: console ready\r\n");
    g_gfx_active = 1;

    hal_uart_puts("=== graphical shell ===\r\n");

    /* Spawn the renderer as a top-level process so the CLI does not wait
     * for it to exit before accepting the next command.
     */
    saramos_proc_spawn_child(NULL, "gfxrend", gfxshell_render_process,
                             0, NULL, NULL, NULL, 0);

    hal_uart_puts("gfxshell: graphical shell started\r\n");
}

void app_register_commands(void)
{
    extern void cli_register_command(const char *name,
                                     void (*fn)(const char *arg));
    cli_register_command("gfxshell", cli_gfxshell);
}
