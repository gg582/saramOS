/*
 * FT6206 capacitive touch controller HAL driver for STM32F769I-DISCO.
 *
 * Pin mapping (per ST's official BSP, stm32f769i_discovery.h):
 *   PB8 -> I2C1_SCL (AF4)
 *   PB9 -> I2C1_SDA (AF4)
 *   PI13 -> touch controller INT (active low) -- not used here; this
 *           driver is polling-only, matching this board's other HAL
 *           drivers (ETH, SDMMC).
 *
 * I2C1 peripheral is STM32F7's "I2C v2" block (RM0410), driven here at
 * register level, polling-only, no DMA/interrupts -- same style as
 * hal_sdmmc.c.
 *
 * FT6206 I2C address: tries the two addresses ST's own BSP tries (board
 * revision A01 uses 0x2A 7-bit / 0x54 8-bit; the more common A02
 * revision uses 0x38 7-bit / 0x70 8-bit), keeping whichever ACKs a
 * register read first.
 */
#include <hal/board.h>
#include <hal/hal_gpio.h>
#include <hal/hal_touch.h>
#include "hal_display.h"

extern void hal_uart_puts(const char *s);

/* FT6206 register map (from ST's ft6x06_reg.h). */
#define FT6X06_TD_STAT_REG      0x02U
#define FT6X06_P1_XH_REG        0x03U
#define FT6X06_P1_XL_REG        0x04U
#define FT6X06_P1_YH_REG        0x05U
#define FT6X06_P1_YL_REG        0x06U
#define FT6X06_CHIP_ID_REG      0xA8U

#define FT6X06_ADDR_A01         0x2AU /* 0x54 >> 1 */
#define FT6X06_ADDR_A02         0x38U /* 0x70 >> 1 */

/* I2C1 timing register value from ST's own BSP reference (DISCOVERY_I2Cx_TIMING
 * in stm32f769i_discovery.h) -- computed for their I2C1 kernel clock
 * setup, which at 216 MHz SYSCLK matches this board's APB1 (PCLK1 =
 * SYSCLK/4 = 54 MHz, see hal_sys.c's bus prescaler comment) closely
 * enough to use directly, same "prime with the vendor's known-good
 * reference value" approach that worked for the DSI panel bring-up. */
#define I2C1_TIMING_VALUE       0x40912732U

static int g_touch_ready = 0;
static uint8_t g_touch_addr = FT6X06_ADDR_A02;

static void i2c_delay(volatile uint32_t n)
{
    while (n--)
        __asm volatile ("nop");
}

static void i2c1_gpio_init(void)
{
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /* AF4 (I2C1), open-drain (I2C is wired-AND -- hal_gpio_init_af()
     * doesn't touch OTYPER, so set it directly here via gpio_reg()),
     * pull-up. */
    hal_gpio_init_af(GPIOB_BASE, 8, 4, GPIO_SPEED_HIGH, GPIO_PUPD_UP);
    hal_gpio_init_af(GPIOB_BASE, 9, 4, GPIO_SPEED_HIGH, GPIO_PUPD_UP);
    *gpio_reg(GPIOB_BASE, GPIO_OTYPER) |= (1U << 8) | (1U << 9);
}

static void i2c1_init(void)
{
    RCC_APB1ENR |= RCC_APB1ENR_I2C1EN;
    RCC_APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    i2c_delay(100);
    RCC_APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;
    i2c_delay(100);

    I2C1->CR1 &= ~I2C_CR1_PE;
    I2C1->TIMINGR = I2C1_TIMING_VALUE;
    /* I2C_CR1_ANFOFF means "analog filter OFF" -- leave it clear (0)
     * so the analog filter stays enabled (the recommended default).
     * Do not set this bit. */
    I2C1->CR1 = 0;
    I2C1->CR1 |= I2C_CR1_PE;
}

/* Blocking, polling I2C1 register read: START, write reg addr (no
 * stop), repeated START, read `len` bytes, STOP. Returns 0 on success,
 * -1 on NACK/timeout. */
