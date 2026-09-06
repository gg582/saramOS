/*
 * HAL SDRAM initialization for STM32F769I-DISCO.
 *
 * Based on the STM32CubeF7 BSP (stm32f769i_discovery_sdram.c).
 * The system clock is 168 MHz from HSE, so HCLK/2 gives an ~84 MHz SDRAM clock.
 *
 * The external SDRAM is an ISSI IS42S32400F-6BL (or equivalent MT48LC4M32B2):
 *   - 16 MByte total
 *   - 12-bit row, 8-bit column, 32-bit data bus, 4 internal banks
 *   - Mapped to FMC SDRAM Bank 1 (0xC0000000)
 */
#include "hal_sdram.h"
#include "hal_display.h"
#include <hal/board.h>
#include <hal/hal_gpio.h>
#include <stdio.h>

extern void hal_uart_puts(const char *s);

#define SDRAM_MODEREG_BURST_LENGTH_1          0x0000U
#define SDRAM_MODEREG_BURST_LENGTH_2          0x0001U
#define SDRAM_MODEREG_BURST_LENGTH_4          0x0002U
#define SDRAM_MODEREG_BURST_LENGTH_8          0x0004U
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   0x0000U
#define SDRAM_MODEREG_BURST_TYPE_INTERLEAVED  0x0008U
#define SDRAM_MODEREG_CAS_LATENCY_2           0x0020U
#define SDRAM_MODEREG_CAS_LATENCY_3           0x0030U
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD 0x0000U
#define SDRAM_MODEREG_WRITEBURST_MODE_PROGRAM 0x0000U
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE  0x0200U

