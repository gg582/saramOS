/*
 * Minimal LVGL v9 style definitions.
 */
#ifndef LV_STYLE_H
#define LV_STYLE_H

#include "lv_types.h"
#include "lv_color.h"
#include "../font/lv_font.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LV_STATE_DEFAULT        0x0001
#define LV_PART_MAIN            0x00000000

#define LV_STYLE_PROP_BG_COLOR          0x0100
#define LV_STYLE_PROP_TEXT_COLOR        0x0300
#define LV_STYLE_PROP_TEXT_FONT         0x0500
#define LV_STYLE_PROP_PAD_TOP           0x0A00
#define LV_STYLE_PROP_PAD_BOTTOM        0x0B00
#define LV_STYLE_PROP_PAD_LEFT          0x0C00
#define LV_STYLE_PROP_PAD_RIGHT         0x0D00
#define LV_STYLE_PROP_BORDER_WIDTH      0x1000
#define LV_STYLE_PROP_BORDER_COLOR      0x1100
#define LV_STYLE_PROP_RADIUS            0x1200
#define LV_STYLE_PROP_WIDTH             0x1300
#define LV_STYLE_PROP_HEIGHT            0x1400
#define LV_STYLE_PROP_X                 0x1500
#define LV_STYLE_PROP_Y                 0x1600

typedef uint32_t lv_style_prop_t;

typedef struct {
    lv_color_t color;
} lv_style_value_color_t;

typedef struct {
    const lv_font_t *font;
} lv_style_value_font_t;

typedef union {
    lv_coord_t num;
    lv_color_t color;
    const lv_font_t *ptr;
} lv_style_value_t;

typedef struct {
    lv_style_prop_t prop;
    lv_style_value_t value;
} lv_style_entry_t;

#define LV_STYLE_MAX_ENTRIES    12

typedef struct _lv_style_t {
    lv_style_entry_t entries[LV_STYLE_MAX_ENTRIES];
    uint8_t cnt;
} lv_style_t;

void lv_style_init(lv_style_t *style);
void lv_style_reset(lv_style_t *style);
void lv_style_set_bg_color(lv_style_t *style, lv_color_t color);
void lv_style_set_text_color(lv_style_t *style, lv_color_t color);
void lv_style_set_text_font(lv_style_t *style, const struct _lv_font_t *font);
void lv_style_set_pad_all(lv_style_t *style, lv_coord_t pad);
void lv_style_set_pad_top(lv_style_t *style, lv_coord_t pad);
void lv_style_set_pad_bottom(lv_style_t *style, lv_coord_t pad);
void lv_style_set_pad_left(lv_style_t *style, lv_coord_t pad);
void lv_style_set_pad_right(lv_style_t *style, lv_coord_t pad);
void lv_style_set_border_width(lv_style_t *style, lv_coord_t width);
void lv_style_set_border_color(lv_style_t *style, lv_color_t color);
void lv_style_set_radius(lv_style_t *style, lv_coord_t radius);
void lv_style_set_width(lv_style_t *style, lv_coord_t width);
void lv_style_set_height(lv_style_t *style, lv_coord_t height);
void lv_style_set_x(lv_style_t *style, lv_coord_t x);
void lv_style_set_y(lv_style_t *style, lv_coord_t y);

lv_style_value_t lv_style_get_prop(const lv_style_t *style, lv_style_prop_t prop);

#ifdef __cplusplus
}
#endif

#endif /* LV_STYLE_H */
