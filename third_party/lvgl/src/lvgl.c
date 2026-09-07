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

    /* Double buffering: if a second buffer was actually provided (see
     * lv_display_set_buffers()), render into whichever of draw_buf1/
     * draw_buf2 was NOT rendered into last time, alternating every
     * call, instead of always rendering into draw_buf1. For a display
     * driven in LV_DISPLAY_RENDER_MODE_DIRECT straight into a live,
     * actively-scanned framebuffer (as this port's lvgl_port.c does),
     * draw_buf1 alone being the only-ever render target meant every
     * redraw wrote directly into the exact memory the LTDC was
     * continuously reading from -- no amount of confining the write to
     * the vertical blanking window helped, because the write itself
     * (walking every glyph pixel via put_pixel(), or any full-buffer
     * touch) commonly takes longer than one blanking window to finish,
     * so scan-out would resume mid-write regardless. Rendering into the
     * buffer that is NOT on screen, then flipping (see below), removes
     * the race entirely: the LTDC only ever reads a complete, finished
     * buffer. */
    if (disp->draw_buf2) {
        /* Select based on the CURRENT flag, then toggle for next time --
         * not the other way around -- so the very first call (flag
         * starts at 0 from lv_display_create()'s memset) renders into
         * draw_buf1, matching the address the display driver already
         * configured LTDC_LAYER1->CFBAR to before any of this ever
         * runs (see hal_display_init()/hal_display_start_video()). If
         * the first render went into draw_buf2 instead, the very first
         * frame on screen would be draw_buf1's stale/blank content
         * until the *second* redraw call caught up. */
        layer->buf = disp->render_buf2_active ? disp->draw_buf2 : disp->draw_buf1;
        disp->render_buf2_active = !disp->render_buf2_active;
    } else {
        layer->buf = disp->draw_buf1;
    }
    if (!layer->buf) return LV_DEF_REFR_PERIOD;

    lv_area_t full;
    lv_area_set(&full, 0, 0, disp->hor_res - 1, disp->ver_res - 1);

    /* No pre-clear here -- lv_obj_refresh() -> draw_obj() (lv_obj.c)
     * already unconditionally paints an opaque bg rect covering every
     * object's full coords before drawing its content, and every top-
     * level object refreshed from disp->screen ends up covering the
     * full screen this way (this port's console label is sized to
     * DISPLAY_WIDTH x DISPLAY_HEIGHT). A memset(0) here was therefore
     * fully redundant on top of being one more full-buffer touch. */
    lv_obj_refresh(disp->screen, &full, layer);

    /* flush_cb receives the buffer that was actually just rendered into
     * (layer->buf, set above) -- with double buffering this is the
     * finished, complete buffer the display driver should now switch
     * to displaying. */
    disp->flush_cb(disp, &full, (uint8_t *)layer->buf);
    return LV_DEF_REFR_PERIOD;
}
