/*
 * Minimal LVGL v9 display definitions.
 */
#ifndef LV_DISPLAY_H
#define LV_DISPLAY_H

#include "../misc/lv_types.h"
#include "../misc/lv_area.h"
#include "../misc/lv_color.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LV_DISPLAY_RENDER_MODE_PARTIAL  0
#define LV_DISPLAY_RENDER_MODE_DIRECT   1
#define LV_DISPLAY_RENDER_MODE_FULL     2

typedef uint8_t lv_display_render_mode_t;

typedef struct _lv_layer_t {
    lv_area_t buf_area;
    void *buf;
    uint32_t buf_size;
} lv_layer_t;

struct _lv_display_t;

typedef void (*lv_display_flush_cb_t)(struct _lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

typedef struct _lv_display_t {
    int32_t hor_res;
    int32_t ver_res;
    int32_t physical_hor_res;
    int32_t physical_ver_res;
    lv_area_t inv_areas[4];
    uint8_t inv_area_cnt;
    lv_display_flush_cb_t flush_cb;
    void *draw_buf1;
    void *draw_buf2;
    uint32_t draw_buf_size;
    lv_display_render_mode_t render_mode;
    uint32_t flushing : 1;
    uint32_t flushing_last : 1;
    lv_layer_t layer_bottom;
    struct _lv_obj_t *screen;
} lv_display_t;

lv_display_t *lv_display_create(int32_t hor_res, int32_t ver_res);
void lv_display_delete(lv_display_t *disp);
void lv_display_set_resolution(lv_display_t *disp, int32_t hor_res, int32_t ver_res);
void lv_display_set_physical_resolution(lv_display_t *disp, int32_t hor_res, int32_t ver_res);
void lv_display_set_flush_cb(lv_display_t *disp, lv_display_flush_cb_t flush_cb);
void lv_display_set_buffers(lv_display_t *disp, void *buf1, void *buf2, uint32_t buf_size, lv_display_render_mode_t render_mode);
lv_display_t *lv_display_get_default(void);
lv_layer_t *lv_display_get_layer_bottom(lv_display_t *disp);
int32_t lv_display_get_horizontal_resolution(lv_display_t *disp);
int32_t lv_display_get_vertical_resolution(lv_display_t *disp);

void lv_display_flush_ready(lv_display_t *disp);

#ifdef __cplusplus
}
#endif

#endif /* LV_DISPLAY_H */
