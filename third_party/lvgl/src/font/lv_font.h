/*
 * Minimal LVGL v9 font definitions.
 */
#ifndef LV_FONT_H
#define LV_FONT_H

#include "../misc/lv_types.h"
#include "../misc/lv_area.h"
#include "../misc/lv_color.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _lv_font_t {
    uint16_t line_height;
    uint16_t base_line;
    int16_t dsc;
    const uint8_t *glyph_bitmap;
    uint8_t (*get_glyph_dsc)(const struct _lv_font_t *, uint32_t letter, int16_t *ofx, int16_t *ofy, uint8_t *w, uint8_t *h);
} lv_font_t;

extern const lv_font_t lv_font_unscii_8;

#ifndef LV_FONT_DEFAULT
#define LV_FONT_DEFAULT &lv_font_unscii_8
#endif

/* Minimal glyph access */
int16_t lv_font_get_glyph_width(const lv_font_t *font, uint32_t letter);
int16_t lv_font_get_glyph_height(const lv_font_t *font);

#ifdef __cplusplus
}
#endif

#endif /* LV_FONT_H */
