/*
 * Display port header for the graphical-shell app.
 *
 * The on-screen console is rendered through the vendored, minimal LVGL v9
 * API-compatible implementation in third_party/lvgl (a single full-screen
 * lv_label object), configured in LV_DISPLAY_RENDER_MODE_DIRECT so LVGL
 * renders straight into the real SDRAM framebuffer the LTDC scans out.
 */
#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize SDRAM, display controller, and the on-screen console. */
void lvgl_port_init(void);

/* Call from the application main loop.  Returns recommended sleep ms. */
uint32_t lvgl_port_handler(void);

/* Provide a tick increment (called from systick or app loop). */
void lvgl_port_tick(uint32_t ms);

/* Print one character to the on-screen console.  Thread-safe enough for the
 * cooperative scheduler: the caller may be any process, and the routine
 * disables interrupts around cursor/scroll updates.
 */
void gfx_console_putc(char c);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_PORT_H */
