#include "hal_uart.h"

void hal_uart_init(void)
{
    /* Enable GPIOA and USART1 clocks */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC_APB2ENR |= RCC_APB2ENR_USART1EN;

    /* PA9 (TX) and PA10 (RX) to Alternate Function */
    GPIOA_MODER &= ~((3U << (9 * 2)) | (3U << (10 * 2)));
    GPIOA_MODER |= ((2U << (9 * 2)) | (2U << (10 * 2)));

    /* High speed */
    GPIOA_OSPEEDR |= ((3U << (9 * 2)) | (3U << (10 * 2)));

    /* AF7 for PA9 and PA10 */
    GPIOA_AFRH &= ~((0xFU << ((9 - 8) * 4)) | (0xFU << ((10 - 8) * 4)));
    GPIOA_AFRH |= ((7U << ((9 - 8) * 4)) | (7U << ((10 - 8) * 4)));

    /* Baudrate 115200 @ 16 MHz (HSI) -> 16000000/115200 = 139 */
    USART1_BRR = 139;

    /* Enable TX, RX, USART */
    USART1_CR1 |= (USART_CR1_TE | USART_CR1_RE | USART_CR1_UE);
}

void hal_uart_putc(char c)
{
    while (!(USART1_ISR & USART_ISR_TXE))
        ;
    USART1_TDR = (uint8_t)c;
}

void hal_uart_puts(const char *s)
{
    while (*s) {
        hal_uart_putc(*s++);
    }
}

char hal_uart_getc(void)
{
    while (!(USART1_ISR & USART_ISR_RXNE))
        ;
    return (char)(USART1_RDR & 0xFFU);
}

int hal_uart_try_getc(void)
{
    if (USART1_ISR & USART_ISR_RXNE) {
        return (int)(USART1_RDR & 0xFFU);
    }
    return -1;
}
