#include <hal/board.h>

#define SCB_BASE        0xE000ED00U
#define SCB_CCR         (*(volatile uint32_t *)(SCB_BASE + 0x14U))

#define SYSTICK_BASE    0xE000E010U
#define SYSTICK_CSR     (*(volatile uint32_t *)(SYSTICK_BASE + 0x00U))
#define SYSTICK_RVR     (*(volatile uint32_t *)(SYSTICK_BASE + 0x04U))
#define SYSTICK_CVR     (*(volatile uint32_t *)(SYSTICK_BASE + 0x08U))

#define SYSTICK_ENABLE  (1U << 0)
#define SYSTICK_TICKINT (1U << 1)
#define SYSTICK_CLKSOURCE (1U << 2)

/* System clock is HSI 16 MHz. */
#define SYSCLK_HZ       16000000U
#define SYSTICK_RELOAD  ((SYSCLK_HZ / 1000U) - 1U)

extern volatile uint32_t saramos_tick_ms;
#define SCB_CCSIDR      (*(volatile uint32_t *)(SCB_BASE + 0x80U))
#define SCB_DCIMVAC     (*(volatile uint32_t *)(SCB_BASE + 0x5CU))
#define SCB_DCCMVAC     (*(volatile uint32_t *)(SCB_BASE + 0x68U))

#define SCB_CCR_DC      (1U << 16)
#define SCB_CCR_IC      (1U << 17)

#define SCB_DCACHE_LINE_SIZE 32U

static inline void dsb_barrier(void)
{
    __asm volatile ("dsb" ::: "memory");
}

static inline void isb_barrier(void)
{
    __asm volatile ("isb" ::: "memory");
}

void hal_system_init(void)
{
    /* Enable I-Cache and D-Cache for Cortex-M7.
     * The caches are invalid after reset, so we can enable directly.
     */
    dsb_barrier();
    isb_barrier();

    /* Caches are kept disabled until an MPU region table is added to
     * separate Normal (SRAM/flash) and Device (peripheral) memory types.
     * Enabling D-cache without MPU on Cortex-M7 causes bus faults. */
    (void)SCB_CCR;

    dsb_barrier();
    isb_barrier();

    /* Set FLASH latency for 16 MHz (0 wait states is enough) */
    FLASH_ACR &= ~0xFU;
    FLASH_ACR |= 0U; /* 0 WS */

    /* Keep HSI as system clock (default after reset) */
    /* No PLL setup needed for minimal bring-up */
}

void scb_inv_dcache(void *addr, uint32_t len)
{
    if (!(SCB_CCR & SCB_CCR_DC))
        return;

    uint32_t start = (uint32_t)addr & ~(SCB_DCACHE_LINE_SIZE - 1U);
    uint32_t end = (uint32_t)addr + len;

    for (uint32_t mva = start; mva < end; mva += SCB_DCACHE_LINE_SIZE) {
        SCB_DCIMVAC = mva;
    }

    dsb_barrier();
    isb_barrier();
}

void scb_clean_dcache(const void *addr, uint32_t len)
{
    if (!(SCB_CCR & SCB_CCR_DC))
        return;

    uint32_t start = (uint32_t)addr & ~(SCB_DCACHE_LINE_SIZE - 1U);
    uint32_t end = (uint32_t)addr + len;

    for (uint32_t mva = start; mva < end; mva += SCB_DCACHE_LINE_SIZE) {
        SCB_DCCMVAC = mva;
    }

    dsb_barrier();
    isb_barrier();
}

void hal_systick_init(void)
{
    SYSTICK_CVR = 0;
    SYSTICK_RVR = SYSTICK_RELOAD;
    SYSTICK_CSR = SYSTICK_ENABLE | SYSTICK_TICKINT | SYSTICK_CLKSOURCE;
}

void SysTick_Handler(void)
{
    saramos_tick_ms++;
}
