/*
 * SDMMC2 HAL driver for STM32F769I-DISCO on-board microSD socket.
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
 * 4-bit mode (ACMD6 + CLKCR WIDBUS) is implemented and self-verifying
 * (see sdmmc_set_bus_width_4bit() and HAL_SDMMC_ATTEMPT_4BIT in
 * hal_sdmmc_init()) but disabled by default: PB3/PB4 (D2/D3) share the
 * debug/trace port, and driving them at SDMMC speed measurably degrades
 * RMII Ethernet reliability on this board (confirmed by A/B test --
 * see the comment at HAL_SDMMC_ATTEMPT_4BIT). Stays in 1-bit mode.
 * The driver is polling-only (no DMA).
 */

#include <hal/hal_sdmmc.h>
#include <hal/hal_gpio.h>
#include <hal/board.h>
#include <stdio.h>
#include <string.h>

/* SDMMC2's kernel clock is SYSCLK (SDMMC2SEL reset default = 0 = SYSCLK;
 * nothing in this file selects PLL48CLK instead), which changed from
 * 168 MHz to 216 MHz when hal_sys.c was updated to match Zephyr's SDRAM
 * clock reference (216/168 = 1.2857x).
 *
 * SDIO_CK = SDIOCLK / (CLKDIV + 2) (BYPASS=0, NEGEDGE=0). DIV=7 here
 * targets a spec-compliant ~24 MHz ("Default Speed", which tops out at
 * 25 MHz -- "High Speed", up to 50 MHz, requires first switching the
 * card into it via CMD6, which this driver never does). A higher clock
 * (DIV=2, ~54 MHz -- already out of spec, run unintentionally at one
 * point when SYSCLK moved from 168 to 216 MHz and this divisor was
 * scaled to keep the same SDIO_CK as before rather than checked against
 * spec) is a plausible cause of the SD FIFO simply stopping mid-
 * transfer under real load (confirmed via debugger: a task got stuck
 * forever in sdmmc_fifo_read()'s poll loop -- see that function's
 * bounded-timeout fix above). */
#define SDMMC_CLK_SLOW_DIV  26U   /* was 20 @ 168 MHz */
#define SDMMC_CLK_FAST_DIV  7U    /* ~24 MHz @ 216 MHz -- spec-compliant Default Speed */

#define SD_CMD_ERR_MASK     (SDMMC_STA_CCRCFAIL | SDMMC_STA_CTIMEOUT)
#define SD_DATA_ERR_MASK    (SDMMC_STA_DCRCFAIL | SDMMC_STA_DTIMEOUT | \
                             SDMMC_STA_RXOVERR | SDMMC_STA_TXUNDERR)

static uint16_t sd_rca;
static uint32_t sd_capacity_blocks;
static int sd_initialized;
static int sd_high_capacity;

static void sd_delay(volatile uint32_t n)
{
    while (n--)
        __asm volatile ("nop");
}

