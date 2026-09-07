/*
 * Colored-screen diagnostic app for saramOS.
 *
 * Cycles the whole panel through solid full-screen colors (Red, Green,
 * Blue, White, Cyan, Magenta, Yellow -- each held 3s, with a black clear
 * in between), so any DSI/LTDC positional or color-channel corruption
 * shows up as an unambiguous "which color, which region of the screen"
 * fact instead of something that has to be described secondhand from a
 * complex picture (a house scene, LVGL console text, ...). A pure solid
 * fill also makes tearing/banding/position artifacts far easier to
 * characterize precisely than a busy image would: any deviation from a
 * single flat color IS the artifact, with nothing else competing for
 * attention.
 */
#include <hal/board.h>
#include "hal_sdram.h"
#include "hal_display.h"
#include <stdint.h>

extern void hal_uart_puts(const char *s);
extern volatile uint32_t saramos_tick_ms;

static volatile uint8_t g_colortest_ready = 0;

static void delay_ms(uint32_t ms)
{
    uint32_t start = saramos_tick_ms;
    while ((saramos_tick_ms - start) < ms)
        ;
}

/* Compensating channel rotation: a colortest run (pure R/G/B/W/C/M/Y
 * fills, one per screen, held long enough to rule out any timing
 * ambiguity) showed the on-screen result is consistently R'=commanded
 * G, G'=commanded B, B'=commanded R -- verified exactly against every
 * one of the 7 colors (White, whose channels are all equal, was the
 * one color unaffected by this, which is why the bug went unnoticed in
 * blended-color content like the house picture or LVGL text/background
 * this entire investigation until a synthetic primary-color test made
 * it unambiguous). Wherever this rotation actually happens (DSI wrapper
 * WCFGR.COLMUX="RGB888" doesn't have an alternate/BGR constant in the
 * real ST HAL either, so it isn't simply the wrong enum value -- see
 * commit message), pre-rotating here compensates for it in software:
 * to make (r,g,b) actually APPEAR as (r,g,b), send (b,r,g) instead, so
 * that the hardware's R'=G_sent/G'=B_sent/B'=R_sent recovers the
 * original color. */
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t sr = b, sg = r, sb = g;
    return (uint16_t)(((uint16_t)(sr & 0xF8U) << 8) |
                       ((uint16_t)(sg & 0xFCU) << 3) |
                       ((uint16_t)(sb >> 3)));
}

static void fill_screen(uint16_t color)
{
    volatile uint16_t *fb = (volatile uint16_t *)hal_display_fb_addr();
    uint32_t n = (uint32_t)DISPLAY_WIDTH * (uint32_t)DISPLAY_HEIGHT;
    for (uint32_t i = 0; i < n; i++)
        fb[i] = color;
    __asm volatile ("dsb" ::: "memory");
}

/* Fills [x0,x1) x [y0,y1) with color; the rest of the framebuffer is
 * left untouched by this call (caller clears to black first). Used by
 * halftest to isolate which half of the screen (if any) a color-channel
 * or positional artifact is actually confined to. */
static void fill_rect(int x0, int y0, int x1, int y1, uint16_t color)
{
    volatile uint16_t *fb = (volatile uint16_t *)hal_display_fb_addr();
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            fb[(uint32_t)y * DISPLAY_WIDTH + (uint32_t)x] = color;
    __asm volatile ("dsb" ::: "memory");
}

/* Each entry: name (for the UART log, so the log line and the on-screen
 * color are always unambiguous even if serial output is captured
 * separately from the visual observation) + RGB565 value. Primaries and
 * secondaries only -- if a specific bit/channel is being dropped or
 * shifted on the DSI wire, a pure primary/secondary makes exactly which
 * one obvious (unlike the house picture's blended tones). */
static const struct {
    const char *name;
    uint8_t r, g, b;
} k_colors[] = {
    {"RED",     255, 0,   0  },
    {"GREEN",   0,   255, 0  },
    {"BLUE",    0,   0,   255},
    {"WHITE",   255, 255, 255},
    {"CYAN",    0,   255, 255},
    {"MAGENTA", 255, 0,   255},
    {"YELLOW",  255, 255, 0  },
};
#define K_NUM_COLORS ((int)(sizeof(k_colors) / sizeof(k_colors[0])))

