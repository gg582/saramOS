/*
 * Minimal LVGL v9 object definitions.
 */
#ifndef LV_OBJ_H
#define LV_OBJ_H

#include "../misc/lv_types.h"
#include "../misc/lv_area.h"
#include "../misc/lv_color.h"
#include "../misc/lv_style.h"
#include "../font/lv_font.h"
#include "lv_display.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LV_OBJ_FLAG_HIDDEN          (1U << 0)

#define LV_ALIGN_DEFAULT            0
#define LV_ALIGN_TOP_LEFT           1
#define LV_ALIGN_TOP_MID            2
#define LV_ALIGN_TOP_RIGHT          3
#define LV_ALIGN_BOTTOM_LEFT        4
#define LV_ALIGN_BOTTOM_MID         5
#define LV_ALIGN_BOTTOM_RIGHT       6
#define LV_ALIGN_LEFT_MID           7
#define LV_ALIGN_RIGHT_MID          8
#define LV_ALIGN_CENTER             9

typedef uint8_t lv_align_t;

#define LV_OBJ_CHILD_NUM_MAX        8

typedef struct _lv_obj_t {
    lv_area_t coords;
    lv_area_t content_area;
    lv_style_t style;
    struct _lv_obj_t *parent;
    struct _lv_obj_t *children[LV_OBJ_CHILD_NUM_MAX];
    uint8_t child_cnt;
    uint32_t flags;
    uint8_t invalid : 1;
    uint8_t type;       /* 0=base, 1=label, 2=textarea */
    void *ext;          /* widget-specific data */
} lv_obj_t;

/* Object lifecycle */
lv_obj_t *lv_obj_create(lv_obj_t *parent);
void lv_obj_delete(lv_obj_t *obj);

/* Object tree */
lv_obj_t *lv_screen_active(void);

/* Geometry */
void lv_obj_set_size(lv_obj_t *obj, lv_coord_t w, lv_coord_t h);
void lv_obj_set_width(lv_obj_t *obj, lv_coord_t w);
void lv_obj_set_height(lv_obj_t *obj, lv_coord_t h);
void lv_obj_set_x(lv_obj_t *obj, lv_coord_t x);
void lv_obj_set_y(lv_obj_t *obj, lv_coord_t y);
void lv_obj_set_pos(lv_obj_t *obj, lv_coord_t x, lv_coord_t y);
void lv_obj_align(lv_obj_t *obj, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs);
void lv_obj_center(lv_obj_t *obj);
lv_coord_t lv_obj_get_width(const lv_obj_t *obj);
lv_coord_t lv_obj_get_height(const lv_obj_t *obj);
lv_coord_t lv_obj_get_x(const lv_obj_t *obj);
lv_coord_t lv_obj_get_y(const lv_obj_t *obj);

/* Flags */
void lv_obj_add_flag(lv_obj_t *obj, uint32_t f);
void lv_obj_clear_flag(lv_obj_t *obj, uint32_t f);
bool lv_obj_has_flag(const lv_obj_t *obj, uint32_t f);

/* Styles */
void lv_obj_set_style_bg_color(lv_obj_t *obj, lv_color_t color, lv_style_selector_t sel);
void lv_obj_set_style_text_color(lv_obj_t *obj, lv_color_t color, lv_style_selector_t sel);
void lv_obj_set_style_text_font(lv_obj_t *obj, const lv_font_t *font, lv_style_selector_t sel);
void lv_obj_set_style_pad_all(lv_obj_t *obj, lv_coord_t pad, lv_style_selector_t sel);
void lv_obj_set_style_pad_top(lv_obj_t *obj, lv_coord_t pad, lv_style_selector_t sel);
void lv_obj_set_style_pad_bottom(lv_obj_t *obj, lv_coord_t pad, lv_style_selector_t sel);
void lv_obj_set_style_pad_left(lv_obj_t *obj, lv_coord_t pad, lv_style_selector_t sel);
void lv_obj_set_style_pad_right(lv_obj_t *obj, lv_coord_t pad, lv_style_selector_t sel);
void lv_obj_set_style_border_width(lv_obj_t *obj, lv_coord_t width, lv_style_selector_t sel);
void lv_obj_set_style_border_color(lv_obj_t *obj, lv_color_t color, lv_style_selector_t sel);
void lv_obj_set_style_radius(lv_obj_t *obj, lv_coord_t radius, lv_style_selector_t sel);
void lv_obj_set_style_width(lv_obj_t *obj, lv_coord_t width, lv_style_selector_t sel);
void lv_obj_set_style_height(lv_obj_t *obj, lv_coord_t height, lv_style_selector_t sel);
void lv_obj_set_style_x(lv_obj_t *obj, lv_coord_t x, lv_style_selector_t sel);
void lv_obj_set_style_y(lv_obj_t *obj, lv_coord_t y, lv_style_selector_t sel);

/* Invalidation */
void lv_obj_invalidate(lv_obj_t *obj);
void lv_obj_invalidate_area(lv_obj_t *obj, const lv_area_t *area);

/* Internal: refresh invalid objects into layer */
void lv_obj_refresh(lv_obj_t *obj, const lv_area_t *area, lv_layer_t *layer);

#ifdef __cplusplus
}
#endif

#endif /* LV_OBJ_H */
