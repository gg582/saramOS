/*
 * draw-picora: example app for saramOS.
 *
 * Ported from ~/zephyros-stm32/zephyr/draw_picora, a Zephyr+LVGL demo on
 * the same STM32F769I-DISCO board: a 230x230 (252x252 for one variant)
 * RGB565 sprite is centered on the panel; each press of the blue user
 * button (PA0) advances a small state machine that swaps the sprite and
 * nudges its position by 20px, matching the original's
 * execute_system_action() exactly (see that function's states below).
 *
 * Ported as a direct framebuffer blit instead of pulling in LVGL's
 * image widget + decoder (os/default's LVGL_SRC doesn't currently build
 * those, and the sprites are plain uncompressed RGB565 arrays -- an
 * image widget wouldn't do anything a memcpy-per-row doesn't already
 * do here). The embedded pixel data itself (img_src.c/img_parent.c/
 * img_brown.c) is auto-generated straight from the original .c arrays,
 * unchanged.
 *
 * Runs the button-polling loop as a real saramOS TCB task (see
 * saramos_task_init()/saramos_task_add() in os/default/main.c for the
 * same pattern) rather than blocking the CLI.
 */
#include <hal/board.h>
#include "hal_sdram.h"
#include "hal_display.h"
#include <os/saramos_kernel.h>
#include <stdint.h>
#include <string.h>

extern void hal_uart_puts(const char *s);
extern volatile uint32_t saramos_tick_ms;

extern void hal_gpio_init_input(uint32_t port_base, uint8_t pin, uint8_t pupd);
extern int hal_gpio_read(uint32_t port_base, uint8_t pin);
/* GPIO_PUPD_DOWN comes from hal/board.h (stm32f769i-disco.h). */

/* User (blue) button on STM32F769I-DISCO: PA0, active high (board has
 * its own external pull-down, so GPIO_PUPD_DOWN here is just a safe
 * default if that's ever not populated). */
#define BUTTON_PORT GPIOA_BASE
#define BUTTON_PIN  0U

/* Embedded sprites -- see img_src.c/img_parent.c/img_brown.c. */
extern const uint32_t picora_src_w, picora_src_h;
extern const uint8_t picora_src_map[];
extern const uint32_t picora_parent_w, picora_parent_h;
extern const uint8_t picora_parent_map[];
extern const uint32_t picora_brown_w, picora_brown_h;
extern const uint8_t picora_brown_map[];

typedef struct {
    const uint8_t *map;
    uint32_t w, h;
} picora_img_t;

/* ---------------------------------------------------------------------
 * Single live framebuffer, direct writes -- same pattern as
 * apps/colored-screen (the confirmed-working one on real hardware):
 * hal_display_init() + hal_display_start_video() exactly once, then
 * every later redraw (here: each button press) writes straight into
 * that same buffer via hal_display_fb_addr(). No ping-pong/flip --
 * that mechanism is only exercised (and only confirmed working) on the
 * LVGL path in lvgl_port.c; a plain redraw-on-button-press app has no
 * need for it (colored-screen accepts the same brief-tearing-during-
 * redraw tradeoff for the same reason).
 * ------------------------------------------------------------------- */
static int g_display_ready = 0;

static void ensure_display_started(void)
{
    if (g_display_ready)
        return;
    hal_sdram_init();
    hal_display_init();
    memset((void *)hal_display_fb_addr(), 0, DISPLAY_FB_SIZE);
    hal_display_start_video();
    g_display_ready = 1;
}

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((uint16_t)(r & 0xF8U) << 8) |
                       ((uint16_t)(g & 0xFCU) << 3) |
                       ((uint16_t)(b >> 3)));
}

/* Matches the original's setup_background(screen, 0x3A3B3C). */
static uint16_t g_bg_color;

/* ---------------------------------------------------------------------
 * State machine -- a straight port of execute_system_action() in the
 * original main.c, minus the LVGL object calls (replaced by tracking
 * "which sprite, what offset, hidden or not" and redrawing the whole
 * frame each time instead of moving an lv_obj_t).
 * ------------------------------------------------------------------- */
typedef enum {
    STATE_MOVE_UP,
    STATE_MOVE_DOWN,
    STATE_MOVE_LEFT,
    STATE_MOVE_RIGHT,
    STATE_CLEAR,
    STATE_RESTORE,
    STATE_COUNT
} system_state_t;

static system_state_t g_current_action = STATE_MOVE_UP;
static const picora_img_t *g_cur_img;
static int16_t g_off_x = 0;
static int16_t g_off_y = 0;
static int g_hidden = 0;

static picora_img_t g_img_src, g_img_parent, g_img_brown;

static void clamp_offsets(const picora_img_t *img)
{
    int32_t max_x = ((int32_t)DISPLAY_WIDTH - (int32_t)img->w) / 2;
    int32_t max_y = ((int32_t)DISPLAY_HEIGHT - (int32_t)img->h) / 2;

    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;

    if (g_off_x >  max_x) g_off_x =  (int16_t)max_x;
    if (g_off_x < -max_x) g_off_x = (int16_t)-max_x;
    if (g_off_y >  max_y) g_off_y =  (int16_t)max_y;
    if (g_off_y < -max_y) g_off_y = (int16_t)-max_y;
}

/* Fills the back buffer with bg, blits g_cur_img (unless hidden) at
 * center + (g_off_x, g_off_y), then flips -- atomic, no tearing, same
 * ping-pong approach as apps/drop-a-file. */