static void sdram_gpio_init(void)
{
    /* Enable all GPIO clocks touched by FMC SDRAM. */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN |
                   RCC_AHB1ENR_GPIOFEN | RCC_AHB1ENR_GPIOGEN |
                   RCC_AHB1ENR_GPIOHEN | RCC_AHB1ENR_GPIOIEN;

    /* GPIOD: D0=PD14, D1=PD15, D2=PD0, D3=PD1, D13=PD8, D14=PD9, D15=PD10 */
    hal_gpio_init_af(GPIOD_BASE, 14, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOD_BASE, 15, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOD_BASE,  0, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOD_BASE,  1, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOD_BASE,  8, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOD_BASE,  9, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOD_BASE, 10, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);

    /* GPIOE: NBL0=PE0, NBL1=PE1, D4=PE7, D5=PE8, D6=PE9, D7=PE10,
     *        D8=PE11, D9=PE12, D10=PE13, D11=PE14, D12=PE15 */
    hal_gpio_init_af(GPIOE_BASE,  0, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOE_BASE,  1, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOE_BASE,  7, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOE_BASE,  8, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOE_BASE,  9, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOE_BASE, 10, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOE_BASE, 11, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOE_BASE, 12, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOE_BASE, 13, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOE_BASE, 14, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOE_BASE, 15, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);

    /* GPIOF: A0=PF0, A1=PF1, A2=PF2, A3=PF3, A4=PF4, A5=PF5,
     *        A12=PF12, A13=PF13, A14=PF14, A15=PF15, SDNRAS=PF11 */
    hal_gpio_init_af(GPIOF_BASE,  0, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOF_BASE,  1, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOF_BASE,  2, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOF_BASE,  3, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOF_BASE,  4, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOF_BASE,  5, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOF_BASE, 11, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOF_BASE, 12, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOF_BASE, 13, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOF_BASE, 14, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOF_BASE, 15, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);

    /* GPIOG: A10=PG0, A11=PG1, A12=PG2, A14=PG4, A15=PG5,
     *        SDCLK=PG8, SDNCAS=PG15 */
    hal_gpio_init_af(GPIOG_BASE,  0, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOG_BASE,  1, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOG_BASE,  2, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOG_BASE,  4, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOG_BASE,  5, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOG_BASE,  8, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOG_BASE, 15, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);

    /* GPIOH: SDCKE0=PH2, SDNE0=PH3, SDNWE=PH5, D16=PH8, D17=PH9, D18=PH10,
     *        D19=PH11, D20=PH12, D21=PH13, D22=PH14, D23=PH15 */
    hal_gpio_init_af(GPIOH_BASE,  2, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOH_BASE,  3, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOH_BASE,  5, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    for (uint8_t pin = 8; pin <= 15; pin++)
        hal_gpio_init_af(GPIOH_BASE, pin, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);

    /* GPIOI: D24=PI0, D25=PI1, D26=PI2, D27=PI3, NBL2=PI4, NBL3=PI5,
     *        D28=PI6, D29=PI7, D30=PI9, D31=PI10 */
    hal_gpio_init_af(GPIOI_BASE,  0, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOI_BASE,  1, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOI_BASE,  2, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOI_BASE,  3, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOI_BASE,  4, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOI_BASE,  5, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOI_BASE,  6, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOI_BASE,  7, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOI_BASE,  9, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOI_BASE, 10, 12, GPIO_SPEED_VERY_HIGH, GPIO_PUPD_NONE);
}

/* FMC_SDSR bit 5 is the BUSY flag on STM32F7. */
#define FMC_SDSR_BUSY   (1U << 5)

/* Rough busy-wait calibrated for the 216 MHz HCLK configured by hal_system_init()
 * (was 168 MHz; 50 iterations/us at 168 MHz scaled by 216/168 = 1.2857). */
static void sdram_udelay(uint32_t us)
{
    for (volatile uint32_t i = 0; i < (us * 64U); i++)
        ;
}

static void sdram_cmd(uint32_t mode, uint32_t bank, uint32_t nrfs, uint32_t mrd)
{
    uint32_t cmd = (mode << FMC_SDCMR_MODE_Pos) |
                   (bank) |
                   ((nrfs - 1U) << FMC_SDCMR_NRFS_Pos) |
                   (mrd << FMC_SDCMR_MRD_Pos);
    FMC_SDCMR = cmd;
    __asm volatile("dsb" ::: "memory");
    while (FMC_SDSR & FMC_SDSR_BUSY)
        ; /* Busy */
}

void hal_sdram_init(void)
{
    hal_uart_puts("[SDRAM] clock enable\r\n");
    /* Enable FMC clock. */
    RCC_AHB3ENR |= (1U << 0);
    (void)RCC_AHB3ENR;

    sdram_gpio_init();
    hal_uart_puts("[SDRAM] gpio done\r\n");

    /* Configure SDRAM Bank 1.
     *
     * hal_sys.c now runs SYSCLK at 216 MHz specifically so this SDRAM
     * config can match Zephyr's own, verified-working FMC/SDRAM setup for
     * this exact board+panel (boards/shields/st_b_lcd40_dsi1_mb1166 in a
     * Zephyr checkout: st,sdram-control / st,sdram-timing / refresh-rate)
     * byte for byte, at the same 108 MHz SDRAM clock (SDCLK = HCLK/2 =
     * 216/2), instead of re-deriving values by unit conversion from a
     * different base clock -- which is what the previous 168 MHz config
     * did, and got the actual SDCLK frequency (84 MHz, not the 56 MHz the
     * old timing comments assumed) wrong in the process.
     *
     *   NC = 8 bits    -> 0
     *   NR = 12 bits   -> 1
     *   MWID = 32 bits -> 2
     *   NB = 4 internal banks -> 1
     *   CAS latency = 3 -> 3   (Zephyr: STM32_FMC_SDRAM_CAS_3)
     *   Write protection disabled -> 0
     *   SDCLK = HCLK/2 -> 2    (108 MHz from 216 MHz HCLK, matches Zephyr)
     *   Read burst enabled for DSI/LTDC display scans
     *   Read pipe delay = 0 HCLK cycles.
     */
    FMC_SDCR1 = (0U << 0)  | /* NC = 8 bits */
                (1U << 2)  | /* NR = 12 bits */
                (2U << 4)  | /* MWID = 32 bits */
                (1U << 6)  | /* NB = 4 internal banks */
                (3U << 7)  | /* CAS latency = 3 */
                (0U << 9)  | /* WP disabled */
                (2U << 10) | /* SDCLK = HCLK/2 */
                (1U << 12) | /* Read burst enabled */
                (0U << 13);  /* RPIPE = 0 */

    /* Timing taken directly from Zephyr's board overlay for this panel
     * (st,sdram-timing = <2 6 4 6 2 2 2>, i.e. TMRD=2, TXSR=6, TRAS=4,
     * TRC=6, TWR=2, TRP=2, TRCD=2 memory-clock cycles). Those are the
     * *desired cycle counts*, as ST's own HAL_SDRAM_Init() takes them
     * (and Zephyr's driver just forwards them straight into that HAL
     * call) -- the raw FMC_SDTR1 field encoding is cycles-1, which is why
     * every field below is one less than the number quoted above. */
    FMC_SDTR1 = (1U << 0)   | /* TMRD = LoadToActiveDelay: 2 cycles    */
                (5U << 4)   | /* TXSR = ExitSelfRefreshDelay: 6 cycles */
                (3U << 8)   | /* TRAS = SelfRefreshTime: 4 cycles      */
                (5U << 12)  | /* TRC  = RowCycleDelay: 6 cycles        */
                (1U << 16)  | /* TWR  = WriteRecoveryTime: 2 cycles    */
                (1U << 20)  | /* TRP  = RPDelay: 2 cycles              */
                (1U << 24);  /* TRCD = RCDDelay: 2 cycles              */
    FMC_SDTR2 = 0x00000000U;

    /* IS42S32400F power-up: SDCLK must be stable for at least 100 us with CKE
     * low before the CLK_EN command raises CKE. */
    sdram_udelay(200);

    /* Clock enable command for Bank 1 (raises CKE). */
    sdram_cmd(FMC_SDCMR_MODE_CLK_EN, FMC_SDCMR_CTB1, 1, 0);
    hal_uart_puts("[SDRAM] clk en done\r\n");

    /* After CKE goes high, another 100 us must elapse before any command other
     * than NOP/COMMAND INHIBIT. Use a generous margin. */
    sdram_udelay(200);

    /* Precharge all command. */
    sdram_cmd(FMC_SDCMR_MODE_PALL, FMC_SDCMR_CTB1, 1, 0);
    hal_uart_puts("[SDRAM] pall done\r\n");
    sdram_udelay(100);

    /* Auto-refresh: 8 refresh cycles. */
    sdram_cmd(FMC_SDCMR_MODE_AUTO_REFRESH, FMC_SDCMR_CTB1, 8, 0);
    hal_uart_puts("[SDRAM] auto refresh done\r\n");
    sdram_udelay(100);

    /* Load mode register: burst length 1, CAS 3 (must match FMC_SDCR1's
     * CAS field above). */
    uint32_t mrd = SDRAM_MODEREG_BURST_LENGTH_1 |
                   SDRAM_MODEREG_CAS_LATENCY_3;
    sdram_cmd(FMC_SDCMR_MODE_LOAD_MODE, FMC_SDCMR_CTB1, 1, mrd);
    hal_uart_puts("[SDRAM] load mode done\r\n");
    sdram_udelay(100);

    /* Return the controller to normal mode before CPU/DMA accesses. */
    sdram_cmd(FMC_SDCMR_MODE_NORMAL, FMC_SDCMR_CTB1, 1, 0);
    hal_uart_puts("[SDRAM] normal mode done\r\n");

    /* Refresh rate: taken directly from Zephyr's board overlay for this
     * panel (refresh-rate = <603>), at the same 108 MHz SDRAM clock this
     * driver now uses. */
    hal_uart_puts("[SDRAM] set refresh\r\n");
    FMC_SDRTR = (603U << 1);

    /* Reset NOR/PSRAM Bank 1 control to its known-after-reset state.
     * This disables speculative/pre-fetch accesses to the NOR/PSRAM region
     * that can otherwise stall the shared FMC bus and corrupt back-to-back
     * SDRAM writes.  Same workaround used by working community ports for
     * this chip/board. */
    FMC_BCR1 = 0x000030D2U;
    __asm volatile("dsb" ::: "memory");

    /* Wait a little for the controller to settle. */
    for (volatile uint32_t i = 0; i < 0x20000U; i++)
        ;

    volatile uint32_t *p = (volatile uint32_t *)SDRAM_BASE_ADDR;

    /* Diagnostic: show the actual register values after init. */
    {
        char dbg[96];
        snprintf(dbg, sizeof(dbg),
                 "[SDRAM] regs SDCR1=%08lx SDTR1=%08lx SDRTR=%08lx\r\n",
                 (unsigned long)FMC_SDCR1, (unsigned long)FMC_SDTR1,
                 (unsigned long)FMC_SDRTR);
        hal_uart_puts(dbg);
    }

    /* Dummy write/read to ensure the FMC/SDRAM path is out of power-up state. */
    p[0] = 0xDEADBEEFU;
    __asm volatile("dsb" ::: "memory");
    uint32_t dummy = p[0];
    {
        char dbg[64];
        snprintf(dbg, sizeof(dbg), "[SDRAM] dummy read %08lx\r\n",
                 (unsigned long)dummy);
        hal_uart_puts(dbg);
    }

    /* Single-word sanity: write/read back immediately to flush the FMC
     * write path before any bulk test. */
    hal_uart_puts("[SDRAM] sanity test\r\n");
    {
        p[16] = 0x12345678U;
        __asm volatile("dsb" ::: "memory");
        uint32_t got = p[16];
        char dbg[64];
        snprintf(dbg, sizeof(dbg), "[SDRAM] single word got=%08lx\r\n",
                 (unsigned long)got);
        hal_uart_puts(dbg);
    }

    /* Block writes: write all, wait, then read all back.
     * The long delay distinguishes a true write-completion problem from a
     * persistent address/data-path bug. */
    for (uint32_t block = 2; block <= 16; block *= 2) {
        volatile int ok = 1;
        uint32_t base = block * 16U;
        for (uint32_t i = 0; i < block; i++) {
            p[base + i] = 0xA5A5A5A5U ^ i;
            __asm volatile("dsb" ::: "memory");
        }
        /* Long delay to ensure any pending writes complete at SDRAM. */
        sdram_udelay(10000);
        __asm volatile("dsb" ::: "memory");
        for (uint32_t i = 0; i < block; i++) {
            uint32_t exp = 0xA5A5A5A5U ^ i;
            uint32_t got = p[base + i];
            if (got != exp) {
                ok = 0;
                char dbg[80];
                snprintf(dbg, sizeof(dbg),
                         "[SDRAM] block=%lu FAILED off=%lu got=%08lx exp=%08lx\r\n",
                         (unsigned long)block, (unsigned long)i,
                         (unsigned long)got, (unsigned long)exp);
                hal_uart_puts(dbg);
            }
        }
        if (ok) {
            char dbg[48];
            snprintf(dbg, sizeof(dbg), "[SDRAM] block=%lu ok\r\n", (unsigned long)block);
            hal_uart_puts(dbg);
        }
    }
}

uint32_t hal_sdram_base(void)
{
    return SDRAM_BASE_ADDR;
}

uint32_t hal_sdram_size(void)
{
    return SDRAM_SIZE;
}
