#ifndef HAL_TOUCH_H
#define HAL_TOUCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FT6206 capacitive touch controller on I2C1 (PB8=SCL, PB9=SDA),
 * shared with the DSI panel's touch overlay on STM32F769I-DISCO.
 * Polling-only (no EXTI/INT pin wiring) -- matches this board's other
 * HAL drivers (ETH, SDMMC): call hal_touch_read() as often as needed
 * (e.g. once per app main-loop iteration) instead of waiting on an
 * interrupt.
 */

/* Bring up I2C1 and probe the touch controller. Safe to call more than
 * once (no-ops after the first successful init). */
void hal_touch_init(void);

/* Poll for a current touch. Returns 1 and fills x and y (panel pixel
 * coordinates, 0..DISPLAY_WIDTH-1 / 0..DISPLAY_HEIGHT-1) if a finger is
 * down right now, 0 if not (or if the controller isn't responding).
 * Does not track press/release edges itself -- callers wanting
 * "just pressed" semantics should track the previous return value. */
int hal_touch_read(int16_t *x, int16_t *y);

#ifdef __cplusplus
}
#endif

#endif /* HAL_TOUCH_H */
