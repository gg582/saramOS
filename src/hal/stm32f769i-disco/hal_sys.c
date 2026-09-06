#include <hal/board.h>

#define SCB_BASE        0xE000ED00U
#define SCB_VTOR        (*(volatile uint32_t *)(SCB_BASE + 0x08U))
#define SCB_CCR         (*(volatile uint32_t *)(SCB_BASE + 0x14U))

#define SYSTICK_BASE    0xE000E010U
#define SYSTICK_CSR     (*(volatile uint32_t *)(SYSTICK_BASE + 0x00U))
#define SYSTICK_RVR     (*(volatile uint32_t *)(SYSTICK_BASE + 0x04U))
#define SYSTICK_CVR     (*(volatile uint32_t *)(SYSTICK_BASE + 0x08U))

#define SYSTICK_ENABLE  (1U << 0)
#define SYSTICK_TICKINT (1U << 1)
#define SYSTICK_CLKSOURCE (1U << 2)

/* System clock: 168 MHz from the 25 MHz HSE crystal via PLL. */
#define SYSCLK_HZ       168000000U
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

/* MPU registers (Cortex-M7). */
#define SCB_MPU_TYPE    (*(volatile uint32_t *)(SCB_BASE + 0x90U))
#define SCB_MPU_CTRL    (*(volatile uint32_t *)(SCB_BASE + 0x94U))
#define SCB_MPU_RNR     (*(volatile uint32_t *)(SCB_BASE + 0x98U))
#define SCB_MPU_RBAR    (*(volatile uint32_t *)(SCB_BASE + 0x9CU))
#define SCB_MPU_RASR    (*(volatile uint32_t *)(SCB_BASE + 0xA0U))

#define MPU_CTRL_ENABLE      (1U << 0)
#define MPU_CTRL_HFNMIENA    (1U << 1)
#define MPU_CTRL_PRIVDEFENA  (1U << 2)

/* PWR registers needed for high-speed operation. */
#define PWR_BASE        0x40007000U
#define PWR_CR1         (*(volatile uint32_t *)(PWR_BASE + 0x00U))
#define PWR_CSR1        (*(volatile uint32_t *)(PWR_BASE + 0x04U))
#define PWR_CR1_VOS_Pos     14U
#define PWR_CR1_VOS_SCALE1  (3U << PWR_CR1_VOS_Pos)
#define PWR_CSR1_VOSRDY     (1U << 14U)

#define RCC_APB1ENR_PWREN   (1U << 28U)

/* Flash registers. */
#define FLASH_BASE_REG  0x40023C00U
#define FLASH_ACR       (*(volatile uint32_t *)(FLASH_BASE_REG + 0x00U))
#define FLASH_ACR_LATENCY_Pos   0U
#define FLASH_ACR_LATENCY_Msk   0xFU
#define FLASH_ACR_PRFTEN        (1U << 8)
#define FLASH_ACR_ARTEN         (1U << 9)

static void enable_hse(void)
{
    RCC_CR |= (1U << 16); /* HSEON */
    uint32_t timeout = 100000U;
    while (!(RCC_CR & (1U << 17))) { /* HSERDY */
        if (--timeout == 0U)
            break;
    }
}

static void mpu_init(void)
{
    /* If no MPU is present, skip. */
    if ((SCB_MPU_TYPE & 0xFFU) == 0U)
        return;

    /* Region 0: external SDRAM (0xC0000000, 16 MB).
     * Normal, Non-cacheable, Shareable memory (TEX=001,C=0,B=0,S=1). This
     * still forbids the CPU from caching stale copies of LTDC/DMA-written
     * data without any cache-maintenance calls, but unlike Strongly-ordered
     * it permits unaligned accesses and CPU write bursting, which
     * Strongly-ordered forbids at the architecture level and which the
     * framebuffer (bulk RGB565 writes) and DMA2D/LTDC depend on. */
    SCB_MPU_RNR = 0U;
    SCB_MPU_RBAR = 0xC0000000U;
    /* XN=1, AP=full access, TEX=1, S=1, C=0, B=0 (Normal, Non-cacheable),
     * SIZE=23 (2^24 = 16 MB), ENABLE=1. */
    SCB_MPU_RASR = (1U << 28) |
                   (3U << 24) |
                   (1U << 19) |
                   (1U << 18) |
                   (23U << 1) |
                   (1U << 0);

    /* Enable MPU with default memory map for unconfigured regions. */
    SCB_MPU_CTRL = MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;
    dsb_barrier();
    isb_barrier();
}