static void cli_colortest(const char *arg)
{
    (void)arg;

    if (!g_colortest_ready) {
        hal_uart_puts("colortest: init display\r\n");
        hal_sdram_init();
        hal_display_init();
        fill_screen(rgb565(0, 0, 0)); /* black first frame, before video starts */
        hal_display_start_video();
        g_colortest_ready = 1;
        hal_uart_puts("colortest: display ready\r\n");
    }

    for (int i = 0; i < K_NUM_COLORS; i++) {
        hal_uart_puts("colortest: clear\r\n");
        fill_screen(rgb565(0, 0, 0));
        delay_ms(3000U);

        char buf[64];
        int off = 0;
        const char *prefix = "colortest: showing ";
        while (prefix[off]) { buf[off] = prefix[off]; off++; }
        const char *name = k_colors[i].name;
        int j = 0;
        while (name[j] && off < (int)sizeof(buf) - 4) { buf[off++] = name[j++]; }
        buf[off++] = '\r';
        buf[off++] = '\n';
        buf[off] = '\0';
        hal_uart_puts(buf);

        fill_screen(rgb565(k_colors[i].r, k_colors[i].g, k_colors[i].b));
        delay_ms(6000U);
    }

    hal_uart_puts("colortest: clear\r\n");
    fill_screen(rgb565(0, 0, 0));
    hal_uart_puts("colortest: done\r\n");
}

/* Regions to test, in order: top half, bottom half, left half, right
 * half -- each shown against an otherwise-black screen so the color and
 * its exact boundary are both unambiguous. Isolates whether a
 * DSI/LTDC artifact is confined to a specific half of the panel (e.g.
 * a horizontal-porch/packet-size issue would show up only left/right;
 * a vertical-porch/line-count issue only top/bottom) instead of the
 * whole-screen fills colortest does. */
static const struct {
    const char *name;
    int x0, y0, x1, y1;
} k_regions[] = {
    {"TOP",    0,                0, DISPLAY_WIDTH,  DISPLAY_HEIGHT / 2},
    {"BOTTOM", 0, DISPLAY_HEIGHT / 2, DISPLAY_WIDTH,  DISPLAY_HEIGHT},
    {"LEFT",   0,                0, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT},
    {"RIGHT",  DISPLAY_WIDTH / 2, 0, DISPLAY_WIDTH,  DISPLAY_HEIGHT},
};
#define K_NUM_REGIONS ((int)(sizeof(k_regions) / sizeof(k_regions[0])))

static void uart_puts3(const char *a, const char *b, const char *c)
{
    hal_uart_puts(a);
    hal_uart_puts(b);
    hal_uart_puts(c);
}

static void cli_halftest(const char *arg)
{
    (void)arg;

    if (!g_colortest_ready) {
        hal_uart_puts("halftest: init display\r\n");
        hal_sdram_init();
        hal_display_init();
        fill_screen(rgb565(0, 0, 0));
        hal_display_start_video();
        g_colortest_ready = 1;
        hal_uart_puts("halftest: display ready\r\n");
    }

    for (int c = 0; c < K_NUM_COLORS; c++) {
        for (int r = 0; r < K_NUM_REGIONS; r++) {
            hal_uart_puts("halftest: clear\r\n");
            fill_screen(rgb565(0, 0, 0));
            delay_ms(3000U);

            uart_puts3("halftest: showing ", k_colors[c].name, " ");
            uart_puts3(k_regions[r].name, " half\r\n", "");

            fill_rect(k_regions[r].x0, k_regions[r].y0,
                      k_regions[r].x1, k_regions[r].y1,
                      rgb565(k_colors[c].r, k_colors[c].g, k_colors[c].b));
            delay_ms(12000U);
        }
    }

    hal_uart_puts("halftest: clear\r\n");
    fill_screen(rgb565(0, 0, 0));
    hal_uart_puts("halftest: done\r\n");
}

/* Thin (40px) vertical white stripes at 5 known X offsets across the
 * full 800px width, one at a time against black, each held long enough
 * to read off precisely: does the stripe appear where it should, does
 * it appear stretched/duplicated, or does it not appear at all? A
 * halftest run showed the left half's content spreads dimly across the
 * WHOLE screen while the right half's content never appears at all --
 * a classic ~2:1 horizontal compression + data-loss pattern. Narrow
 * stripes at specific X values map input-X to displayed-X directly,
 * instead of inferring the transform from two 400px-wide blocks. */
static const int k_stripe_x[] = {0, 190, 380, 570, 760};
#define K_NUM_STRIPES ((int)(sizeof(k_stripe_x) / sizeof(k_stripe_x[0])))
#define STRIPE_WIDTH 40

