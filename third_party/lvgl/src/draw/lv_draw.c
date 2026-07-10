/*
 * Minimal LVGL v9 software draw implementation (RGB565).
 */
#include "lv_draw.h"
#include <string.h>

void lv_draw_rect_dsc_init(lv_draw_rect_dsc_t *dsc)
{
    memset(dsc, 0, sizeof(*dsc));
    dsc->bg_opa = LV_OPA_COVER;
    dsc->border_opa = LV_OPA_COVER;
}

void lv_draw_label_dsc_init(lv_draw_label_dsc_t *dsc)
{
    memset(dsc, 0, sizeof(*dsc));
    dsc->opa = LV_OPA_COVER;
}

static inline void put_pixel(uint8_t *buf, const lv_area_t *buf_area,
                             uint32_t stride, lv_coord_t x, lv_coord_t y,
                             lv_color_t color)
{
    if (x < buf_area->x1 || x > buf_area->x2 || y < buf_area->y1 || y > buf_area->y2)
        return;
    uint32_t px = (uint32_t)(y - buf_area->y1) * stride + (uint32_t)(x - buf_area->x1);
    ((lv_color_t *)buf)[px] = color;
}

void lv_draw_rect(const lv_area_t *coords, const lv_area_t *clip,
                  const lv_draw_rect_dsc_t *dsc,
                  uint8_t *buf, const lv_area_t *buf_area, uint32_t stride)
{
    lv_area_t draw_area;
    if (!lv_area_intersect(&draw_area, coords, clip))
        return;

    for (lv_coord_t y = draw_area.y1; y <= draw_area.y2; y++) {
        for (lv_coord_t x = draw_area.x1; x <= draw_area.x2; x++) {
            put_pixel(buf, buf_area, stride, x, y, dsc->bg_color);
        }
    }

    if (dsc->border_width > 0) {
        lv_area_t border = draw_area;
        for (lv_coord_t y = border.y1; y <= border.y2 && y <= border.y1 + dsc->border_width - 1; y++) {
            for (lv_coord_t x = border.x1; x <= border.x2; x++)
                put_pixel(buf, buf_area, stride, x, y, dsc->border_color);
        }
        for (lv_coord_t y = border.y2; y >= border.y1 && y >= border.y2 - dsc->border_width + 1; y--) {
            for (lv_coord_t x = border.x1; x <= border.x2; x++)
                put_pixel(buf, buf_area, stride, x, y, dsc->border_color);
        }
        for (lv_coord_t y = border.y1; y <= border.y2; y++) {
            for (lv_coord_t x = border.x1; x <= border.x1 + dsc->border_width - 1 && x <= border.x2; x++)
                put_pixel(buf, buf_area, stride, x, y, dsc->border_color);
            for (lv_coord_t x = border.x2; x >= border.x1 && x >= border.x2 - dsc->border_width + 1; x--)
                put_pixel(buf, buf_area, stride, x, y, dsc->border_color);
        }
    }
}

void lv_draw_label(const lv_area_t *coords, const lv_area_t *clip,
                   const lv_draw_label_dsc_t *dsc,
                   const char *txt,
                   uint8_t *buf, const lv_area_t *buf_area, uint32_t stride)
{
    if (!txt || !dsc->font) return;

    lv_area_t draw_area;
    if (!lv_area_intersect(&draw_area, coords, clip))
        return;

    lv_coord_t cx = coords->x1;
    lv_coord_t cy = coords->y1;

    while (*txt) {
        uint32_t c = (uint8_t)*txt++;
        if (c == '\r') continue;
        if (c == '\n') {
            cx = coords->x1;
            cy += dsc->font->line_height;
            continue;
        }

        if (c < 32 || c >= 127) {
            cx += 8;
            continue;
        }

        int16_t ofx, ofy;
        uint8_t gw, gh;
        if (!dsc->font->get_glyph_dsc(dsc->font, c, &ofx, &ofy, &gw, &gh))
            continue;

        uint32_t idx = ((uint32_t)c - 32U) * gh;
        for (uint8_t row = 0; row < gh; row++) {
            uint8_t bits = dsc->font->glyph_bitmap[idx + row];
            for (uint8_t col = 0; col < gw; col++) {
                if (bits & (0x80U >> col)) {
                    put_pixel(buf, buf_area, stride,
                              cx + col + ofx, cy + row + ofy,
                              dsc->color);
                }
            }
        }
        cx += gw;
    }
}
