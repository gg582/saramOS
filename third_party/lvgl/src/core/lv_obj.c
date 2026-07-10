/*
 * Minimal LVGL v9 object implementation.
 */
#include "lv_obj.h"
#include "../draw/lv_draw.h"
#include "../widgets/lv_label.h"
#include "../widgets/lv_textarea.h"
#include <string.h>
#include <stdlib.h>

#define LV_OBJ_POOL_SIZE 16
static lv_obj_t g_obj_pool[LV_OBJ_POOL_SIZE];
static int g_obj_used[LV_OBJ_POOL_SIZE] = {0};

static lv_obj_t *obj_alloc(void)
{
    for (int i = 0; i < LV_OBJ_POOL_SIZE; i++) {
        if (!g_obj_used[i]) {
            g_obj_used[i] = 1;
            return &g_obj_pool[i];
        }
    }
    return NULL;
}

static void obj_free(lv_obj_t *obj)
{
    if (obj >= g_obj_pool && obj < g_obj_pool + LV_OBJ_POOL_SIZE) {
        g_obj_used[obj - g_obj_pool] = 0;
    }
}

static lv_coord_t style_num(const lv_obj_t *obj, lv_style_prop_t prop, lv_coord_t def)
{
    lv_style_value_t v = lv_style_get_prop(&obj->style, prop);
    if (v.num != 0 || prop == LV_STYLE_PROP_X || prop == LV_STYLE_PROP_Y ||
        prop == LV_STYLE_PROP_WIDTH || prop == LV_STYLE_PROP_HEIGHT)
        return v.num;
    return def;
}

static lv_color_t style_color(const lv_obj_t *obj, lv_style_prop_t prop, lv_color_t def)
{
    lv_style_value_t v = lv_style_get_prop(&obj->style, prop);
    if (v.color.full != 0) return v.color;
    return def;
}

static const lv_font_t *style_font(const lv_obj_t *obj)
{
    lv_style_value_t v = lv_style_get_prop(&obj->style, LV_STYLE_PROP_TEXT_FONT);
    if (v.ptr) return (const lv_font_t *)v.ptr;
    return LV_FONT_DEFAULT;
}

lv_obj_t *lv_obj_create(lv_obj_t *parent)
{
    lv_obj_t *obj = obj_alloc();
    if (!obj) return NULL;
    memset(obj, 0, sizeof(*obj));
    lv_style_init(&obj->style);
    obj->parent = parent;
    if (parent && parent->child_cnt < LV_OBJ_CHILD_NUM_MAX) {
        parent->children[parent->child_cnt++] = obj;
    }
    return obj;
}

void lv_obj_delete(lv_obj_t *obj)
{
    if (!obj) return;
    for (uint8_t i = 0; i < obj->child_cnt; i++) {
        lv_obj_delete(obj->children[i]);
    }
    if (obj->ext) {
        free(obj->ext);
        obj->ext = NULL;
    }
    obj_free(obj);
}

lv_obj_t *lv_screen_active(void)
{
    extern lv_display_t *lv_display_get_default(void);
    lv_display_t *disp = lv_display_get_default();
    return disp ? disp->screen : NULL;
}

void lv_obj_set_size(lv_obj_t *obj, lv_coord_t w, lv_coord_t h)
{
    if (!obj) return;
    lv_obj_set_width(obj, w);
    lv_obj_set_height(obj, h);
}

void lv_obj_set_width(lv_obj_t *obj, lv_coord_t w)
{
    if (!obj) return;
    lv_style_set_width(&obj->style, w);
    obj->coords.x2 = obj->coords.x1 + w - 1;
}

void lv_obj_set_height(lv_obj_t *obj, lv_coord_t h)
{
    if (!obj) return;
    lv_style_set_height(&obj->style, h);
    obj->coords.y2 = obj->coords.y1 + h - 1;
}

void lv_obj_set_x(lv_obj_t *obj, lv_coord_t x)
{
    if (!obj) return;
    lv_style_set_x(&obj->style, x);
    lv_coord_t w = lv_area_get_width(&obj->coords);
    obj->coords.x1 = x;
    obj->coords.x2 = x + w - 1;
}

void lv_obj_set_y(lv_obj_t *obj, lv_coord_t y)
{
    if (!obj) return;
    lv_style_set_y(&obj->style, y);
    lv_coord_t h = lv_area_get_height(&obj->coords);
    obj->coords.y1 = y;
    obj->coords.y2 = y + h - 1;
}

void lv_obj_set_pos(lv_obj_t *obj, lv_coord_t x, lv_coord_t y)
{
    lv_obj_set_x(obj, x);
    lv_obj_set_y(obj, y);
}

