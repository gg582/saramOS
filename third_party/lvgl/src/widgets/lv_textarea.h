/*
 * Minimal LVGL v9 textarea widget.
 */
#ifndef LV_TEXTAREA_H
#define LV_TEXTAREA_H

#include "../core/lv_obj.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LV_TEXTAREA_TEXT_LEN_MAX    2048

typedef struct {
    char text[LV_TEXTAREA_TEXT_LEN_MAX];
    uint16_t cursor;
    uint16_t len;
    uint8_t one_line : 1;
} lv_textarea_ext_t;

lv_obj_t *lv_textarea_create(lv_obj_t *parent);
void lv_textarea_set_text(lv_obj_t *ta, const char *txt);
void lv_textarea_add_text(lv_obj_t *ta, const char *txt);
void lv_textarea_add_char(lv_obj_t *ta, uint32_t c);
void lv_textarea_set_one_line(lv_obj_t *ta, bool en);
const char *lv_textarea_get_text(const lv_obj_t *ta);
void lv_textarea_set_cursor_pos(lv_obj_t *ta, int32_t pos);

#ifdef __cplusplus
}
#endif

#endif /* LV_TEXTAREA_H */
