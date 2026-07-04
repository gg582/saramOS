/*
 * SDMMC2 HAL driver for STM32F769I-DISC1 on-board microSD socket.
 *
 * Pin mapping:
 *   PD6  -> SDMMC2_CK  (AF11)
 *   PD7  -> SDMMC2_CMD (AF11)
 *   PG9  -> SDMMC2_D0  (AF11)
 *   PG10 -> SDMMC2_D1  (AF11)
 *   PB3  -> SDMMC2_D2  (AF10)
 *   PB4  -> SDMMC2_D3  (AF10)
 *   PI15 -> SD_DETECT  (input, active-low)
 *
 * Bring-up uses 1-bit mode until the card is selected, then switches to
 * 4-bit mode for better throughput.  The driver is polling-only.
 */

#include <hal/hal_sdmmc.h>
#include <hal/hal_gpio.h>
#include <hal/stm32f769i-disc1.h>
#include <stdio.h>
#include <string.h>

#define SDMMC_CLK_SLOW_DIV  20U   /* 16 MHz / (2*20) = 400 kHz */
#define SDMMC_CLK_FAST_DIV  1U    /* 16 MHz / (2*1)  = 8 MHz  */

#define SD_CMD_ERR_MASK     (SDMMC_STA_CCRCFAIL | SDMMC_STA_CTIMEOUT)
#define SD_DATA_ERR_MASK    (SDMMC_STA_DCRCFAIL | SDMMC_STA_DTIMEOUT | SDMMC_STA_RXOVERR)

static uint16_t sd_rca;
static uint32_t sd_capacity_blocks;
static int sd_initialized;
static int sd_high_capacity;

static void sd_delay(volatile uint32_t n)
{
    while (n--)
        __asm volatile ("nop");
}

static void sdmmc_clock_enable(void)
{
    RCC_APB2ENR |= RCC_APB2ENR_SDMMC2EN;
    (void)RCC_APB2ENR;
}

static void sdmmc_reset(void)
{
    RCC_APB2RSTR |= RCC_APB2RSTR_SDMMC2RST;
    RCC_APB2RSTR &= ~RCC_APB2RSTR_SDMMC2RST;
    sd_delay(1000);
}

static void sdmmc_set_clock(uint32_t div)
{
    uint32_t clkcr = SDMMC_CLKCR_CLKEN | ((div & 0x3FFU) << 0);
    SDMMC2->CLKCR = clkcr;
    sd_delay(1000);
}

static void sdmmc_power_on(void)
{
    /* Power-off -> delay -> power-on sequence from the reference manual. */
    SDMMC2->POWER = 0U;
    sd_delay(50000);
    SDMMC2->POWER = (SDMMC_POWER_PWRCTRL_ON << SDMMC_POWER_PWRCTRL_Pos);
    sd_delay(100000);
}

static void sdmmc_clear_flags(void)
{
    SDMMC2->ICR = SDMMC_ICR_STATIC_MASK;
}

static int sdmmc_wait_cmd(int expect_resp, uint32_t timeout_us)
{
    while (timeout_us--) {
        uint32_t sta = SDMMC2->STA;
        if (sta & SDMMC_STA_CTIMEOUT)
            return HAL_SDMMC_TIMEOUT;
        if (sta & SDMMC_STA_CCRCFAIL)
            return HAL_SDMMC_ERR;
        if (expect_resp) {
            if (sta & SDMMC_STA_CMDREND)
                return HAL_SDMMC_OK;
        } else {
            if (sta & SDMMC_STA_CMDSENT)
                return HAL_SDMMC_OK;
        }
    }
    return HAL_SDMMC_TIMEOUT;
}

static int sdmmc_wait_data_end(uint32_t timeout_us)
{
    while (timeout_us--) {
        uint32_t sta = SDMMC2->STA;
        if (sta & SDMMC_STA_DTIMEOUT)
            return HAL_SDMMC_TIMEOUT;
        if (sta & SDMMC_STA_DCRCFAIL)
            return HAL_SDMMC_ERR;
        if (sta & SDMMC_STA_DATAEND)
            return HAL_SDMMC_OK;
    }
    return HAL_SDMMC_TIMEOUT;
}

