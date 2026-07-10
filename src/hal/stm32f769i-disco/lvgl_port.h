/*
 * Display port header for the graphical-shell app.
 *
 * The bundled "LVGL" is only a stub without a real renderer, so the graphical
 * shell draws directly into the SDRAM framebuffer and lets the LTDC scan it out.
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