void lv_obj_align(lv_obj_t *obj, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs)
{
    if (!obj || !obj->parent) return;
    lv_coord_t pw = lv_area_get_width(&obj->parent->coords);
    lv_coord_t ph = lv_area_get_height(&obj->parent->coords);
    lv_coord_t ow = lv_area_get_width(&obj->coords);
    lv_coord_t oh = lv_area_get_height(&obj->coords);
    lv_coord_t x = 0, y = 0;

    switch (align) {
    case LV_ALIGN_TOP_LEFT:      x = 0;           y = 0;           break;
    case LV_ALIGN_TOP_MID:       x = (pw - ow)/2; y = 0;           break;
    case LV_ALIGN_TOP_RIGHT:     x = pw - ow;     y = 0;           break;
    case LV_ALIGN_BOTTOM_LEFT:   x = 0;           y = ph - oh;     break;
    case LV_ALIGN_BOTTOM_MID:    x = (pw - ow)/2; y = ph - oh;     break;
    case LV_ALIGN_BOTTOM_RIGHT:  x = pw - ow;     y = ph - oh;     break;
    case LV_ALIGN_LEFT_MID:      x = 0;           y = (ph - oh)/2; break;
    case LV_ALIGN_RIGHT_MID:     x = pw - ow;     y = (ph - oh)/2; break;
    case LV_ALIGN_CENTER:        x = (pw - ow)/2; y = (ph - oh)/2; break;
    default:                     x = 0;           y = 0;           break;
    }
    lv_obj_set_pos(obj, x + x_ofs, y + y_ofs);
}

void lv_obj_center(lv_obj_t *obj)
{
    lv_obj_align(obj, LV_ALIGN_CENTER, 0, 0);
}

lv_coord_t lv_obj_get_width(const lv_obj_t *obj)
{
    return obj ? lv_area_get_width(&obj->coords) : 0;
}

lv_coord_t lv_obj_get_height(const lv_obj_t *obj)
{
    return obj ? lv_area_get_height(&obj->coords) : 0;
}

lv_coord_t lv_obj_get_x(const lv_obj_t *obj)
{
    return obj ? obj->coords.x1 : 0;
}

lv_coord_t lv_obj_get_y(const lv_obj_t *obj)
{
    return obj ? obj->coords.y1 : 0;
}

void lv_obj_add_flag(lv_obj_t *obj, uint32_t f)
{
    if (obj) obj->flags |= f;
}

void lv_obj_clear_flag(lv_obj_t *obj, uint32_t f)
{
    if (obj) obj->flags &= ~f;
}

bool lv_obj_has_flag(const lv_obj_t *obj, uint32_t f)
{
    return obj ? (obj->flags & f) != 0 : false;
}

/* Style setters */
void lv_obj_set_style_bg_color(lv_obj_t *obj, lv_color_t color, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_style_set_bg_color(&obj->style, color);
}

void lv_obj_set_style_text_color(lv_obj_t *obj, lv_color_t color, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_style_set_text_color(&obj->style, color);
}

void lv_obj_set_style_text_font(lv_obj_t *obj, const lv_font_t *font, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_style_set_text_font(&obj->style, font);
}

void lv_obj_set_style_pad_all(lv_obj_t *obj, lv_coord_t pad, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_style_set_pad_all(&obj->style, pad);
}

void lv_obj_set_style_pad_top(lv_obj_t *obj, lv_coord_t pad, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_style_set_pad_top(&obj->style, pad);
}

void lv_obj_set_style_pad_bottom(lv_obj_t *obj, lv_coord_t pad, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_style_set_pad_bottom(&obj->style, pad);
}

void lv_obj_set_style_pad_left(lv_obj_t *obj, lv_coord_t pad, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_style_set_pad_left(&obj->style, pad);
}

void lv_obj_set_style_pad_right(lv_obj_t *obj, lv_coord_t pad, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_style_set_pad_right(&obj->style, pad);
}

void lv_obj_set_style_border_width(lv_obj_t *obj, lv_coord_t width, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_style_set_border_width(&obj->style, width);
}

void lv_obj_set_style_border_color(lv_obj_t *obj, lv_color_t color, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_style_set_border_color(&obj->style, color);
}

void lv_obj_set_style_radius(lv_obj_t *obj, lv_coord_t radius, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_style_set_radius(&obj->style, radius);
}

void lv_obj_set_style_width(lv_obj_t *obj, lv_coord_t width, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_obj_set_width(obj, width);
}

void lv_obj_set_style_height(lv_obj_t *obj, lv_coord_t height, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_obj_set_height(obj, height);
}

void lv_obj_set_style_x(lv_obj_t *obj, lv_coord_t x, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_obj_set_x(obj, x);
}

