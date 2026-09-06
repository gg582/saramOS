/*
 * HAL display initialization for STM32F769I-DISCO.
 *
 * The on-board MIPI DSI LCD is an OTM8009A panel mounted in landscape mode:
 *   - Active area: 800 x 480 pixels
 *   - Pixel format used by the LVGL port: RGB565
 *   - Framebuffer lives in the external SDRAM at 0xC0000000
 */
#ifndef HAL_DISPLAY_H
#define HAL_DISPLAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_WIDTH       800U
#define DISPLAY_HEIGHT      480U
#define DISPLAY_BPP         2U   /* RGB565 */
#define DISPLAY_FB_SIZE     ((uint32_t)DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BPP)

/* Backlight GPIO: PI14 on STM32F769I-DISCO */
#define DISP_BACKLIGHT_PORT GPIOI_BASE
#define DISP_BACKLIGHT_PIN  14U

/* LCD reset GPIO: PJ15 */
#define DISP_RESET_PORT     GPIOJ_BASE
#define DISP_RESET_PIN      15U

/* Initialize DSI + LTDC config + OTM8009A (panel awake and initialized)
 * but do NOT start Video Mode / LTDC scan-out / backlight yet. Callers
 * must write their desired first-frame content to the framebuffer
 * (hal_display_fb_addr()) and then call hal_display_start_video() --
 * see that function's comment for why this is split in two: writing the
 * framebuffer only after video output has already started produces an
 * unsynchronized tearing race (confirmed on hardware). */
void hal_display_init(void);

/* Start LTDC scan-out + DSI Video Mode + backlight. Call once, after
 * hal_display_init() and after the desired first frame has already been
 * written to the framebuffer. */
void hal_display_start_video(void);

/* Return the configured framebuffer address (in SDRAM). */
uint32_t hal_display_fb_addr(void);

/* Turn backlight on/off. */
void hal_display_backlight_on(void);
void hal_display_backlight_off(void);

/* Read the OTM8009A's MIPI DCS "Display Power Mode" register (0x0A) over
 * the live DSI link. Returns 0 and fills *out on success, -1 on timeout
 * (e.g. if the link has genuinely gone unresponsive). Callable any time
 * after hal_display_init() -- used to check whether the panel's own
 * self-reported state (Display On/Off, Sleep In/Out, ...) changes after
 * the point where the picture on screen is observed to fade. */
int hal_display_read_power_mode(uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HAL_DISPLAY_H */