static void cli_stripetest(const char *arg)
{
    (void)arg;

    if (!g_colortest_ready) {
        hal_uart_puts("stripetest: init display\r\n");
        hal_sdram_init();
        hal_display_init();
        fill_screen(rgb565(0, 0, 0));
        hal_display_start_video();
        g_colortest_ready = 1;
        hal_uart_puts("stripetest: display ready\r\n");
    }

    for (int i = 0; i < K_NUM_STRIPES; i++) {
        hal_uart_puts("stripetest: clear\r\n");
        fill_screen(rgb565(0, 0, 0));
        delay_ms(3000U);

        int x0 = k_stripe_x[i];
        int x1 = x0 + STRIPE_WIDTH;
        if (x1 > (int)DISPLAY_WIDTH) x1 = (int)DISPLAY_WIDTH;

        char buf[64];
        int off = 0;
        const char *prefix = "stripetest: showing stripe at x=";
        while (prefix[off]) { buf[off] = prefix[off]; off++; }
        /* x0 is always 0-799, so at most 3 digits. */
        int digits[3], nd = 0;
        int v = x0;
        if (v == 0) { digits[nd++] = 0; }
        while (v > 0) { digits[nd++] = v % 10; v /= 10; }
        for (int k = nd - 1; k >= 0; k--) buf[off++] = (char)('0' + digits[k]);
        buf[off++] = '\r';
        buf[off++] = '\n';
        buf[off] = '\0';
        hal_uart_puts(buf);

        fill_rect(x0, 0, x1, (int)DISPLAY_HEIGHT, rgb565(255, 255, 255));
        delay_ms(8000U);
    }

    hal_uart_puts("stripetest: clear\r\n");
    fill_screen(rgb565(0, 0, 0));
    hal_uart_puts("stripetest: done\r\n");
}

/* Shows RED-TOP then RED-LEFT, each held 40s (vs. halftest's 12s), back
 * to back in one run. halftest showed LEFT visibly "spreading"/
 * strengthening over its 12s hold; this checks whether TOP does the
 * same thing given enough time (i.e. a universal, purely time-based
 * drift that is just slower/less noticeable for a vertical split) or
 * stays flat the whole 40s (i.e. genuinely confined to horizontal
 * splits, not just slower elsewhere). */
static void cli_longtest(const char *arg)
{
    (void)arg;

    if (!g_colortest_ready) {
        hal_uart_puts("longtest: init display\r\n");
        hal_sdram_init();
        hal_display_init();
        fill_screen(rgb565(0, 0, 0));
        hal_display_start_video();
        g_colortest_ready = 1;
        hal_uart_puts("longtest: display ready\r\n");
    }

    hal_uart_puts("longtest: clear\r\n");
    fill_screen(rgb565(0, 0, 0));
    delay_ms(3000U);

    hal_uart_puts("longtest: showing RED TOP half (40s)\r\n");
    fill_rect(0, 0, (int)DISPLAY_WIDTH, (int)DISPLAY_HEIGHT / 2, rgb565(255, 0, 0));
    delay_ms(40000U);

    hal_uart_puts("longtest: clear\r\n");
    fill_screen(rgb565(0, 0, 0));
    delay_ms(3000U);

    hal_uart_puts("longtest: showing RED LEFT half (40s)\r\n");
    fill_rect(0, 0, (int)DISPLAY_WIDTH / 2, (int)DISPLAY_HEIGHT, rgb565(255, 0, 0));
    delay_ms(40000U);

    hal_uart_puts("longtest: clear\r\n");
    fill_screen(rgb565(0, 0, 0));
    hal_uart_puts("longtest: done\r\n");
}

/* Shows GREEN as a FULL-WIDTH horizontal band (no left/right split at
 * all) at 5 different Y positions -- top, upper-mid, middle, lower-mid,
 * bottom -- each held long enough to read the color off precisely.
 * Tests directly whether the apparent color drifts with Y position even
 * for uniform, full-width content (which halftest/colortest never
 * isolated -- they only checked whether the intended region lit up, not
 * whether its exact color held constant down the frame). If the color
 * itself shifts by row, the RGB channel-rotation "bug" fixed earlier
 * this session and the horizontal-split "bleeding" artifact are likely
 * the same root cause (a rotation amount that isn't actually constant
 * across the frame, just averaged-out/invisible for solid full-frame
 * fills) rather than two separate bugs. */
static const int k_band_y[] = {0, 100, 200, 300, 400};
#define K_NUM_BANDS ((int)(sizeof(k_band_y) / sizeof(k_band_y[0])))
#define BAND_HEIGHT 60

