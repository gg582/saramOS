/*
 * Minimal LVGL v9 core runtime.
 */
#include "../lv_conf.h"
#include "../lvgl.h"
#include <string.h>

static uint32_t g_tick = 0;

void lv_init(void)
{
    g_tick = 0;
    lv_display_create(LV_HOR_RES_MAX, LV_VER_RES_MAX);
}

void lv_deinit(void)
{
}

void lv_tick_inc(uint32_t tick_period)
{
    g_tick += tick_period;
}

uint32_t lv_tick_get(void)
{
    return g_tick;
}

uint32_t lv_timer_handler(void)
{
    lv_display_t *disp = lv_display_get_default();
    if (!disp || !disp->flush_cb || !disp->draw_buf1)
        return LV_DEF_REFR_PERIOD;

    lv_layer_t *layer = lv_display_get_layer_bottom(disp);
    if (!layer->buf) return LV_DEF_REFR_PERIOD;

    lv_area_t full;
    lv_area_set(&full, 0, 0, disp->hor_res - 1, disp->ver_res - 1);

    /* For minimal implementation, refresh the whole screen each call. */
    memset(layer->buf, 0, layer->buf_size);
    lv_obj_refresh(disp->screen, &full, layer);

    disp->flush_cb(disp, &full, (uint8_t *)layer->buf);
    return LV_DEF_REFR_PERIOD;
}