void lv_obj_set_style_y(lv_obj_t *obj, lv_coord_t y, lv_style_selector_t sel)
{
    (void)sel;
    if (obj) lv_obj_set_y(obj, y);
}

void lv_obj_invalidate(lv_obj_t *obj)
{
    if (obj) obj->invalid = 1;
}

void lv_obj_invalidate_area(lv_obj_t *obj, const lv_area_t *area)
{
    (void)area;
    if (obj) obj->invalid = 1;
}

static void apply_layout(lv_obj_t *obj)
{
    lv_coord_t x = style_num(obj, LV_STYLE_PROP_X, obj->coords.x1);
    lv_coord_t y = style_num(obj, LV_STYLE_PROP_Y, obj->coords.y1);
    lv_coord_t w = style_num(obj, LV_STYLE_PROP_WIDTH, lv_area_get_width(&obj->coords));
    lv_coord_t h = style_num(obj, LV_STYLE_PROP_HEIGHT, lv_area_get_height(&obj->coords));
    lv_area_set(&obj->coords, x, y, x + w - 1, y + h - 1);

    lv_coord_t pad = style_num(obj, LV_STYLE_PROP_PAD_LEFT, 0);
    obj->content_area.x1 = obj->coords.x1 + pad;
    obj->content_area.y1 = obj->coords.y1 + style_num(obj, LV_STYLE_PROP_PAD_TOP, 0);
    obj->content_area.x2 = obj->coords.x2 - style_num(obj, LV_STYLE_PROP_PAD_RIGHT, 0);
    obj->content_area.y2 = obj->coords.y2 - style_num(obj, LV_STYLE_PROP_PAD_BOTTOM, 0);
}

static void draw_obj(lv_obj_t *obj, const lv_area_t *clip, uint8_t *buf,
                     const lv_area_t *buf_area, uint32_t stride)
{
    apply_layout(obj);

    lv_draw_rect_dsc_t rd;
    lv_draw_rect_dsc_init(&rd);
    rd.bg_color = style_color(obj, LV_STYLE_PROP_BG_COLOR, LV_COLOR_WHITE);
    rd.border_color = style_color(obj, LV_STYLE_PROP_BORDER_COLOR, LV_COLOR_BLACK);
    rd.border_width = style_num(obj, LV_STYLE_PROP_BORDER_WIDTH, 0);
    rd.radius = style_num(obj, LV_STYLE_PROP_RADIUS, 0);
    lv_draw_rect(&obj->coords, clip, &rd, buf, buf_area, stride);

    if (obj->type == 1) {
        /* label */
        const lv_label_ext_t *ext = (const lv_label_ext_t *)obj->ext;
        if (ext && ext->text[0]) {
            lv_draw_label_dsc_t ld;
            lv_draw_label_dsc_init(&ld);
            ld.color = style_color(obj, LV_STYLE_PROP_TEXT_COLOR, LV_COLOR_BLACK);
            ld.font = style_font(obj);
            lv_draw_label(&obj->content_area, clip, &ld, ext->text, buf, buf_area, stride);
        }
    } else if (obj->type == 2) {
        /* textarea */
        const lv_textarea_ext_t *ext = (const lv_textarea_ext_t *)obj->ext;
        if (ext && ext->text[0]) {
            lv_draw_label_dsc_t ld;
            lv_draw_label_dsc_init(&ld);
            ld.color = style_color(obj, LV_STYLE_PROP_TEXT_COLOR, LV_COLOR_BLACK);
            ld.font = style_font(obj);
            lv_draw_label(&obj->content_area, clip, &ld, ext->text, buf, buf_area, stride);
        }
    }
}

static void refresh_recursive(lv_obj_t *obj, const lv_area_t *clip,
                              uint8_t *buf, const lv_area_t *buf_area,
                              uint32_t stride)
{
    if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN))
        return;

    lv_area_t child_clip;
    if (!lv_area_intersect(&child_clip, clip, &obj->coords))
        return;

    draw_obj(obj, &child_clip, buf, buf_area, stride);

    for (uint8_t i = 0; i < obj->child_cnt; i++) {
        refresh_recursive(obj->children[i], &child_clip, buf, buf_area, stride);
    }
}

void lv_obj_refresh(lv_obj_t *obj, const lv_area_t *area, lv_layer_t *layer)
{
    if (!obj || !layer || !layer->buf) return;

    lv_area_t full;
    lv_area_set(&full, 0, 0, layer->buf_area.x2, layer->buf_area.y2);
    lv_area_t clip;
    if (!lv_area_intersect(&clip, area, &full))
        return;

    uint32_t stride = (uint32_t)lv_area_get_width(&layer->buf_area);
    refresh_recursive(obj, &clip, (uint8_t *)layer->buf, &layer->buf_area, stride);
}
