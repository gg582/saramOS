/*
 * Minimal LVGL v9 label widget.
 */
#ifndef LV_LABEL_H
#define LV_LABEL_H

#include "../core/lv_obj.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LV_LABEL_TEXT_LEN_MAX       256

typedef struct {
    char text[LV_LABEL_TEXT_LEN_MAX];
} lv_label_ext_t;

lv_obj_t *lv_label_create(lv_obj_t *parent);
void lv_label_set_text(lv_obj_t *lbl, const char *txt);
void lv_label_set_text_static(lv_obj_t *lbl, const char *txt);
const char *lv_label_get_text(const lv_obj_t *lbl);

#ifdef __cplusplus
}
#endif

#endif /* LV_LABEL_H */
