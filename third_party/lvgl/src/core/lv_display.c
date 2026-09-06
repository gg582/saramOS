/*
 * Minimal LVGL v9 display implementation.
 */
#include "lv_display.h"
#include "lv_obj.h"
#include <string.h>

static lv_display_t g_disp;
static int g_disp_inited = 0;

lv_display_t *lv_display_create(int32_t hor_res, int32_t ver_res)
{
    if (g_disp_inited)
        return &g_disp;

    memset(&g_disp, 0, sizeof(g_disp));
    g_disp.hor_res = hor_res;
    g_disp.ver_res = ver_res;
    g_disp.physical_hor_res = hor_res;
    g_disp.physical_ver_res = ver_res;
    g_disp.render_mode = LV_DISPLAY_RENDER_MODE_PARTIAL;

    /* Create default screen */
    g_disp.screen = lv_obj_create(NULL);
    g_disp.screen->coords.x1 = 0;
    g_disp.screen->coords.y1 = 0;
    g_disp.screen->coords.x2 = hor_res - 1;
    g_disp.screen->coords.y2 = ver_res - 1;
    g_disp_inited = 1;
    return &g_disp;
}

void lv_display_delete(lv_display_t *disp)
{
    (void)disp;
}

void lv_display_set_resolution(lv_display_t *disp, int32_t hor_res, int32_t ver_res)
{
    if (!disp) return;
    disp->hor_res = hor_res;
    disp->ver_res = ver_res;
    if (disp->screen) {
        disp->screen->coords.x2 = hor_res - 1;
        disp->screen->coords.y2 = ver_res - 1;
    }
}

void lv_display_set_physical_resolution(lv_display_t *disp, int32_t hor_res, int32_t ver_res)
{
    if (!disp) return;
    disp->physical_hor_res = hor_res;
    disp->physical_ver_res = ver_res;
}

void lv_display_set_flush_cb(lv_display_t *disp, lv_display_flush_cb_t flush_cb)
{
    if (disp) disp->flush_cb = flush_cb;
}

void lv_display_set_buffers(lv_display_t *disp, void *buf1, void *buf2,
                            uint32_t buf_size, lv_display_render_mode_t render_mode)
{
    if (!disp) return;
    disp->draw_buf1 = buf1;
    disp->draw_buf2 = buf2;
    disp->draw_buf_size = buf_size;
    disp->render_mode = render_mode;
    disp->layer_bottom.buf = buf1;
    disp->layer_bottom.buf_size = buf_size;
    /* buf_area was never set anywhere, which left it zeroed by the
     * memset() in lv_display_create() -- i.e. a single pixel at (0,0).
     * lv_obj_refresh()'s "full" area and lv_draw.c's put_pixel() bounds
     * check both key off this field, so without it nothing outside that
     * one pixel was ever actually drawn. Set it to the physical
     * resolution passed to lv_display_create() (or later overridden via
     * lv_display_set_physical_resolution()). */
    lv_area_set(&disp->layer_bottom.buf_area, 0, 0,
                disp->physical_hor_res - 1, disp->physical_ver_res - 1);
}

lv_display_t *lv_display_get_default(void)
{
    return g_disp_inited ? &g_disp : NULL;
}

lv_layer_t *lv_display_get_layer_bottom(lv_display_t *disp)
{
    return disp ? &disp->layer_bottom : NULL;
}

int32_t lv_display_get_horizontal_resolution(lv_display_t *disp)
{
    return disp ? disp->hor_res : 0;
}

int32_t lv_display_get_vertical_resolution(lv_display_t *disp)
{
    return disp ? disp->ver_res : 0;
}

void lv_display_flush_ready(lv_display_t *disp)
{
    if (disp) disp->flushing = 0;
}
