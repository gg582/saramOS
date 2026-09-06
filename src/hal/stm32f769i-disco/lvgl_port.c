/*
 * Display/console port for STM32F769I-DISCO graphical shell.
 *
 * Unlike the earlier version of this file, the on-screen console is now
 * actually driven through LVGL (the vendored, minimal LVGL v9 API-compatible
 * implementation in third_party/lvgl) instead of a hand-rolled font blitter:
 * a single full-screen lv_label object holds the console text, and LVGL's
 * own software rasterizer draws it. The label's render buffer *is* the real
 * LTDC/DSI framebuffer in SDRAM (LV_DISPLAY_RENDER_MODE_DIRECT), so nothing
 * extra needs to be copied out -- the LTDC scans the same memory LVGL just
 * rendered into.
 */
#include "lvgl_port.h"
#include "hal_sdram.h"
#include "hal_display.h"
#include <hal/board.h>
#include <string.h>
#include <stdio.h>
#include "lvgl.h"

extern void hal_uart_puts(const char *s);

/* --------------------------------------------------------------------------
 * Console text buffer: a fixed grid of rows/cols (matching the on-screen
 * 8x8-font layout), rebuilt into one newline-joined string and handed to
 * the LVGL label only when something actually changed. LVGL's own
 * lv_draw_label() understands '\n' as a line break, so building the joined
 * string here is enough to get the same wrap/scroll behavior the old
 * per-pixel blitter had.
 * -------------------------------------------------------------------------- */
#define CONSOLE_FONT_WIDTH  8U
#define CONSOLE_FONT_HEIGHT 8U
#define COLS    (DISPLAY_WIDTH  / CONSOLE_FONT_WIDTH)
#define ROWS    (DISPLAY_HEIGHT / CONSOLE_FONT_HEIGHT)

static lv_obj_t *g_console_label = NULL;
static char g_rows[ROWS][COLS + 1U];
static char g_text_buf[LV_LABEL_TEXT_LEN_MAX];
static uint32_t g_cursor_col = 0;
static uint32_t g_cursor_row = 0;
static volatile uint8_t g_dirty = 0;

static inline void irq_disable(void)
{
    __asm volatile ("cpsid i" ::: "memory");
}

static inline void irq_enable(void)
{
    __asm volatile ("cpsie i" ::: "memory");
}

static void row_clear(uint32_t row)
{
    memset(g_rows[row], ' ', COLS);
    g_rows[row][COLS] = '\0';
}

static void scroll_up(void)
{
    memmove(g_rows[0], g_rows[1], (size_t)(ROWS - 1U) * sizeof(g_rows[0]));
    row_clear(ROWS - 1U);
}

static void console_newline(void)
{
    g_cursor_col = 0;
    g_cursor_row++;
    if (g_cursor_row >= ROWS) {
        scroll_up();
        g_cursor_row = ROWS - 1U;
    }
}

void gfx_console_putc(char c)
{
    irq_disable();

    if (c == '\r') {
        irq_enable();
        return;
    }
    if (c == '\n') {
        console_newline();
        g_dirty = 1;
        irq_enable();
        return;
    }
    if (c == '\b') {
        if (g_cursor_col > 0U)
            g_cursor_col--;
        g_dirty = 1;
        irq_enable();
        return;
    }
    if (c == '\t') {
        uint32_t next = (g_cursor_col + 8U) & ~7U;
        while (g_cursor_col < next && g_cursor_col < COLS)
            g_rows[g_cursor_row][g_cursor_col++] = ' ';
        if (g_cursor_col >= COLS)
            console_newline();
        g_dirty = 1;
        irq_enable();
        return;
    }

    g_rows[g_cursor_row][g_cursor_col] = c;
    g_cursor_col++;
    if (g_cursor_col >= COLS)
        console_newline();
    g_dirty = 1;

    irq_enable();
}

/* Rebuild g_text_buf from the row grid (trimming trailing spaces per row to
 * save space) and hand it to the label. Called only when g_dirty is set. */
static void console_flush(void)
{
    uint32_t off = 0;

    irq_disable();
    for (uint32_t r = 0; r < ROWS; r++) {
        uint32_t len = COLS;
        while (len > 0U && g_rows[r][len - 1U] == ' ')
            len--;
        if (off + len + 1U >= sizeof(g_text_buf))
            break;
        memcpy(g_text_buf + off, g_rows[r], len);
        off += len;
        g_text_buf[off++] = '\n';
    }
    if (off > 0U)
        off--; /* drop the trailing newline after the last row */
    g_text_buf[off] = '\0';
    g_dirty = 0;
    irq_enable();

    if (g_console_label)
        lv_label_set_text(g_console_label, g_text_buf);
}

/* LV_DISPLAY_RENDER_MODE_DIRECT means lv_timer_handler() already rendered
 * straight into the real framebuffer (see lvgl_port_init() below) -- there
 * is nothing left to copy out, just acknowledge the flush. */
static void lv_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)area;
    (void)px_map;
    lv_display_flush_ready(disp);
}

void lvgl_port_init(void)
{
    hal_uart_puts("[LVGL] sdram init\r\n");
    hal_sdram_init();
    hal_uart_puts("[LVGL] display init\r\n");
    hal_display_init();

    hal_uart_puts("[LVGL] lv_init\r\n");
    lv_init();

    lv_display_t *disp = lv_display_get_default();
    lv_display_set_physical_resolution(disp, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    /* The label draws straight into the SDRAM framebuffer the LTDC scans
     * out -- render mode DIRECT, no separate copy/flush buffer. */
    lv_display_set_buffers(disp, (void *)hal_display_fb_addr(), NULL,
                            (uint32_t)DISPLAY_WIDTH * (uint32_t)DISPLAY_HEIGHT *
                                (uint32_t)sizeof(uint16_t),
                            LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, lv_flush_cb);

    g_console_label = lv_label_create(lv_screen_active());
    if (!g_console_label) {
        hal_uart_puts("[LVGL] label create FAILED\r\n");
        return;
    }
    lv_obj_set_pos(g_console_label, 0, 0);
    lv_obj_set_size(g_console_label, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(g_console_label, lv_color_black(), 0);
    lv_obj_set_style_text_color(g_console_label, lv_color_white(), 0);
    lv_obj_set_style_pad_all(g_console_label, 0, 0);

    for (uint32_t r = 0; r < ROWS; r++)
        row_clear(r);
    g_cursor_col = 0;
    g_cursor_row = 0;

    /* Diagnostic text visible immediately after init, before any UART hook. */
    const char *test = "TEST FONT (LVGL)";
    while (*test)
        gfx_console_putc(*test++);

    console_flush();
    lv_timer_handler();

    hal_uart_puts("[LVGL] port init done\r\n");
}

void lvgl_port_tick(uint32_t ms)
{
    lv_tick_inc(ms);
}

uint32_t lvgl_port_handler(void)
{
    if (g_dirty) {
        console_flush();
        lv_timer_handler();
    }
    return 16;
}
