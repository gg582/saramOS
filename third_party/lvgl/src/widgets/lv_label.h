/*
 * Minimal LVGL v9 label widget.
 */
#ifndef LV_LABEL_H
#define LV_LABEL_H

#include "../core/lv_obj.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This is malloc'd per label (see lv_label_create()) against a bare-metal
 * heap (_sbrk() in syscalls.c) that has no out-of-memory check at all --
 * it just grows from _end with no bound, so an allocation that doesn't
 * fit silently corrupts whatever memory it lands on (typically another
 * process's stack) instead of failing cleanly. Kept modest for that
 * reason; console_flush() already stops appending further rows once this
 * fills rather than overflowing it, so a console taller than this fits
 * just degrades to showing fewer trailing rows, not a crash. */
#define LV_LABEL_TEXT_LEN_MAX       1024

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
