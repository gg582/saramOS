#ifndef HAL_STM32F769I_DISC1_H
#define HAL_STM32F769I_DISC1_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- RCC --- */
#define RCC_BASE        0x40023800U
#define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x00U))
#define RCC_PLLCFGR     (*(volatile uint32_t *)(RCC_BASE + 0x04U))
#define RCC_CFGR        (*(volatile uint32_t *)(RCC_BASE + 0x08U))
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30U))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x44U))

#define RCC_AHB1ENR_GPIOAEN     (1U << 0)
#define RCC_APB2ENR_USART1EN    (1U << 4)

/* --- FLASH --- */
#define FLASH_BASE_REG  0x40023C00U
#define FLASH_ACR       (*(volatile uint32_t *)(FLASH_BASE_REG + 0x00U))

/* --- GPIOA --- */
#define GPIOA_BASE      0x40020000U
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00U))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x08U))
#define GPIOA_PUPDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x0CU))
#define GPIOA_AFRH      (*(volatile uint32_t *)(GPIOA_BASE + 0x24U))

/* --- USART1 --- */
#define USART1_BASE     0x40011000U
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x00U))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x0CU))
#define USART1_ISR      (*(volatile uint32_t *)(USART1_BASE + 0x1CU))
#define USART1_TDR      (*(volatile uint32_t *)(USART1_BASE + 0x28U))
#define USART1_RDR      (*(volatile uint32_t *)(USART1_BASE + 0x24U))

#define USART_ISR_TXE   (1U << 7)
#define USART_ISR_TC    (1U << 6)
#define USART_ISR_RXNE  (1U << 5)
#define USART_CR1_TE    (1U << 3)
#define USART_CR1_RE    (1U << 2)
#define USART_CR1_UE    (1U << 0)

/* UART API */
void hal_uart_init(void);
void hal_uart_putc(char c);
void hal_uart_puts(const char *s);
char hal_uart_getc(void);
int  hal_uart_try_getc(void);

/* System init */
void hal_system_init(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_STM32F769I_DISC1_H */
