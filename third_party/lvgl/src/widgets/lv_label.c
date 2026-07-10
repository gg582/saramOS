/*
 * Minimal LVGL v9 label widget implementation.
 */
#include "lv_label.h"
#include <string.h>
#include <stdlib.h>

lv_obj_t *lv_label_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    if (!obj) return NULL;

    lv_label_ext_t *ext = malloc(sizeof(*ext));
    if (!ext) {
        lv_obj_delete(obj);
        return NULL;
    }
    memset(ext, 0, sizeof(*ext));
    obj->type = 1;
    obj->ext = ext;
    return obj;
}

void lv_label_set_text(lv_obj_t *lbl, const char *txt)
{
    if (!lbl || lbl->type != 1) return;
    lv_label_ext_t *ext = (lv_label_ext_t *)lbl->ext;
    if (!ext) return;
    if (!txt) txt = "";
    size_t len = strlen(txt);
    if (len >= LV_LABEL_TEXT_LEN_MAX) len = LV_LABEL_TEXT_LEN_MAX - 1;
    memcpy(ext->text, txt, len);
    ext->text[len] = '\0';
    lv_obj_invalidate(lbl);
}

void lv_label_set_text_static(lv_obj_t *lbl, const char *txt)
{
    lv_label_set_text(lbl, txt);
}

const char *lv_label_get_text(const lv_obj_t *lbl)
{
    if (!lbl || lbl->type != 1) return "";
    const lv_label_ext_t *ext = (const lv_label_ext_t *)lbl->ext;
    return ext ? ext->text : "";
}
