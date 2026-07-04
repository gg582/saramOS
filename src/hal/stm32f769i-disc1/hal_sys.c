#include <hal/stm32f769i-disc1.h>

#define SCB_BASE        0xE000ED00U
#define SCB_CCR         (*(volatile uint32_t *)(SCB_BASE + 0x14U))
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
