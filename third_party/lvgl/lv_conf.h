/*
 * Minimal LVGL v9 configuration for saramOS graphical shell.
 * 800x480 display, RGB565, partial draw buffer, textarea/label only.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Display resolution */
#define LV_HOR_RES_MAX          800
#define LV_VER_RES_MAX          480

/* Color depth */
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0

/* Use single display */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

/* No OS, no threads */
#define LV_USE_OS               LV_OS_NONE

/* Default screen refresh period */
#define LV_DEF_REFR_PERIOD      16

/* Use default theme */
#define LV_USE_THEME_DEFAULT    1

/* Enable only textarea and label widgets */
#define LV_USE_TEXTAREA         1
#define LV_USE_LABEL            1

/* Built-in font: 8x8 bitmap */
#define LV_FONT_UNSCII_8        1
#define LV_FONT_DEFAULT         &lv_font_unscii_8

/* Disable most optional features */
#define LV_USE_ANIMATION        0
#define LV_USE_INDEV            0
#define LV_USE_LOG              0
#define LV_USE_ASSERT_NULL      0
#define LV_USE_ASSERT_MALLOC    0
#define LV_USE_ASSERT_STYLE     0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_PERF_MONITOR     0
#define LV_USE_MEM_MONITOR      0

/* Memory pool for LVGL internal allocations */
#define LV_MEM_CUSTOM           0
#define LV_MEM_SIZE             (16U * 1024U)

/* Draw buffer */
#define LV_DRAW_BUF_STRIDE_ALIGN 1
#define LV_DRAW_BUF_ALIGN       4

#endif /* LV_CONF_H */