static void cli_bandtest(const char *arg)
{
    (void)arg;

    if (!g_colortest_ready) {
        hal_uart_puts("bandtest: init display\r\n");
        hal_sdram_init();
        hal_display_init();
        fill_screen(rgb565(0, 0, 0));
        hal_display_start_video();
        g_colortest_ready = 1;
        hal_uart_puts("bandtest: display ready\r\n");
    }

    for (int i = 0; i < K_NUM_BANDS; i++) {
        hal_uart_puts("bandtest: clear\r\n");
        fill_screen(rgb565(0, 0, 0));
        delay_ms(3000U);

        int y0 = k_band_y[i];
        int y1 = y0 + BAND_HEIGHT;
        if (y1 > (int)DISPLAY_HEIGHT) y1 = (int)DISPLAY_HEIGHT;

        char buf[64];
        int off = 0;
        const char *prefix = "bandtest: showing GREEN band at y=";
        while (prefix[off]) { buf[off] = prefix[off]; off++; }
        int digits[3], nd = 0;
        int v = y0;
        if (v == 0) { digits[nd++] = 0; }
        while (v > 0) { digits[nd++] = v % 10; v /= 10; }
        for (int k = nd - 1; k >= 0; k--) buf[off++] = (char)('0' + digits[k]);
        buf[off++] = '\r';
        buf[off++] = '\n';
        buf[off] = '\0';
        hal_uart_puts(buf);

        fill_rect(0, y0, (int)DISPLAY_WIDTH, y1, rgb565(0, 255, 0));
        delay_ms(8000U);
    }

    hal_uart_puts("bandtest: clear\r\n");
    fill_screen(rgb565(0, 0, 0));
    hal_uart_puts("bandtest: done\r\n");
}

/* Classic SMPTE-style color bars: 7 equal-width vertical stripes
 * spanning the full screen, all shown at once (White, Yellow, Cyan,
 * Green, Magenta, Red, Blue -- decreasing luminance left to right, the
 * traditional TV test-pattern order). Puts every color and 6
 * simultaneous vertical (horizontal-direction) transitions on screen
 * together as one static pattern -- bandtest (full-width horizontal
 * bands) already showed color is stable down the frame (Y-independent,
 * ruling out a Y-drifting rotation), so this is the complementary
 * check: is the halftest/stripetest bleeding artifact visible at every
 * X boundary simultaneously, confined to specific boundaries, or does
 * having many transitions on screen at once change the picture (e.g.
 * bleeding between adjacent bars) versus a single isolated boundary? */
static const struct {
    const char *name;
    uint8_t r, g, b;
} k_bars[] = {
    {"WHITE",   255, 255, 255},
    {"YELLOW",  255, 255, 0  },
    {"CYAN",    0,   255, 255},
    {"GREEN",   0,   255, 0  },
    {"MAGENTA", 255, 0,   255},
    {"RED",     255, 0,   0  },
    {"BLUE",    0,   0,   255},
};
#define K_NUM_BARS ((int)(sizeof(k_bars) / sizeof(k_bars[0])))

static void cli_colorbars(const char *arg)
{
    (void)arg;

    if (!g_colortest_ready) {
        hal_uart_puts("colorbars: init display\r\n");
        hal_sdram_init();
        hal_display_init();
        fill_screen(rgb565(0, 0, 0));
        hal_display_start_video();
        g_colortest_ready = 1;
        hal_uart_puts("colorbars: display ready\r\n");
    }

    hal_uart_puts("colorbars: showing SMPTE-style bars (White/Yellow/Cyan/Green/Magenta/Red/Blue)\r\n");
    int bar_w = (int)DISPLAY_WIDTH / K_NUM_BARS;
    for (int i = 0; i < K_NUM_BARS; i++) {
        int x0 = i * bar_w;
        int x1 = (i == K_NUM_BARS - 1) ? (int)DISPLAY_WIDTH : (x0 + bar_w);
        fill_rect(x0, 0, x1, (int)DISPLAY_HEIGHT,
                  rgb565(k_bars[i].r, k_bars[i].g, k_bars[i].b));
    }
    hal_uart_puts("colorbars: done, holding\r\n");
}

void app_register_commands(void)
{
    extern void cli_register_command(const char *name,
                                     void (*fn)(const char *arg));
    cli_register_command("colortest", cli_colortest);
    cli_register_command("halftest", cli_halftest);
    cli_register_command("longtest", cli_longtest);
    cli_register_command("stripetest", cli_stripetest);
    cli_register_command("bandtest", cli_bandtest);
    cli_register_command("colorbars", cli_colorbars);
}
