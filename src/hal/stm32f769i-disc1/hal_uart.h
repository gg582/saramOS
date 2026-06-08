#ifndef HAL_UART_H
#define HAL_UART_H

#include <hal/stm32f769i-disc1.h>

#ifdef __cplusplus
extern "C" {
#endif

void hal_uart_init(void);
void hal_uart_putc(char c);
void hal_uart_puts(const char *s);
char hal_uart_getc(void);
int  hal_uart_try_getc(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_UART_H */
