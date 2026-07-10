/*
 * Minimal LVGL v9 textarea widget implementation.
 */
#include "lv_textarea.h"
#include <string.h>
#include <stdlib.h>

lv_obj_t *lv_textarea_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    if (!obj) return NULL;

    lv_textarea_ext_t *ext = malloc(sizeof(*ext));
    if (!ext) {
        lv_obj_delete(obj);
        return NULL;
    }
    memset(ext, 0, sizeof(*ext));
    obj->type = 2;
    obj->ext = ext;
    return obj;
}

void lv_textarea_set_text(lv_obj_t *ta, const char *txt)
{
    if (!ta || ta->type != 2) return;
    lv_textarea_ext_t *ext = (lv_textarea_ext_t *)ta->ext;
    if (!ext) return;
    if (!txt) txt = "";
    size_t len = strlen(txt);
    if (len >= LV_TEXTAREA_TEXT_LEN_MAX) len = LV_TEXTAREA_TEXT_LEN_MAX - 1;
    memcpy(ext->text, txt, len);
    ext->text[len] = '\0';
    ext->len = (uint16_t)len;
    ext->cursor = ext->len;
    lv_obj_invalidate(ta);
}

void lv_textarea_add_text(lv_obj_t *ta, const char *txt)
{
    if (!ta || ta->type != 2 || !txt) return;
    lv_textarea_ext_t *ext = (lv_textarea_ext_t *)ta->ext;
    if (!ext) return;
    while (*txt) {
        if (ext->len >= LV_TEXTAREA_TEXT_LEN_MAX - 1)
            break;
        /* Shift tail to make room at cursor. */
        for (uint16_t i = ext->len; i > ext->cursor; i--) {
            ext->text[i] = ext->text[i - 1];
        }
        ext->text[ext->cursor++] = *txt++;
        ext->len++;
    }
    ext->text[ext->len] = '\0';
    lv_obj_invalidate(ta);
}

void lv_textarea_add_char(lv_obj_t *ta, uint32_t c)
{
    char buf[2] = {(char)c, '\0'};
    lv_textarea_add_text(ta, buf);
}

void lv_textarea_set_one_line(lv_obj_t *ta, bool en)
{
    if (!ta || ta->type != 2) return;
    lv_textarea_ext_t *ext = (lv_textarea_ext_t *)ta->ext;
    if (ext) ext->one_line = en ? 1 : 0;
}

const char *lv_textarea_get_text(const lv_obj_t *ta)
{
    if (!ta || ta->type != 2) return "";
    const lv_textarea_ext_t *ext = (const lv_textarea_ext_t *)ta->ext;
    return ext ? ext->text : "";
}

void lv_textarea_set_cursor_pos(lv_obj_t *ta, int32_t pos)
{
    if (!ta || ta->type != 2) return;
    lv_textarea_ext_t *ext = (lv_textarea_ext_t *)ta->ext;
    if (!ext) return;
    if (pos < 0) pos = 0;
    if (pos > (int32_t)ext->len) pos = ext->len;
    ext->cursor = (uint16_t)pos;
}
