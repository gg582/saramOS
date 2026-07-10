/*
 * Minimal LVGL v9 public header for saramOS graphical shell.
 */
#ifndef LVGL_H
#define LVGL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "src/misc/lv_types.h"
#include "src/misc/lv_color.h"
#include "src/misc/lv_area.h"
#include "src/misc/lv_style.h"
#include "src/misc/lv_timer.h"
#include "src/font/lv_font.h"
#include "src/core/lv_obj.h"
#include "src/core/lv_display.h"
#include "src/widgets/lv_textarea.h"
#include "src/widgets/lv_label.h"

/* Core init / handler */
void lv_init(void);
void lv_deinit(void);
void lv_tick_inc(uint32_t tick_period);
uint32_t lv_timer_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_H */
