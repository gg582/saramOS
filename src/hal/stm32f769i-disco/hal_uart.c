#include "hal_uart.h"

/* Weak hook so the graphical shell can mirror UART output. */
__attribute__((weak)) void hal_uart_output_hook(char c)
{
    (void)c;
}

void hal_uart_init(void)
{
    /* Enable GPIOA and USART1 clocks */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC_APB2ENR |= RCC_APB2ENR_USART1EN;

    USART1_CR1 &= ~USART_CR1_UE;

    /* PA9 (TX) and PA10 (RX) to Alternate Function */
    GPIOA_MODER &= ~((3U << (9 * 2)) | (3U << (10 * 2)));
    GPIOA_MODER |= ((2U << (9 * 2)) | (2U << (10 * 2)));

    /* Push-pull TX/RX pins, RX pulled up for the ST-LINK VCP idle state. */
    GPIOA_OTYPER &= ~((1U << 9) | (1U << 10));
    GPIOA_PUPDR &= ~((3U << (9 * 2)) | (3U << (10 * 2)));
    GPIOA_PUPDR |= (1U << (10 * 2));

    /* High speed */
    GPIOA_OSPEEDR |= ((3U << (9 * 2)) | (3U << (10 * 2)));

    /* AF7 for PA9 and PA10 */
    GPIOA_AFRH &= ~((0xFU << ((9 - 8) * 4)) | (0xFU << ((10 - 8) * 4)));
    GPIOA_AFRH |= ((7U << ((9 - 8) * 4)) | (7U << ((10 - 8) * 4)));

    /* Baudrate 115200 @ 84 MHz (APB2 = HCLK/2 = 168/2) -> 84000000/115200 = 729 */
    USART1_BRR = 729;

    /* Disable overrun detection so ORE never blocks new incoming bytes. */
    USART1_CR3 |= USART_CR3_OVRDIS;
    USART1_ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF;

    /* Enable TX, RX, USART */
    USART1_CR1 |= (USART_CR1_TE | USART_CR1_RE | USART_CR1_UE);
}

void hal_uart_putc(char c)
{
    hal_uart_output_hook(c);
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

/* Clear any error flags (ORE/FE/NE) that can block RXNE on STM32F7. */
static inline void uart_clear_errors(void)
{
    if (USART1_ISR & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) {
        USART1_ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF;
    }
}

int hal_uart_readable(void)
{
    uart_clear_errors();
    return (USART1_ISR & USART_ISR_RXNE) ? 1 : 0;
}

int hal_uart_try_getc(void)
{
    uart_clear_errors();
    if (USART1_ISR & USART_ISR_RXNE) {
        return (int)(USART1_RDR & 0xFFU);
    }
    return -1;
}