static void sd_debug(const char *msg, int rc)
{
    char buf[80];
    if (rc == HAL_SDMMC_OK)
        snprintf(buf, sizeof(buf), "[SD] %s: OK\r\n", msg);
    else if (rc == HAL_SDMMC_TIMEOUT)
        snprintf(buf, sizeof(buf), "[SD] %s: TIMEOUT (STA=0x%08lX)\r\n", msg, (unsigned long)SDMMC2->STA);
    else
        snprintf(buf, sizeof(buf), "[SD] %s: ERR (STA=0x%08lX)\r\n", msg, (unsigned long)SDMMC2->STA);
    hal_uart_puts(buf);
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
    uint32_t clkcr = SDMMC2->CLKCR;
    clkcr &= ~(SDMMC_CLKCR_CLKEN | 0x3FFU);   /* preserve bus width etc. */
    clkcr |= SDMMC_CLKCR_CLKEN | ((div & 0x3FFU) << 0);
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
        if (sta & (SDMMC_STA_DCRCFAIL | SDMMC_STA_TXUNDERR))
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
    int r3_resp = 0;

    if (resp_type == 1) {
        cmd |= SDMMC_CMD_WAITRESP_SHORT;
        expect_resp = 1;
    } else if (resp_type == 2) {
        cmd |= SDMMC_CMD_WAITRESP_LONG;
        expect_resp = 1;
    } else if (resp_type == 3) {
        /* R3 response (ACMD41 OCR): short response format, no CRC.
         * The SDMMC peripheral may raise CCRCFAIL instead of CMDREND
         * because the card does not send a CRC.  Treat either flag as
         * a successfully received response. */
        cmd |= SDMMC_CMD_WAITRESP_SHORT;
        expect_resp = 1;
        r3_resp = 1;
    }
    cmd |= SDMMC_CMD_CPSMEN;

    while (SDMMC2->STA & SDMMC_STA_CMDACT) {}
    SDMMC2->ARG = arg;
    SDMMC2->CMD = cmd;

    int rc;
    if (r3_resp) {
        uint32_t timeout_us = 200000U;
        rc = HAL_SDMMC_TIMEOUT;
        while (timeout_us--) {
            uint32_t sta = SDMMC2->STA;
            if (sta & SDMMC_STA_CTIMEOUT)
                break;
            if ((sta & (SDMMC_STA_CMDREND | SDMMC_STA_CCRCFAIL)) != 0) {
                rc = HAL_SDMMC_OK;
                break;
            }
        }
    } else {
        rc = sdmmc_wait_cmd(expect_resp, 200000U);
    }

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

/*
 * Poll CMD13 (SEND_STATUS) until the card leaves the programming state and
 * returns to the transfer state with READY_FOR_DATA asserted.  This is
 * required after any block write before the next command can be issued.
 */
static int sdmmc_wait_card_ready(uint32_t timeout_us)
{
    uint32_t r;

    while (timeout_us--) {
        int rc = sdmmc_send_cmd_raw(13, ((uint32_t)sd_rca << 16), 1, &r);
        if (rc != HAL_SDMMC_OK)
            return rc;

        /* R1: bits [12:9] = current state, bit 8 = READY_FOR_DATA.
         * State 4 (TRAN) and READY_FOR_DATA means programming is done. */
        if (((r >> 9) & 0xFU) == 4U && (r & (1U << 8)))
            return HAL_SDMMC_OK;

        sd_delay(10);
    }
    return HAL_SDMMC_TIMEOUT;
}

/* ACMD wrapper: CMD55 (APP_CMD) targeting the selected card's RCA,
 * immediately followed by the application command itself -- required
 * for ACMD6 (SET_BUS_WIDTH) below. */
static int sdmmc_send_acmd(uint8_t acmd, uint32_t arg, uint32_t *resp)
{
    uint32_t r;
    int rc = sdmmc_send_cmd_raw(55, ((uint32_t)sd_rca << 16), 1, &r);
    if (rc != HAL_SDMMC_OK)
        return rc;
    return sdmmc_send_cmd_raw(acmd, arg, 1, resp);
}

/* Switch the card + SDMMC2 peripheral to 4-bit bus mode. Must be called
 * with the card already selected (after CMD7) and the bus still in
 * 1-bit mode. ACMD6 argument bits [1:0]: 00 = 1-bit, 10 = 4-bit (bit 1
 * set, bit 0 clear -- i.e. value 2). The card switches essentially
 * immediately on a successful R1 response; only the peripheral side
 * (CLKCR WIDBUS) needs a subsequent register write to match. */
static int sdmmc_set_bus_width_4bit(void)
{
    uint32_t r1;
    int rc = sdmmc_send_acmd(6, 0x2U, &r1);
    if (rc != HAL_SDMMC_OK)
        return rc;
    /* R1 card status: bits [31:24] mostly error flags (OUT_OF_RANGE,
     * ADDRESS_ERROR, ..., COM_CRC_ERROR, ILLEGAL_COMMAND). Anything set
     * there past the fact that sdmmc_send_cmd_raw() already validated
     * the response CRC means the card rejected the switch. */
    if (r1 & 0xFFF80000U)
        return HAL_SDMMC_ERR;

    SDMMC2->CLKCR = (SDMMC2->CLKCR & ~SDMMC_CLKCR_WIDBUS_Msk) | SDMMC_CLKCR_WIDBUS_4BIT;
    sd_delay(1000);
    return HAL_SDMMC_OK;
}

/* Per-word FIFO poll timeout: each word must show RXFIFOHF/RXDAVL (read)
 * or TXFIFOHE (write) within this many polling iterations, or the call
 * fails instead of spinning forever if the card stops responding
 * mid-transfer. hal_sdmmc_read_blocks()/write_blocks() then retry the
 * whole block a few times (see SDMMC_BLOCK_RETRY_MAX below) rather than
 * treating one bad word as fatal.
 *
 * Sized against the ~24 MHz SDIO_CK this driver runs at (see
 * SDMMC_CLK_FAST_DIV above): a word genuinely arriving takes on the
 * order of 1-2 microseconds, so 20000 iterations (order of a few
 * hundred microseconds of CPU-side polling) is already generous
 * headroom, while still failing a truly stuck transfer quickly -- a
 * timeout budget of, say, 100+ ms per word here would make each bad
 * sector expensive enough that a handful of them during one image's
 * worth of row reads could turn a single upload into a multi-minute
 * operation once retries are factored in. */
#define SDMMC_FIFO_WORD_TIMEOUT 20000U

static int sdmmc_fifo_read(uint8_t *buf, uint32_t words)
{
    uint32_t *p = (uint32_t *)(void *)buf;
    for (uint32_t i = 0; i < words; i++) {
        uint32_t timeout = SDMMC_FIFO_WORD_TIMEOUT;
        while (!(SDMMC2->STA & (SDMMC_STA_RXFIFOHF | SDMMC_STA_RXDAVL))) {
            if (--timeout == 0)
                return HAL_SDMMC_TIMEOUT;
        }
        p[i] = SDMMC2->FIFO;
    }
    return HAL_SDMMC_OK;
}

static int sdmmc_fifo_write(const uint8_t *buf, uint32_t words)
{
    const uint32_t *p = (const uint32_t *)(const void *)buf;
    for (uint32_t i = 0; i < words; i++) {
        uint32_t timeout = SDMMC_FIFO_WORD_TIMEOUT;
        while (!(SDMMC2->STA & SDMMC_STA_TXFIFOHE)) {
            if (--timeout == 0)
                return HAL_SDMMC_TIMEOUT;
        }
        SDMMC2->FIFO = p[i];
    }
    return HAL_SDMMC_OK;
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

    rc = sdmmc_fifo_read(buf, HAL_SDMMC_BLOCK_SIZE / 4U);
    if (rc != HAL_SDMMC_OK) {
        sdmmc_clear_flags();
        return rc;
    }

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

    rc = sdmmc_fifo_write(buf, HAL_SDMMC_BLOCK_SIZE / 4U);
    if (rc != HAL_SDMMC_OK) {
        sdmmc_clear_flags();
        return rc;
    }

    rc = sdmmc_wait_data_end(100000U);
    if (rc != HAL_SDMMC_OK) {
        sdmmc_clear_flags();
        return rc;
    }

    /* Wait for the card to finish programming the block before the next
     * command is issued. */
    rc = sdmmc_wait_card_ready(2000000U);
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
    uint32_t r;
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
    sd_debug("CMD0", rc);
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
    sd_debug("CMD8", cmd8_ok ? HAL_SDMMC_OK : HAL_SDMMC_ERR);

    /* ACMD41: send OCR (HCS=1, voltage window 3.2-3.4 V).
     * ACMD41 has an R3 response, so CRC is ignored (resp_type=3). */
    for (uint32_t i = 0; i < 20000U; i++) {
        rc = sdmmc_send_cmd_raw(55, ((uint32_t)sd_rca << 16), 1, &r);
        if (rc != HAL_SDMMC_OK) {
            sd_debug("CMD55", rc);
            return rc;
        }
        rc = sdmmc_send_cmd_raw(41, 0x40300000U, 3, &ocr);
        if (rc != HAL_SDMMC_OK) {
            sd_debug("ACMD41", rc);
            return rc;
        }
        if (ocr & (1U << 31))
            break;
        sd_delay(2000);
    }
    if (!(ocr & (1U << 31))) {
        sd_debug("ACMD41 ready", HAL_SDMMC_TIMEOUT);
        return HAL_SDMMC_TIMEOUT;
    }
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "[SD] OCR=0x%08lX HC=%d\r\n", (unsigned long)ocr, (ocr & (1U << 30)) ? 1 : 0);
        hal_uart_puts(buf);
    }
    sd_debug("ACMD41", HAL_SDMMC_OK);

    sd_high_capacity = (ocr & (1U << 30)) ? 1 : 0;

    /* CMD2: get CID */
    rc = sdmmc_send_cmd_raw(2, 0, 2, resp);
    sd_debug("CMD2", rc);
    if (rc != HAL_SDMMC_OK)
        return rc;

    /* CMD3: get RCA */
    rc = sdmmc_send_cmd_raw(3, 0, 1, &resp[0]);
    sd_debug("CMD3", rc);
    if (rc != HAL_SDMMC_OK)
        return rc;
    sd_rca = (uint16_t)(resp[0] >> 16);

    /* CMD9: get CSD */
    rc = sdmmc_send_cmd_raw(9, ((uint32_t)sd_rca << 16), 2, resp);
    sd_debug("CMD9", rc);
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
    sd_debug("CMD7", rc);
    if (rc != HAL_SDMMC_OK)
        return rc;

    /* CMD16: set block length */
    rc = sdmmc_send_cmd_raw(16, HAL_SDMMC_BLOCK_SIZE, 1, &resp[0]);
    sd_debug("CMD16", rc);
    if (rc != HAL_SDMMC_OK)
        return rc;

    /* 4-bit bus (ACMD6 + CLKCR WIDBUS) is implemented and does work on
     * its own terms -- sdmmc_set_bus_width_4bit() + a real block-0 read
     * (checking the AA55 boot-sector signature) confirm the SD side is
     * completely fine in 4-bit mode. The reason it's disabled by
     * default is a different, unexpected finding: PB3/PB4 (D2/D3) are
     * shared with the debug/trace port, and driving them at SDMMC
     * speed measurably degrades RMII Ethernet reliability on this
     * board -- confirmed by direct A/B test (see git history for this
     * comment): with HAL_SDMMC_ATTEMPT_4BIT on, DHCP got stuck in
     * SELECTING with the RX path throwing errors on every single
     * frame, surviving even a full power cycle; flipping this back to
     * 0 (1-bit) with no other change and reflashing fixed it
     * immediately, no power cycle needed. Since apps/drop-a-file (an
     * HTTP upload app) needs networking far more than it needs faster
     * SD writes, 1-bit stays the default. Flip this on only for a
     * board/app combination that doesn't need Ethernet at the same
     * time as the SD card. */