static int sdmmc_send_cmd_raw(uint8_t cmd_idx, uint32_t arg, int resp_type, uint32_t *resp)
{
    uint32_t cmd = ((uint32_t)(cmd_idx & 0x3FU) << SDMMC_CMD_CMDINDEX_Pos);
    int expect_resp = 0;

    if (resp_type == 1) {
        cmd |= SDMMC_CMD_WAITRESP_SHORT;
        expect_resp = 1;
    } else if (resp_type == 2) {
        cmd |= SDMMC_CMD_WAITRESP_LONG;
        expect_resp = 1;
    }
    cmd |= SDMMC_CMD_CPSMEN;

    while (SDMMC2->STA & SDMMC_STA_CMDACT) {}
    SDMMC2->ARG = arg;
    SDMMC2->CMD = cmd;

    int rc = sdmmc_wait_cmd(expect_resp, 200000U);
    if (rc != HAL_SDMMC_OK) {
        sdmmc_clear_flags();
        return rc;
    }

    if (resp) {
        if (resp_type == 2) {
            resp[0] = SDMMC2->RESP4;
            resp[1] = SDMMC2->RESP3;
            resp[2] = SDMMC2->RESP2;
            resp[3] = SDMMC2->RESP1;
        } else {
            *resp = SDMMC2->RESP1;
        }
    }

    sdmmc_clear_flags();
    return HAL_SDMMC_OK;
}

static int sdmmc_send_acmd(uint8_t acmd, uint32_t arg, uint32_t *resp)
{
    uint32_t r;
    int rc = sdmmc_send_cmd_raw(55, ((uint32_t)sd_rca << 16), 1, &r);
    if (rc != HAL_SDMMC_OK)
        return rc;
    return sdmmc_send_cmd_raw(acmd, arg, 1, resp);
}

static void sdmmc_fifo_read(uint8_t *buf, uint32_t words)
{
    uint32_t *p = (uint32_t *)(void *)buf;
    for (uint32_t i = 0; i < words; i++) {
        while (!(SDMMC2->STA & (SDMMC_STA_RXFIFOHF | SDMMC_STA_RXDAVL)))
            ;
        p[i] = SDMMC2->FIFO;
    }
}

static void sdmmc_fifo_write(const uint8_t *buf, uint32_t words)
{
    const uint32_t *p = (const uint32_t *)(const void *)buf;
    for (uint32_t i = 0; i < words; i++) {
        while (!(SDMMC2->STA & SDMMC_STA_TXFIFOHE))
            ;
        SDMMC2->FIFO = p[i];
    }
}

static int sdmmc_setup_data_xfer(uint32_t blocks, int direction_read)
{
    SDMMC2->DTIMER = 0xFFFFFFFFU;
    SDMMC2->DLEN = blocks * HAL_SDMMC_BLOCK_SIZE;
    SDMMC2->DCTRL = SDMMC_DCTRL_DTEN |
                    (direction_read ? SDMMC_DCTRL_DTDIR : 0U) |
                    (9U << SDMMC_DCTRL_DBLOCKSIZE_Pos); /* 512 bytes */
    return HAL_SDMMC_OK;
}

static int sdmmc_read_block(uint32_t lba, uint8_t *buf)
{
    int rc;
    uint32_t addr = sd_high_capacity ? lba : (lba * HAL_SDMMC_BLOCK_SIZE);

    sdmmc_clear_flags();

    rc = sdmmc_setup_data_xfer(1, 1);
    if (rc != HAL_SDMMC_OK)
        return rc;

    rc = sdmmc_send_cmd_raw(17, addr, 1, NULL);
    if (rc != HAL_SDMMC_OK)
        return rc;

    sdmmc_fifo_read(buf, HAL_SDMMC_BLOCK_SIZE / 4U);

    rc = sdmmc_wait_data_end(100000U);
    if (rc != HAL_SDMMC_OK) {
        sdmmc_clear_flags();
        return rc;
    }

    sdmmc_clear_flags();
    scb_inv_dcache(buf, HAL_SDMMC_BLOCK_SIZE);
    return HAL_SDMMC_OK;
}

