#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <hal/board.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Convenience wrappers around the register definitions in stm32f769i-disc1.h */

void hal_gpio_init_af(uint32_t port_base, uint8_t pin, uint8_t af, uint8_t speed, uint8_t pupd);
void hal_gpio_init_output(uint32_t port_base, uint8_t pin, uint8_t speed);
void hal_gpio_init_input(uint32_t port_base, uint8_t pin, uint8_t pupd);
void hal_gpio_init_pullup(uint32_t port_base, uint8_t pin);
void hal_gpio_init_pulldown(uint32_t port_base, uint8_t pin);
void hal_gpio_write(uint32_t port_base, uint8_t pin, uint8_t val);
int  hal_gpio_read(uint32_t port_base, uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_H */