#define HAL_SDMMC_ATTEMPT_4BIT 0
#if HAL_SDMMC_ATTEMPT_4BIT
    rc = sdmmc_set_bus_width_4bit();
#else
    rc = HAL_SDMMC_ERR;
    (void)sdmmc_set_bus_width_4bit;
#endif
    if (rc == HAL_SDMMC_OK) {
        sdmmc_set_clock(SDMMC_CLK_FAST_DIV);

        uint8_t probe[HAL_SDMMC_BLOCK_SIZE];
        rc = sdmmc_read_block(0, probe);
        if (rc == HAL_SDMMC_OK && probe[510] == 0x55U && probe[511] == 0xAAU) {
            hal_uart_puts("[SD] init complete (4-bit fast, LBA0 sig verified)\r\n");
            sd_initialized = 1;
            return HAL_SDMMC_OK;
        }

        hal_uart_puts("[SD] 4-bit mode LBA0 verify failed -- falling back to 1-bit\r\n");
        SDMMC2->CLKCR = (SDMMC2->CLKCR & ~SDMMC_CLKCR_WIDBUS_Msk) | SDMMC_CLKCR_WIDBUS_1BIT;
        (void)sdmmc_send_acmd(6, 0x0U, &r); /* tell the card to go back to 1-bit too */
    } else {
        hal_uart_puts("[SD] ACMD6 (4-bit) rejected -- staying in 1-bit\r\n");
    }

    /* Raise clock to fast speed in 1-bit bus mode (either 4-bit was
     * never attempted successfully, or it failed verification above). */
    sdmmc_set_clock(SDMMC_CLK_FAST_DIV);
    hal_uart_puts("[SD] init complete (1-bit fast)\r\n");
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

