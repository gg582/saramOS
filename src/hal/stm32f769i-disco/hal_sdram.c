/*
 * HAL SDRAM initialization for STM32F769I-DISCO.
 *
 * Based on the STM32CubeF7 BSP (stm32f769i_discovery_sdram.c) but adapted
 * for the saramOS HSI-based clock tree (HCLK = 16 MHz -> SDRAM clock ~8 MHz).
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

static void sdram_cmd(uint32_t mode, uint32_t bank, uint32_t nrfs, uint32_t mrd)
{
    uint32_t cmd = (mode << FMC_SDCMR_MODE_Pos) |
                   (bank) |
                   (nrfs << FMC_SDCMR_NRFS_Pos) |
                   (mrd << FMC_SDCMR_MRD_Pos);
    FMC_SDCMR = cmd;
    while (FMC_SDSR & (1U << 5))
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

    /* Configure SDRAM Bank 1:
     *   NC = 8 bits  -> 0
     *   NR = 12 bits -> 1
     *   MWID = 32 bits -> 2
     *   NB = 4 internal banks -> 1
     *   CAS latency = 3 -> 2
     *   Write protection disabled -> 0
     *   SDCLK = HCLK/2 -> 1
     *   Read burst disabled (safer at 84 MHz without D-cache/MPU tuning)
     *   Read pipe delay = 2 HCLK cycles for 84 MHz SDRAM.
     */
    FMC_SDCR1 = (0U << 0)  | /* NC = 8 bits */
                (1U << 2)  | /* NR = 12 bits */
                (2U << 4)  | /* MWID = 32 bits */
                (1U << 6)  | /* NB = 4 internal banks */
                (2U << 7)  | /* CAS latency = 3 */
                (0U << 9)  | /* WP disabled */
                (1U << 10) | /* SDCLK = HCLK/2 */
                (0U << 12) | /* Read burst disabled */
                (2U << 13);  /* RPIPE = 2 */

    /* Timing counts are the same as the ST BSP; they are safe at 84 MHz. */
    FMC_SDTR1 = (2U << 0)   | /* TMRD = LoadToActiveDelay    */
                (7U << 4)   | /* TXSR = ExitSelfRefreshDelay */
                (4U << 8)   | /* TRAS = SelfRefreshTime      */
                (7U << 12)  | /* TRC  = RowCycleDelay        */
                (2U << 16)  | /* TWR  = WriteRecoveryTime    */
                (2U << 20)  | /* TRP  = RPDelay              */
                (2U << 24);  /* TRCD = RCDDelay             */
    FMC_SDTR2 = 0x00000000U;

    /* Clock enable command for Bank 1. */
    sdram_cmd(FMC_SDCMR_MODE_CLK_EN, FMC_SDCMR_CTB1, 1, 0);
    hal_uart_puts("[SDRAM] clk en done\r\n");

    /* Small delay (>100 us at 16 MHz). */
    for (volatile uint32_t i = 0; i < 0x2000U; i++)
        ;

    /* Precharge all command. */
    sdram_cmd(FMC_SDCMR_MODE_PALL, FMC_SDCMR_CTB1, 1, 0);
    hal_uart_puts("[SDRAM] pall done\r\n");

    /* Auto-refresh: 8 refresh cycles. */
    sdram_cmd(FMC_SDCMR_MODE_AUTO_REFRESH, FMC_SDCMR_CTB1, 8, 0);
    hal_uart_puts("[SDRAM] auto refresh done\r\n");

    /* Load mode register: burst length 1, sequential, CAS 3, single write. */
    uint32_t mrd = SDRAM_MODEREG_BURST_LENGTH_1 |
                   SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
                   SDRAM_MODEREG_CAS_LATENCY_3 |
                   SDRAM_MODEREG_OPERATING_MODE_STANDARD |
                   SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;
    sdram_cmd(FMC_SDCMR_MODE_LOAD_MODE, FMC_SDCMR_CTB1, 1, mrd);
    hal_uart_puts("[SDRAM] load mode done\r\n");

    /* Refresh rate: 4096 rows / 64 ms -> ~15.6 us/row.
     * With an ~84 MHz SDRAM clock (HCLK/2 from 168 MHz PLL) one cycle is
     * ~11.9 ns.  Count = 15.6 us / 11.9 ns - 20 ~= 1292.
     */
    hal_uart_puts("[SDRAM] set refresh\r\n");
    FMC_SDRTR = (1000U << 1);

    /* Wait a little for the controller to settle. */
    for (volatile uint32_t i = 0; i < 0x10000U; i++)
        ;

    volatile uint32_t *p = (volatile uint32_t *)SDRAM_BASE_ADDR;

    /* Simple sanity check: write a few distinct patterns and read them back. */
    hal_uart_puts("[SDRAM] sanity test\r\n");
    static const uint32_t patterns[] = {
        0xA5A5A5A5U,
        0x5A5A5A5AU,
        0x12345678U,
        0x9ABCDEF0U,
        0x00000000U,
        0xFFFFFFFFU,
    };
    volatile int ok = 1;
    for (uint32_t n = 0; n < (sizeof(patterns) / sizeof(patterns[0])); n++) {
        uint32_t base = n * 256U;
        uint32_t pat = patterns[n];
        for (uint32_t i = 0; i < 64U; i++)
            p[base + i] = pat ^ i;
        __asm volatile("dsb" ::: "memory");
        for (uint32_t i = 0; i < 64U; i++) {
            uint32_t expected = pat ^ i;
            uint32_t got = p[base + i];
            if (got != expected) {
                ok = 0;
                char dbg[80];
                snprintf(dbg, sizeof(dbg),
                         "[SDRAM] FAILED pat=%08lx off=%lu got=%08lx exp=%08lx\r\n",
                         (unsigned long)pat, (unsigned long)i,
                         (unsigned long)got, (unsigned long)expected);
                hal_uart_puts(dbg);
                break;
            }
        }
        if (!ok)
            break;
    }
    hal_uart_puts(ok ? "[SDRAM] sanity ok\r\n" : "[SDRAM] sanity FAILED\r\n");
}

uint32_t hal_sdram_base(void)
{
    return SDRAM_BASE_ADDR;
}

uint32_t hal_sdram_size(void)
{
    return SDRAM_SIZE;
}
