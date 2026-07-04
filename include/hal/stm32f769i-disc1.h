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
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x34U))
#define RCC_AHB2RSTR    (*(volatile uint32_t *)(RCC_BASE + 0x14U))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x44U))
#define RCC_APB2RSTR    (*(volatile uint32_t *)(RCC_BASE + 0x24U))
#define RCC_DCKCFGR2    (*(volatile uint32_t *)(RCC_BASE + 0x90U))

#define RCC_AHB1ENR_GPIOAEN     (1U << 0)
#define RCC_AHB1ENR_GPIOBEN     (1U << 1)
#define RCC_AHB1ENR_GPIOCEN     (1U << 2)
#define RCC_AHB1ENR_GPIODEN     (1U << 3)
#define RCC_AHB1ENR_GPIOEEN     (1U << 4)
#define RCC_AHB1ENR_GPIOFEN     (1U << 5)
#define RCC_AHB1ENR_GPIOGEN     (1U << 6)
#define RCC_AHB1ENR_GPIOHEN     (1U << 7)
#define RCC_AHB1ENR_GPIOIEN     (1U << 8)
#define RCC_AHB1ENR_GPIOJEN     (1U << 9)
#define RCC_AHB1ENR_GPIOKEN     (1U << 10)

#define RCC_AHB1ENR_ETHMACEN    (1U << 25)
#define RCC_AHB1ENR_ETHMACTXEN  (1U << 26)
#define RCC_AHB1ENR_ETHMACRXEN  (1U << 27)

#define RCC_APB2ENR_USART1EN    (1U << 4)
#define RCC_APB2ENR_SDMMC1EN    (1U << 11)
#define RCC_APB2ENR_SYSCFGEN    (1U << 14)

#define RCC_APB2RSTR_SDMMC1RST  (1U << 11)

#define RCC_DCKCFGR2_SDMMC1SEL  (1U << 28)

/* --- SYSCFG --- */
#define SYSCFG_BASE     0x40013800U
#define SYSCFG_PMC      (*(volatile uint32_t *)(SYSCFG_BASE + 0x04U))
#define SYSCFG_PMC_MII_RMII_SEL (1U << 23)

/* --- FLASH --- */
#define FLASH_BASE_REG  0x40023C00U
#define FLASH_ACR       (*(volatile uint32_t *)(FLASH_BASE_REG + 0x00U))

/* --- GPIO --- */
#define GPIOA_BASE      0x40020000U
#define GPIOB_BASE      0x40020400U
#define GPIOC_BASE      0x40020800U
#define GPIOD_BASE      0x40020C00U
#define GPIOE_BASE      0x40021000U
#define GPIOF_BASE      0x40021400U
#define GPIOG_BASE      0x40021800U
#define GPIOH_BASE      0x40021C00U
#define GPIOI_BASE      0x40022000U
#define GPIOJ_BASE      0x40022400U
#define GPIOK_BASE      0x40022800U

#define GPIO_MODER      0x00U
#define GPIO_OTYPER     0x04U
#define GPIO_OSPEEDR    0x08U
#define GPIO_PUPDR      0x0CU
#define GPIO_IDR        0x10U
#define GPIO_ODR        0x14U
#define GPIO_BSRR       0x18U
#define GPIO_AFRL       0x20U
#define GPIO_AFRH       0x24U

static inline volatile uint32_t *gpio_reg(uint32_t port_base, uint32_t offset)
{
    return (volatile uint32_t *)(port_base + offset);
}

#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + GPIO_MODER))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + GPIO_OSPEEDR))
#define GPIOA_PUPDR     (*(volatile uint32_t *)(GPIOA_BASE + GPIO_PUPDR))
#define GPIOA_AFRH      (*(volatile uint32_t *)(GPIOA_BASE + GPIO_AFRH))

#define GPIO_MODE_INPUT     0U
#define GPIO_MODE_OUTPUT    1U
#define GPIO_MODE_AF        2U
#define GPIO_MODE_ANALOG    3U

#define GPIO_SPEED_LOW      0U
#define GPIO_SPEED_MEDIUM   1U
#define GPIO_SPEED_HIGH     2U
#define GPIO_SPEED_VERY_HIGH 3U

#define GPIO_PUPD_NONE      0U
#define GPIO_PUPD_UP        1U
#define GPIO_PUPD_DOWN      2U