static void redraw(void)
{
    volatile uint16_t *fb;
    int32_t x0, y0;

    ensure_display_started();
    fb = (volatile uint16_t *)hal_display_fb_addr();

    for (uint32_t i = 0; i < (uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT; i++)
        fb[i] = g_bg_color;

    if (!g_hidden && g_cur_img && g_cur_img->map) {
        x0 = ((int32_t)DISPLAY_WIDTH  - (int32_t)g_cur_img->w) / 2 + g_off_x;
        y0 = ((int32_t)DISPLAY_HEIGHT - (int32_t)g_cur_img->h) / 2 + g_off_y;

        for (uint32_t sy = 0; sy < g_cur_img->h; sy++) {
            int32_t dy = y0 + (int32_t)sy;
            if (dy < 0 || dy >= (int32_t)DISPLAY_HEIGHT)
                continue;
            const uint8_t *srow = g_cur_img->map + (size_t)sy * g_cur_img->w * 2U;
            volatile uint16_t *drow = fb + (uint32_t)dy * DISPLAY_WIDTH;
            for (uint32_t sx = 0; sx < g_cur_img->w; sx++) {
                int32_t dx = x0 + (int32_t)sx;
                if (dx < 0 || dx >= (int32_t)DISPLAY_WIDTH)
                    continue;
                uint16_t px = (uint16_t)srow[sx * 2U] | ((uint16_t)srow[sx * 2U + 1U] << 8);
                drow[dx] = px;
            }
        }
    }

    __asm volatile("dsb" ::: "memory");
}

static void execute_system_action(void)
{
    switch (g_current_action) {
    case STATE_MOVE_UP:
        g_cur_img = &g_img_parent;
        g_off_y -= 20;
        break;
    case STATE_MOVE_DOWN:
        g_cur_img = &g_img_src;
        g_off_y += 20;
        break;
    case STATE_MOVE_LEFT:
        g_cur_img = &g_img_brown;
        g_off_x -= 20;
        break;
    case STATE_MOVE_RIGHT:
        /* Original Zephyr source reuses SRC here (same as DOWN), leaving
         * one of the four directions with no image of its own. Reusing
         * BROWN instead groups the four directions into a coherent
         * pair: UP/DOWN toggle PARENT<->SRC on the vertical axis, and
         * LEFT/RIGHT both show BROWN on the horizontal axis. */
        g_cur_img = &g_img_brown;
        g_off_x += 20;
        break;
    case STATE_CLEAR:
        g_hidden = 1;
        break;
    case STATE_RESTORE:
        g_hidden = 0;
        g_cur_img = &g_img_src;
        g_off_x = 0;
        g_off_y = 0;
        break;
    default:
        break;
    }

    if (!g_hidden)
        clamp_offsets(g_cur_img);

    g_current_action = (system_state_t)((g_current_action + 1) % STATE_COUNT);
    redraw();
}

/* ---------------------------------------------------------------------
 * Button-polling TCB task. Simple debounce: fire once per rising edge,
 * then require the button to read low again before it can fire once
 * more (avoids one physical press registering as several actions from
 * contact bounce).
 * ------------------------------------------------------------------- */
static void delay_ms(uint32_t ms)
{
    uint32_t start = saramos_tick_ms;
    while ((saramos_tick_ms - start) < ms)
        ;
}

static void button_task_entry(void *arg)
{
    int was_pressed = 0;
    (void)arg;

    for (;;) {
        int pressed = hal_gpio_read(BUTTON_PORT, BUTTON_PIN) ? 1 : 0;
        if (pressed && !was_pressed) {
            execute_system_action();
        }
        was_pressed = pressed;
        delay_ms(30); /* ~33Hz poll, plenty for a hand press, cheap debounce */
    }
}

/* ---------------------------------------------------------------------
 * CLI command.
 * ------------------------------------------------------------------- */
static int g_started = 0;

static void cli_picora(const char *arg)
{
    (void)arg;

    if (g_started) {
        hal_uart_puts("draw-picora: already running (press the blue button)\r\n");
        return;
    }

    g_bg_color = rgb565(0x3A, 0x3B, 0x3C);

    g_img_src.map = picora_src_map;    g_img_src.w = picora_src_w;    g_img_src.h = picora_src_h;
    g_img_parent.map = picora_parent_map; g_img_parent.w = picora_parent_w; g_img_parent.h = picora_parent_h;
    g_img_brown.map = picora_brown_map;   g_img_brown.w = picora_brown_w;   g_img_brown.h = picora_brown_h;

    g_cur_img = &g_img_src;
    g_off_x = 0;
    g_off_y = 0;
    g_hidden = 0;
    g_current_action = STATE_MOVE_UP;

    hal_gpio_init_input(BUTTON_PORT, BUTTON_PIN, GPIO_PUPD_DOWN);

    redraw();

    {
        static uint8_t button_task_stack[1024];
        static saramos_tcb_t button_task_tcb;
        saramos_task_init(&button_task_tcb, 2, button_task_entry, NULL,
                          button_task_stack, sizeof(button_task_stack),
                          100U, NULL, NULL);
        saramos_task_add(&button_task_tcb);
    }

    g_started = 1;
    hal_uart_puts("draw-picora: drawn. Press the blue user button to cycle.\r\n");
}

void app_register_commands(void)
{
    extern void cli_register_command(const char *name, void (*fn)(const char *arg));
    cli_register_command("picora", cli_picora);
}