static void set_sysclk_pll(void)
{
    /* Enable PWR clock and select voltage regulator scale 1 for 168 MHz. */
    RCC_APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC_APB1ENR;
    PWR_CR1 |= PWR_CR1_VOS_SCALE1;
    /* Wait a short time for the regulator scaling to settle; some revisions
     * do not assert VOSRDY immediately after a fresh reset.
     */
    for (volatile uint32_t i = 0; i < 10000U; i++)
        ;

    /* Set flash latency to 6 WS and enable ART prefetch (required at 168 MHz). */
    FLASH_ACR &= ~FLASH_ACR_LATENCY_Msk;
    FLASH_ACR |= 6U;
    FLASH_ACR |= FLASH_ACR_PRFTEN | FLASH_ACR_ARTEN;
    while ((FLASH_ACR & FLASH_ACR_LATENCY_Msk) != 6U)
        ;

    /* Configure PLL: HSE(25 MHz) / 25 * 336 / 2 = 168 MHz. */
    RCC_PLLCFGR = (25U << 0)   |  /* PLLM = 25  */
                  (336U << 6)  |  /* PLLN = 336 */
                  (0U << 16)   |  /* PLLP = 2   */
                  (1U << 22)   |  /* PLLSRC = HSE */
                  (7U << 24);     /* PLLQ = 7   */

    RCC_CR |= (1U << 24); /* PLLON */
    uint32_t timeout = 100000U;
    while (!(RCC_CR & (1U << 25))) { /* PLLRDY */
        if (--timeout == 0U)
            break;
    }

    /* Bus prescalers: AHB=1, APB1=/4 (42 MHz), APB2=/2 (84 MHz). */
    uint32_t cfgr = RCC_CFGR;
    cfgr &= ~0xFFFFU;
    cfgr |= (0U << 4)   | /* HPRE  /1  */
            (5U << 10)  | /* PPRE1 /4  */
            (4U << 13);   /* PPRE2 /2  */
    RCC_CFGR = cfgr;

    /* Switch system clock to PLL. */
    cfgr = RCC_CFGR;
    cfgr &= ~3U;
    cfgr |= 2U; /* SW = PLL */
    RCC_CFGR = cfgr;
    while ((RCC_CFGR & 0xCU) != (2U << 2))
        ;
}

void hal_system_init(void)
{
    SCB_VTOR = 0x08000000U;

    /* Enable I-Cache and D-Cache for Cortex-M7.
     * The caches are invalid after reset, so we can enable directly.
     */
    dsb_barrier();
    isb_barrier();

    /* Caches are kept disabled until an MPU region table is added to
     * separate Normal (SRAM/flash) and Device (peripheral) memory types.
     * Enabling D-cache without MPU on Cortex-M7 causes bus faults.
     */
    (void)SCB_CCR;

    dsb_barrier();
    isb_barrier();

    enable_hse();
    set_sysclk_pll();

    /* Configure MPU before caches are enabled or SDRAM is accessed.
     * SDRAM must be treated as Device/Shareable so CPU writes are visible
     * to LTDC in order and FMC read-after-write hazards are avoided. */
    mpu_init();
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
    __asm volatile ("cpsie i" ::: "memory");
}

void SysTick_Handler(void)
{
    saramos_tick_ms++;
}