static int sdmmc_write_block(uint32_t lba, const uint8_t *buf)
{
    int rc;
    uint32_t addr = sd_high_capacity ? lba : (lba * HAL_SDMMC_BLOCK_SIZE);

    scb_clean_dcache(buf, HAL_SDMMC_BLOCK_SIZE);
    sdmmc_clear_flags();

    rc = sdmmc_setup_data_xfer(1, 0);
    if (rc != HAL_SDMMC_OK)
        return rc;

    rc = sdmmc_send_cmd_raw(24, addr, 1, NULL);
    if (rc != HAL_SDMMC_OK)
        return rc;

    sdmmc_fifo_write(buf, HAL_SDMMC_BLOCK_SIZE / 4U);

    rc = sdmmc_wait_data_end(100000U);
    if (rc != HAL_SDMMC_OK) {
        sdmmc_clear_flags();
        return rc;
    }

    sdmmc_clear_flags();
    return HAL_SDMMC_OK;
}

static void sdmmc_gpio_init(void)
{
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOBEN |
                   RCC_AHB1ENR_GPIODEN |
                   RCC_AHB1ENR_GPIOGEN |
                   RCC_AHB1ENR_GPIOIEN;
    (void)RCC_AHB1ENR;

    /* CK: no pull, CMD/D0-D3: pull-up per SD spec */
    hal_gpio_init_af(GPIOD_BASE, 6, 11, GPIO_SPEED_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOD_BASE, 7, 11, GPIO_SPEED_HIGH, GPIO_PUPD_UP);
    hal_gpio_init_af(GPIOG_BASE, 9, 11, GPIO_SPEED_HIGH, GPIO_PUPD_UP);
    hal_gpio_init_af(GPIOG_BASE, 10, 11, GPIO_SPEED_HIGH, GPIO_PUPD_UP);
    hal_gpio_init_af(GPIOB_BASE, 3, 10, GPIO_SPEED_HIGH, GPIO_PUPD_UP);
    hal_gpio_init_af(GPIOB_BASE, 4, 10, GPIO_SPEED_HIGH, GPIO_PUPD_UP);

    /* Card detect: input pull-up, active-low */
    hal_gpio_init_input(GPIOI_BASE, 15, GPIO_PUPD_UP);
}

int hal_sdmmc_card_present(void)
{
    return hal_gpio_read(GPIOI_BASE, 15) == 0;
}

