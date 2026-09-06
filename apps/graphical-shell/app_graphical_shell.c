/*
 * Graphical shell application for saramOS.
 * Initializes the STM32F769I-DISCO display and mirrors UART output onto the
 * on-screen console.
 */
#include <hal/board.h>
#include <os/saramos_process.h>
#include <os/saramos_scheduler.h>
#include "lvgl_port.h"
#include "hal_sdram.h"
#include "hal_display.h"
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
        lvgl_port_tick(16);
        lvgl_port_handler();
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

/* --------------------------------------------------------------------------
 * "picture" command: a minimal, self-contained example that draws one
 * static image (a house on a hillside) directly into the RGB565 SDRAM
 * framebuffer, with no LVGL/console dependency. Useful as a bare-bones
 * check that the DSI/LTDC display path is actually producing a picture.
 * -------------------------------------------------------------------------- */
static volatile uint8_t g_picture_display_ready = 0;

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((uint16_t)(r & 0xF8U) << 8) |
                       ((uint16_t)(g & 0xFCU) << 3) |
                       ((uint16_t)(b >> 3)));
}

static inline void gfx_set_pixel(volatile uint16_t *fb, int x, int y, uint16_t color)
{
    if (x < 0 || y < 0 || x >= (int)DISPLAY_WIDTH || y >= (int)DISPLAY_HEIGHT)
        return;
    fb[(uint32_t)y * DISPLAY_WIDTH + (uint32_t)x] = color;
}

static void gfx_fill_rect(volatile uint16_t *fb, int x0, int y0, int w, int h, uint16_t color)
{
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++)
            gfx_set_pixel(fb, x, y, color);
}

static void gfx_fill_circle(volatile uint16_t *fb, int cx, int cy, int r, uint16_t color)
{
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r)
                gfx_set_pixel(fb, cx + x, cy + y, color);
}

/* Fills a triangle via a bounding-box scan and an edge-function test. */
static void gfx_fill_triangle(volatile uint16_t *fb,
                               int x0, int y0, int x1, int y1, int x2, int y2,
                               uint16_t color)
{
    int minx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int maxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);

    for (int y = miny; y <= maxy; y++) {
        for (int x = minx; x <= maxx; x++) {
            int w0 = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
            int w1 = (x2 - x1) * (y - y1) - (y2 - y1) * (x - x1);
            int w2 = (x0 - x2) * (y - y2) - (y0 - y2) * (x - x2);
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
                gfx_set_pixel(fb, x, y, color);
        }
    }
}

static void draw_house_picture(void)
{
    volatile uint16_t *fb = (volatile uint16_t *)hal_display_fb_addr();

    uint16_t sky    = rgb565(135, 206, 235);
    uint16_t ground = rgb565(76, 175, 80);
    uint16_t sun    = rgb565(255, 221, 51);
    uint16_t wall   = rgb565(198, 70, 58);
    uint16_t roof   = rgb565(101, 67, 33);
    uint16_t door   = rgb565(63, 42, 25);
    uint16_t window = rgb565(120, 200, 255);

    const int ground_h = 140;

    gfx_fill_rect(fb, 0, 0, (int)DISPLAY_WIDTH, (int)DISPLAY_HEIGHT, sky);
    gfx_fill_rect(fb, 0, (int)DISPLAY_HEIGHT - ground_h, (int)DISPLAY_WIDTH, ground_h, ground);
    gfx_fill_circle(fb, 120, 100, 60, sun);

    int house_w = 260, house_h = 200;
    int house_x = 300;
    int house_y = (int)DISPLAY_HEIGHT - ground_h - house_h;

    gfx_fill_rect(fb, house_x, house_y, house_w, house_h, wall);
    gfx_fill_triangle(fb,
                       house_x - 30, house_y,
                       house_x + house_w + 30, house_y,
                       house_x + house_w / 2, house_y - 120,
                       roof);
    gfx_fill_rect(fb, house_x + house_w / 2 - 30, house_y + house_h - 90, 60, 90, door);
    gfx_fill_rect(fb, house_x + 30, house_y + 40, 60, 60, window);
    gfx_fill_rect(fb, house_x + house_w - 90, house_y + 40, 60, 60, window);

    __asm volatile ("dsb" ::: "memory");
}

static void cli_picture(const char *arg)
{
    (void)arg;

    if (!g_gfx_active && !g_picture_display_ready) {
        hal_uart_puts("picture: init display\r\n");
        hal_sdram_init();
        hal_display_init();
        g_picture_display_ready = 1;
        hal_uart_puts("picture: display ready\r\n");
    }

    draw_house_picture();
    hal_uart_puts("picture: drawn\r\n");
}

void app_register_commands(void)
{
    extern void cli_register_command(const char *name,
                                     void (*fn)(const char *arg));
    cli_register_command("gfxshell", cli_gfxshell);
    cli_register_command("picture", cli_picture);
}