/* --- SDMMC1 --- */
#define SDMMC1_BASE     0x40012C00U

typedef struct {
    volatile uint32_t POWER;
    volatile uint32_t CLKCR;
    volatile uint32_t ARG;
    volatile uint32_t CMD;
    volatile uint32_t RESPCMD;
    volatile uint32_t RESP1;
    volatile uint32_t RESP2;
    volatile uint32_t RESP3;
    volatile uint32_t RESP4;
    volatile uint32_t DTIMER;
    volatile uint32_t DLEN;
    volatile uint32_t DCTRL;
    volatile uint32_t DCOUNT;
    volatile uint32_t STA;
    volatile uint32_t ICR;
    volatile uint32_t MASK;
    volatile uint32_t RESERVED0[2];
    volatile uint32_t FIFOCNT;
    volatile uint32_t RESERVED1[13];
    volatile uint32_t FIFO;
} SDMMC1_TypeDef;

#define SDMMC1          ((SDMMC1_TypeDef *)SDMMC1_BASE)

/* --- SDMMC2 (on-board microSD on STM32F769I-DISC1) --- */
#define SDMMC2_BASE     0x40011C00U

typedef struct {
    volatile uint32_t POWER;
    volatile uint32_t CLKCR;
    volatile uint32_t ARG;
    volatile uint32_t CMD;
    volatile uint32_t RESPCMD;
    volatile uint32_t RESP1;
    volatile uint32_t RESP2;
    volatile uint32_t RESP3;
    volatile uint32_t RESP4;
    volatile uint32_t DTIMER;
    volatile uint32_t DLEN;
    volatile uint32_t DCTRL;
    volatile uint32_t DCOUNT;
    volatile uint32_t STA;
    volatile uint32_t ICR;
    volatile uint32_t MASK;
    volatile uint32_t RESERVED0[2];
    volatile uint32_t FIFOCNT;
    volatile uint32_t RESERVED1[13];
    volatile uint32_t FIFO;
} SDMMC2_TypeDef;

#define SDMMC2          ((SDMMC2_TypeDef *)SDMMC2_BASE)

#define RCC_APB2ENR_SDMMC2EN    (1U << 7)
#define RCC_APB2RSTR_SDMMC2RST  (1U << 7)
#define RCC_DCKCFGR2_SDMMC2SEL  (1U << 29)

#define SDMMC_POWER_PWRCTRL_Pos     0U
#define SDMMC_POWER_PWRCTRL_ON      3U

#define SDMMC_CLKCR_CLKEN           (1U << 8)
#define SDMMC_CLKCR_WIDBUS_Pos      11U

#define SDMMC_CMD_CPSMEN            (1U << 10)
#define SDMMC_CMD_CMDINDEX_Pos      0U
#define SDMMC_CMD_WAITRESP_SHORT    (1U << 6)
#define SDMMC_CMD_WAITRESP_LONG     (3U << 6)

#define SDMMC_DCTRL_DTEN            (1U << 0)
#define SDMMC_DCTRL_DTDIR           (1U << 1)
#define SDMMC_DCTRL_DBLOCKSIZE_Pos  4U

#define SDMMC_STA_CCRCFAIL          (1U << 0)
#define SDMMC_STA_DCRCFAIL          (1U << 1)
#define SDMMC_STA_CTIMEOUT          (1U << 2)
#define SDMMC_STA_DTIMEOUT          (1U << 3)
#define SDMMC_STA_TXUNDERR          (1U << 4)
#define SDMMC_STA_RXOVERR           (1U << 5)
#define SDMMC_STA_CMDREND           (1U << 6)
#define SDMMC_STA_CMDSENT           (1U << 7)
#define SDMMC_STA_DATAEND           (1U << 8)
#define SDMMC_STA_DBCKEND           (1U << 10)
#define SDMMC_STA_CMDACT            (1U << 11)
#define SDMMC_STA_RXFIFOHF          (1U << 15)
#define SDMMC_STA_TXFIFOHE          (1U << 14)
#define SDMMC_STA_RXDAVL            (1U << 21)

#define SDMMC_ICR_STATIC_MASK       0x00C007FFU

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

/* Cortex-M7 D-cache maintenance (no-op if D-cache is not enabled) */
void scb_inv_dcache(void *addr, uint32_t len);
void scb_clean_dcache(const void *addr, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* HAL_STM32F769I_DISC1_H */