int hal_sdmmc_init(void)
{
    uint32_t resp[4];
    uint32_t ocr;
    int rc;

    if (sd_initialized)
        return HAL_SDMMC_OK;

    sd_rca = 0;
    sd_capacity_blocks = 0;
    sd_high_capacity = 0;

    sdmmc_gpio_init();

    if (!hal_sdmmc_card_present()) {
        hal_uart_puts("[SD] no card\r\n");
        return HAL_SDMMC_NO_CARD;
    }

    /* Select SYSCLK as SDMMC2 clock source. */
    RCC_DCKCFGR2 |= RCC_DCKCFGR2_SDMMC2SEL;

    sdmmc_clock_enable();
    sdmmc_reset();

    /* Slow init clock must be running before power-on. */
    sdmmc_set_clock(SDMMC_CLK_SLOW_DIV);
    sdmmc_power_on();

    /* CMD0: go idle */
    rc = sdmmc_send_cmd_raw(0, 0, 0, NULL);
    if (rc != HAL_SDMMC_OK)
        return rc;

    /* CMD8: interface condition.  Retry a few times for slow cards. */
    int cmd8_ok = 0;
    for (int retry = 0; retry < 3; retry++) {
        rc = sdmmc_send_cmd_raw(8, 0x1AAU, 1, &resp[0]);
        if (rc == HAL_SDMMC_OK && (resp[0] & 0xFFU) == 0xAAU) {
            cmd8_ok = 1;
            break;
        }
        sd_delay(10000);
    }
    (void)cmd8_ok;

    /* ACMD41: send OCR (HCS=1, voltage window 3.2-3.4 V) */
    for (uint32_t i = 0; i < 20000U; i++) {
        rc = sdmmc_send_acmd(41, 0x40300000U, &ocr);
        if (rc != HAL_SDMMC_OK)
            return rc;
        if (ocr & (1U << 31))
            break;
        sd_delay(2000);
    }
    if (!(ocr & (1U << 31)))
        return HAL_SDMMC_TIMEOUT;

    sd_high_capacity = (ocr & (1U << 30)) ? 1 : 0;

    /* CMD2: get CID */
    rc = sdmmc_send_cmd_raw(2, 0, 2, resp);
    if (rc != HAL_SDMMC_OK)
        return rc;

    /* CMD3: get RCA */
    rc = sdmmc_send_cmd_raw(3, 0, 1, &resp[0]);
    if (rc != HAL_SDMMC_OK)
        return rc;
    sd_rca = (uint16_t)(resp[0] >> 16);

    /* CMD9: get CSD */
    rc = sdmmc_send_cmd_raw(9, ((uint32_t)sd_rca << 16), 2, resp);
    if (rc != HAL_SDMMC_OK)
        return rc;

    /* Parse capacity.
     * resp[0]=RESP4, resp[1]=RESP3, resp[2]=RESP2, resp[3]=RESP1.
     * CSD v2.0: C_SIZE = bits 69:48.
     */
    if (sd_high_capacity) {
        uint32_t c_size = ((resp[2] & 0x3FU) << 16) | ((resp[1] >> 16) & 0xFFFFU);
        sd_capacity_blocks = (c_size + 1U) * 1024U;
    } else {
        uint32_t c_size = ((resp[2] & 0xFFU) << 2) | (resp[1] >> 30);
        uint32_t c_size_mult = (resp[1] >> 15) & 0x07U;
        uint32_t read_bl_len = (resp[2] >> 16) & 0x0FU;
        if (read_bl_len >= 9)
            sd_capacity_blocks = (c_size + 1U) << (c_size_mult + read_bl_len - 7U);
        else
            sd_capacity_blocks = (c_size + 1U) >> (7U - read_bl_len - c_size_mult);
    }

    /* CMD7: select card */
    rc = sdmmc_send_cmd_raw(7, ((uint32_t)sd_rca << 16), 1, &resp[0]);
    if (rc != HAL_SDMMC_OK)
        return rc;

    /* CMD16: set block length */
    rc = sdmmc_send_cmd_raw(16, HAL_SDMMC_BLOCK_SIZE, 1, &resp[0]);
    if (rc != HAL_SDMMC_OK)
        return rc;

    /* Switch to 4-bit bus via ACMD6 */
    rc = sdmmc_send_acmd(6, 0x02U, &resp[0]);
    if (rc != HAL_SDMMC_OK)
        return rc;

    SDMMC2->CLKCR |= (2U << SDMMC_CLKCR_WIDBUS_Pos);

    /* Increase clock to fast mode */
    sdmmc_set_clock(SDMMC_CLK_FAST_DIV);

    sd_initialized = 1;
    return HAL_SDMMC_OK;
}

uint32_t hal_sdmmc_get_sector_count(void)
{
    return sd_capacity_blocks;
}

int hal_sdmmc_send_cmd(uint32_t cmd, uint32_t arg, uint32_t *resp)
{
    return sdmmc_send_cmd_raw((uint8_t)cmd, arg, 1, resp);
}

int hal_sdmmc_read_blocks(uint32_t lba, uint8_t *buf, uint32_t count)
{
    if (!sd_initialized)
        return HAL_SDMMC_ERR;

    for (uint32_t i = 0; i < count; i++) {
        int rc = sdmmc_read_block(lba + i, &buf[i * HAL_SDMMC_BLOCK_SIZE]);
        if (rc != HAL_SDMMC_OK)
            return rc;
    }
    return HAL_SDMMC_OK;
}

int hal_sdmmc_write_blocks(uint32_t lba, const uint8_t *buf, uint32_t count)
{
    if (!sd_initialized)
        return HAL_SDMMC_ERR;

    for (uint32_t i = 0; i < count; i++) {
        int rc = sdmmc_write_block(lba + i, &buf[i * HAL_SDMMC_BLOCK_SIZE]);
        if (rc != HAL_SDMMC_OK)
            return rc;
    }
    return HAL_SDMMC_OK;
}
