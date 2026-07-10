/*
 * Minimal LVGL v9 draw definitions.
 */
#ifndef LV_DRAW_H
#define LV_DRAW_H

#include "../misc/lv_types.h"
#include "../misc/lv_area.h"
#include "../misc/lv_color.h"
#include "../misc/lv_style.h"
#include "../font/lv_font.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_color_t bg_color;
    lv_color_t border_color;
    lv_coord_t border_width;
    lv_coord_t radius;
    lv_opa_t bg_opa;
    lv_opa_t border_opa;
} lv_draw_rect_dsc_t;

typedef struct {
    lv_color_t color;
    const lv_font_t *font;
    lv_opa_t opa;
} lv_draw_label_dsc_t;

void lv_draw_rect_dsc_init(lv_draw_rect_dsc_t *dsc);
void lv_draw_label_dsc_init(lv_draw_label_dsc_t *dsc);

/* Draw into a partial RGB565 buffer. */
void lv_draw_rect(const lv_area_t *coords, const lv_area_t *clip,
                  const lv_draw_rect_dsc_t *dsc,
                  uint8_t *buf, const lv_area_t *buf_area, uint32_t stride);

void lv_draw_label(const lv_area_t *coords, const lv_area_t *clip,
                   const lv_draw_label_dsc_t *dsc,
                   const char *txt,
                   uint8_t *buf, const lv_area_t *buf_area, uint32_t stride);

#ifdef __cplusplus
}
#endif

#endif /* LV_DRAW_H */
