/*
 * Minimal LVGL v9 color definitions (RGB565).
 */
#ifndef LV_COLOR_H
#define LV_COLOR_H

#include "lv_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LV_COLOR_FORMAT_RGB565  1
#define LV_COLOR_FORMAT_NATIVE  LV_COLOR_FORMAT_RGB565

typedef union {
    uint16_t full;
    struct {
        uint16_t blue : 5;
        uint16_t green : 6;
        uint16_t red : 5;
    } ch;
} lv_color16_t;

typedef lv_color16_t lv_color_t;

/* Compensating channel rotation -- see apps/colored-screen/
 * app_colored_screen.c's rgb565() for the full diagnosis (a solid-
 * primary-color test cycle on this exact board/panel driver): the
 * actual on-screen result is R'=commanded G, G'=commanded B, B'=
 * commanded R. White/black (equal or zero channels) are unaffected,
 * which is why this went unnoticed in every LVGL console render this
 * whole investigation -- text is white-on-black. Storing (b,r,g)
 * instead of (r,g,b) compensates. */
static inline lv_color_t lv_color_make(uint8_t r, uint8_t g, uint8_t b)
{
    lv_color_t c;
    uint8_t sr = b, sg = r, sb = g;
    c.ch.red   = (sr >> 3) & 0x1F;
    c.ch.green = (sg >> 2) & 0x3F;
    c.ch.blue  = (sb >> 3) & 0x1F;
    return c;
}

static inline lv_color_t lv_color_hex(uint32_t c)
{
    return lv_color_make((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

static inline lv_color_t lv_color_black(void)  { return lv_color_make(0, 0, 0); }
static inline lv_color_t lv_color_white(void)  { return lv_color_make(255, 255, 255); }
static inline lv_color_t lv_color_red(void)    { return lv_color_make(255, 0, 0); }
static inline lv_color_t lv_color_green(void)  { return lv_color_make(0, 255, 0); }
static inline lv_color_t lv_color_blue(void)   { return lv_color_make(0, 0, 255); }

#define LV_COLOR_BLACK          lv_color_black()
#define LV_COLOR_WHITE          lv_color_white()

#ifdef __cplusplus
}
#endif

#endif /* LV_COLOR_H */