/* Some sectors -- observed repeatedly and specifically at a partition's
 * first LBA right after a fresh format (a never-written region from the
 * card's own flash-translation-layer perspective) -- time out on the
 * very first attempt but succeed immediately on a retry. A handful of
 * retries with a short backoff turns that into a non-issue instead of
 * a hard read/write failure (which, for a read, means f_mount() itself
 * fails with FR_DISK_ERR/FR_NO_FILESYSTEM even though the volume is
 * completely fine). */
#define SDMMC_BLOCK_RETRY_MAX 5U

int hal_sdmmc_read_blocks(uint32_t lba, uint8_t *buf, uint32_t count)
{
    if (!sd_initialized)
        return HAL_SDMMC_ERR;

    for (uint32_t i = 0; i < count; i++) {
        int rc = HAL_SDMMC_ERR;
        for (uint32_t attempt = 0; attempt < SDMMC_BLOCK_RETRY_MAX; attempt++) {
            rc = sdmmc_read_block(lba + i, &buf[i * HAL_SDMMC_BLOCK_SIZE]);
            if (rc == HAL_SDMMC_OK)
                break;
            sd_delay(50000 * (attempt + 1));
        }
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
        int rc = HAL_SDMMC_ERR;
        for (uint32_t attempt = 0; attempt < SDMMC_BLOCK_RETRY_MAX; attempt++) {
            rc = sdmmc_write_block(lba + i, &buf[i * HAL_SDMMC_BLOCK_SIZE]);
            if (rc == HAL_SDMMC_OK)
                break;
            sd_delay(50000 * (attempt + 1));
        }
        if (rc != HAL_SDMMC_OK)
            return rc;
    }
    return HAL_SDMMC_OK;
}
