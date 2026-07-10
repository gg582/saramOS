/*
 * Minimal LVGL v9 style implementation.
 */
#include "lv_style.h"
#include <string.h>

void lv_style_init(lv_style_t *style)
{
    memset(style, 0, sizeof(*style));
}

void lv_style_reset(lv_style_t *style)
{
    lv_style_init(style);
}

static void set_prop(lv_style_t *style, lv_style_prop_t prop, lv_style_value_t value)
{
    for (uint8_t i = 0; i < style->cnt; i++) {
        if (style->entries[i].prop == prop) {
            style->entries[i].value = value;
            return;
        }
    }
    if (style->cnt < LV_STYLE_MAX_ENTRIES) {
        style->entries[style->cnt].prop = prop;
        style->entries[style->cnt].value = value;
        style->cnt++;
    }
}

static lv_style_value_t make_num(lv_coord_t v)
{
    lv_style_value_t val;
    val.num = v;
    return val;
}

static lv_style_value_t make_color(lv_color_t v)
{
    lv_style_value_t val;
    val.color = v;
    return val;
}

static lv_style_value_t make_ptr(const void *v)
{
    lv_style_value_t val;
    val.ptr = v;
    return val;
}

void lv_style_set_bg_color(lv_style_t *style, lv_color_t color)
{
    set_prop(style, LV_STYLE_PROP_BG_COLOR, make_color(color));
}

void lv_style_set_text_color(lv_style_t *style, lv_color_t color)
{
    set_prop(style, LV_STYLE_PROP_TEXT_COLOR, make_color(color));
}

void lv_style_set_text_font(lv_style_t *style, const lv_font_t *font)
{
    set_prop(style, LV_STYLE_PROP_TEXT_FONT, make_ptr(font));
}

void lv_style_set_pad_all(lv_style_t *style, lv_coord_t pad)
{
    set_prop(style, LV_STYLE_PROP_PAD_TOP, make_num(pad));
    set_prop(style, LV_STYLE_PROP_PAD_BOTTOM, make_num(pad));
    set_prop(style, LV_STYLE_PROP_PAD_LEFT, make_num(pad));
    set_prop(style, LV_STYLE_PROP_PAD_RIGHT, make_num(pad));
}

void lv_style_set_pad_top(lv_style_t *style, lv_coord_t pad)
{
    set_prop(style, LV_STYLE_PROP_PAD_TOP, make_num(pad));
}

void lv_style_set_pad_bottom(lv_style_t *style, lv_coord_t pad)
{
    set_prop(style, LV_STYLE_PROP_PAD_BOTTOM, make_num(pad));
}

void lv_style_set_pad_left(lv_style_t *style, lv_coord_t pad)
{
    set_prop(style, LV_STYLE_PROP_PAD_LEFT, make_num(pad));
}

void lv_style_set_pad_right(lv_style_t *style, lv_coord_t pad)
{
    set_prop(style, LV_STYLE_PROP_PAD_RIGHT, make_num(pad));
}

void lv_style_set_border_width(lv_style_t *style, lv_coord_t width)
{
    set_prop(style, LV_STYLE_PROP_BORDER_WIDTH, make_num(width));
}

void lv_style_set_border_color(lv_style_t *style, lv_color_t color)
{
    set_prop(style, LV_STYLE_PROP_BORDER_COLOR, make_color(color));
}

void lv_style_set_radius(lv_style_t *style, lv_coord_t radius)
{
    set_prop(style, LV_STYLE_PROP_RADIUS, make_num(radius));
}

void lv_style_set_width(lv_style_t *style, lv_coord_t width)
{
    set_prop(style, LV_STYLE_PROP_WIDTH, make_num(width));
}

void lv_style_set_height(lv_style_t *style, lv_coord_t height)
{
    set_prop(style, LV_STYLE_PROP_HEIGHT, make_num(height));
}

void lv_style_set_x(lv_style_t *style, lv_coord_t x)
{
    set_prop(style, LV_STYLE_PROP_X, make_num(x));
}

void lv_style_set_y(lv_style_t *style, lv_coord_t y)
{
    set_prop(style, LV_STYLE_PROP_Y, make_num(y));
}

lv_style_value_t lv_style_get_prop(const lv_style_t *style, lv_style_prop_t prop)
{
    lv_style_value_t v;
    v.num = 0;
    for (uint8_t i = 0; i < style->cnt; i++) {
        if (style->entries[i].prop == prop) {
            return style->entries[i].value;
        }
    }
    return v;
}