static int i2c1_read_reg(uint8_t dev_addr7, uint8_t reg, uint8_t *buf, uint32_t len)
{
    uint32_t timeout;

    /* Wait for bus idle. */
    timeout = 100000;
    while ((I2C1->ISR & I2C_ISR_BUSY) && --timeout)
        ;
    if (timeout == 0)
        return -1;

    /* Write phase: 1 byte (register address), no autoend (need a
     * repeated start into the read phase next). */
    I2C1->CR2 = ((uint32_t)dev_addr7 << I2C_CR2_SADD7_Pos) |
                (1U << I2C_CR2_NBYTES_Pos) |
                I2C_CR2_START;

    timeout = 100000;
    while (!(I2C1->ISR & (I2C_ISR_TXIS | I2C_ISR_NACKF)) && --timeout)
        ;
    if (timeout == 0 || (I2C1->ISR & I2C_ISR_NACKF)) {
        char dbg[96];
        __builtin_sprintf(dbg, "[TOUCH] addr phase fail: addr7=0x%02X timeout=%lu ISR=0x%08lX CR2=0x%08lX\r\n",
                           dev_addr7, (unsigned long)timeout, (unsigned long)I2C1->ISR, (unsigned long)I2C1->CR2);
        hal_uart_puts(dbg);
        I2C1->ICR = I2C_ICR_NACKCF;
        I2C1->CR2 |= I2C_CR2_STOP;
        return -1;
    }
    I2C1->TXDR = reg;

    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_TC) && --timeout)
        ;
    if (timeout == 0)
        return -1;

    /* Read phase: repeated START, `len` bytes, autoend (issues STOP
     * automatically after the last byte). */
    I2C1->CR2 = ((uint32_t)dev_addr7 << I2C_CR2_SADD7_Pos) |
                (len << I2C_CR2_NBYTES_Pos) |
                I2C_CR2_RD_WRN | I2C_CR2_START | I2C_CR2_AUTOEND;

    for (uint32_t i = 0; i < len; i++) {
        timeout = 100000;
        while (!(I2C1->ISR & (I2C_ISR_RXNE | I2C_ISR_NACKF)) && --timeout)
            ;
        if (timeout == 0 || (I2C1->ISR & I2C_ISR_NACKF)) {
            I2C1->ICR = I2C_ICR_NACKCF;
            return -1;
        }
        buf[i] = (uint8_t)I2C1->RXDR;
    }

    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_STOPF) && --timeout)
        ;
    I2C1->ICR = I2C_ICR_STOPCF;

    return 0;
}

void hal_touch_init(void)
{
    uint8_t chip_id;

    if (g_touch_ready)
        return;

    i2c1_gpio_init();
    i2c1_init();

    {
        char dbg[64];
        __builtin_sprintf(dbg, "[TOUCH] I2C1 ISR after init: 0x%08lX\r\n",
                           (unsigned long)I2C1->ISR);
        hal_uart_puts(dbg);
    }

    /* Probe: try the more common A02-revision address first, then
     * fall back to A01 -- same order/reasoning as ST's own BSP. */
    if (i2c1_read_reg(FT6X06_ADDR_A02, FT6X06_CHIP_ID_REG, &chip_id, 1) == 0) {
        g_touch_addr = FT6X06_ADDR_A02;
        g_touch_ready = 1;
    } else {
        hal_uart_puts("[TOUCH] probe @0x38 (A02) failed\r\n");
        if (i2c1_read_reg(FT6X06_ADDR_A01, FT6X06_CHIP_ID_REG, &chip_id, 1) == 0) {
            g_touch_addr = FT6X06_ADDR_A01;
            g_touch_ready = 1;
        } else {
            hal_uart_puts("[TOUCH] probe @0x2A (A01) failed\r\n");
        }
    }

    if (g_touch_ready) {
        char msg[48];
        __builtin_sprintf(msg, "[TOUCH] FT6206 ready, addr=0x%02X id=0x%02X\r\n",
                           g_touch_addr, chip_id);
        hal_uart_puts(msg);
    } else {
        hal_uart_puts("[TOUCH] FT6206 not responding (no touch panel?)\r\n");
    }
}

int hal_touch_read(int16_t *x, int16_t *y)
{
    uint8_t buf[5];

    if (!g_touch_ready)
        return 0;

    if (i2c1_read_reg(g_touch_addr, FT6X06_TD_STAT_REG, buf, 5) != 0)
        return 0;

    if ((buf[0] & 0x0FU) == 0U)
        return 0; /* no touch points */

    /* buf[1]=P1_XH, buf[2]=P1_XL, buf[3]=P1_YH, buf[4]=P1_YL.
     * XH/YH's low nibble holds the coordinate's high bits; the top
     * nibble is an event flag (0=press down, 1=lift up, 2=contact)
     * this driver doesn't need. */
    int16_t rx = (int16_t)(((buf[1] & 0x0FU) << 8) | buf[2]);
    int16_t ry = (int16_t)(((buf[3] & 0x0FU) << 8) | buf[4]);

    if (rx < 0) rx = 0;
    if (ry < 0) ry = 0;
    if (rx >= (int16_t)DISPLAY_WIDTH) rx = (int16_t)DISPLAY_WIDTH - 1;
    if (ry >= (int16_t)DISPLAY_HEIGHT) ry = (int16_t)DISPLAY_HEIGHT - 1;

    *x = rx;
    *y = ry;
    return 1;
}
