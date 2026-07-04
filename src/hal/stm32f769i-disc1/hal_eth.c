#include <hal/hal_eth.h>
#include <hal/hal_gpio.h>
#include <string.h>

/* RMII pin mapping for STM32F769I-DISC1 (LAN8742A):
 * PA1  -> REF_CLK   (AF11)
 * PA2  -> MDIO      (AF11)
 * PA7  -> CRS_DV    (AF11)
 * PB13 -> TXD1      (AF11)
 * PC1  -> MDC       (AF11)
 * PC4  -> RXD0      (AF11)
 * PC5  -> RXD1      (AF11)
 * PG13 -> TXD0      (AF11)
 * PG14 -> TX_EN     (AF11)
 * PH3  -> INT       (AF11) - optional
 * PI10 -> RX_ER     (AF11) - optional
 */

static eth_dma_desc_t tx_desc[HAL_ETH_TX_DESC_COUNT] __attribute__((aligned(4)));
static eth_dma_desc_t rx_desc[HAL_ETH_RX_DESC_COUNT] __attribute__((aligned(4)));
static uint8_t tx_buf[HAL_ETH_TX_DESC_COUNT][HAL_ETH_BUF_SIZE] __attribute__((aligned(32)));
static uint8_t rx_buf[HAL_ETH_RX_DESC_COUNT][HAL_ETH_BUF_SIZE] __attribute__((aligned(32)));

static volatile uint32_t tx_idx;
static volatile uint32_t rx_idx;
static volatile int eth_link_up;
static uint8_t eth_mac_addr[6];

/* PHY registers (IEEE + LAN8742A) */
#define PHY_BCR         0
#define PHY_BSR         1
#define PHY_ID1         2
#define PHY_ID2         3
#define PHY_ANAR        4
#define PHY_ANLPAR      5
#define PHY_ANER        6
#define PHY_SCSR        31    /* LAN8742A special control/status */

#define PHY_BCR_RESET       (1U << 15)
#define PHY_BCR_ANEG_EN     (1U << 12)
#define PHY_BCR_ANEG_RST    (1U << 9)
#define PHY_BCR_FULLD       (1U << 8)
#define PHY_BCR_SPEED_100   (1U << 13)

#define PHY_BSR_LINK_UP     (1U << 2)
#define PHY_BSR_ANEG_CMPLT  (1U << 5)

#define PHY_SCSR_SPEED_Pos  3
#define PHY_SCSR_DUPLEX     (1U << 2)

static void eth_delay(volatile uint32_t n)
{
    while (n--) {
        __asm volatile ("nop");
    }
}

static void smii_write(uint32_t phy, uint32_t reg, uint16_t val)
{
    while (ETH->MIIAR & ETH_MACMIIAR_MB)
        ;
    ETH->MIIDR = val;
    ETH->MIIAR = (phy << ETH_MACMIIAR_PA_Pos) |
                 (reg << ETH_MACMIIAR_MR_Pos) |
                 ETH_MACMIIAR_CR_DIV16 |
                 ETH_MACMIIAR_MW |
                 ETH_MACMIIAR_MB;
    while (ETH->MIIAR & ETH_MACMIIAR_MB)
        ;
}

static uint16_t smii_read(uint32_t phy, uint32_t reg)
{
    while (ETH->MIIAR & ETH_MACMIIAR_MB)
        ;
    ETH->MIIAR = (phy << ETH_MACMIIAR_PA_Pos) |
                 (reg << ETH_MACMIIAR_MR_Pos) |
                 ETH_MACMIIAR_CR_DIV16 |
                 ETH_MACMIIAR_MB;
    while (ETH->MIIAR & ETH_MACMIIAR_MB)
        ;
    return (uint16_t)ETH->MIIDR;
}

static void configure_rmii_pins(void)
{
    /* Enable all GPIO clocks used by RMII */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN |
                   RCC_AHB1ENR_GPIOBEN |
                   RCC_AHB1ENR_GPIOCEN |
                   RCC_AHB1ENR_GPIOGEN;

    /* PA1 REF_CLK, PA2 MDIO, PA7 CRS_DV */
    hal_gpio_init_af(GPIOA_BASE, 1, 11, GPIO_SPEED_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOA_BASE, 2, 11, GPIO_SPEED_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOA_BASE, 7, 11, GPIO_SPEED_HIGH, GPIO_PUPD_NONE);

    /* PB13 TXD1 */
    hal_gpio_init_af(GPIOB_BASE, 13, 11, GPIO_SPEED_HIGH, GPIO_PUPD_NONE);

    /* PC1 MDC, PC4 RXD0, PC5 RXD1 */
    hal_gpio_init_af(GPIOC_BASE, 1, 11, GPIO_SPEED_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOC_BASE, 4, 11, GPIO_SPEED_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOC_BASE, 5, 11, GPIO_SPEED_HIGH, GPIO_PUPD_NONE);

    /* PG13 TXD0, PG14 TX_EN */
    hal_gpio_init_af(GPIOG_BASE, 13, 11, GPIO_SPEED_HIGH, GPIO_PUPD_NONE);
    hal_gpio_init_af(GPIOG_BASE, 14, 11, GPIO_SPEED_HIGH, GPIO_PUPD_NONE);
}

static void set_mac_address(const uint8_t *mac)
{
    ETH->A0HR = ((uint32_t)mac[5] << 8) | (uint32_t)mac[4];
    ETH->A0LR = ((uint32_t)mac[3] << 24) |
                ((uint32_t)mac[2] << 16) |
                ((uint32_t)mac[1] << 8)  |
                ((uint32_t)mac[0]);
}

static void init_descriptor_rings(void)
{
    uint32_t i;

    tx_idx = 0;
    rx_idx = 0;

    for (i = 0; i < HAL_ETH_TX_DESC_COUNT; i++) {
        tx_desc[i].status = ETH_TDES0_TCH;
        tx_desc[i].control = 0;
        tx_desc[i].buf1 = (uint32_t)(uintptr_t)tx_buf[i];
        tx_desc[i].next = &tx_desc[(i + 1) % HAL_ETH_TX_DESC_COUNT];
    }

    for (i = 0; i < HAL_ETH_RX_DESC_COUNT; i++) {
        rx_desc[i].status = ETH_RDES0_OWN;
        rx_desc[i].control = (HAL_ETH_BUF_SIZE << ETH_RDES1_RBS1_Pos) | ETH_RDES1_RCH;
        rx_desc[i].buf1 = (uint32_t)(uintptr_t)rx_buf[i];
        rx_desc[i].next = &rx_desc[(i + 1) % HAL_ETH_RX_DESC_COUNT];
    }

    /* Mark last RX descriptor? In chained mode RER is not needed for circular */
    rx_desc[HAL_ETH_RX_DESC_COUNT - 1].control |= ETH_RDES1_RER;

    ETH->DMATDLAR = (uint32_t)&tx_desc[0];
    ETH->DMARDLAR = (uint32_t)&rx_desc[0];
}

static int phy_init(void)
{
    uint16_t bcr;
    uint16_t bsr;
    uint32_t timeout;

    /* Reset PHY */
    smii_write(LAN8742A_PHY_ADDR, PHY_BCR, PHY_BCR_RESET);
    timeout = 100000;
    while (timeout--) {
        if (!(smii_read(LAN8742A_PHY_ADDR, PHY_BCR) & PHY_BCR_RESET))
            break;
    }
    if (timeout == 0)
        return -1;

    /* Enable auto-negotiation */
    bcr = PHY_BCR_ANEG_EN;
    smii_write(LAN8742A_PHY_ADDR, PHY_BCR, bcr);
    smii_write(LAN8742A_PHY_ADDR, PHY_BCR, bcr | PHY_BCR_ANEG_RST);

    /* Wait for link up and auto-negotiation complete */
    timeout = 1000000;
    while (timeout--) {
        bsr = smii_read(LAN8742A_PHY_ADDR, PHY_BSR);
        if ((bsr & (PHY_BSR_LINK_UP | PHY_BSR_ANEG_CMPLT)) ==
            (PHY_BSR_LINK_UP | PHY_BSR_ANEG_CMPLT)) {
            break;
        }
    }
    if (timeout == 0)
        return -2;

    eth_link_up = 1;
    return 0;
}

static void read_phy_speed_duplex(void)
{
    uint16_t scsr = smii_read(LAN8742A_PHY_ADDR, PHY_SCSR);
    int speed100 = ((scsr >> PHY_SCSR_SPEED_Pos) & 3U) >= 2U;
    int full_duplex = (scsr & PHY_SCSR_DUPLEX) ? 1 : 0;

    uint32_t maccr = ETH->CR;
    maccr &= ~(ETH_MACCR_FES | ETH_MACCR_DM);
    if (speed100)
        maccr |= ETH_MACCR_FES;
    if (full_duplex)
        maccr |= ETH_MACCR_DM;
    ETH->CR = maccr;
}

int hal_eth_init(const uint8_t *mac_addr)
{
    uint32_t timeout;

    if (mac_addr) {
        for (int i = 0; i < 6; i++)
            eth_mac_addr[i] = mac_addr[i];
    } else {
        /* default MAC 02:00:00:00:00:01 */
        eth_mac_addr[0] = 0x02;
        eth_mac_addr[1] = 0x00;
        eth_mac_addr[2] = 0x00;
        eth_mac_addr[3] = 0x00;
        eth_mac_addr[4] = 0x00;
        eth_mac_addr[5] = 0x01;
    }

    /* Enable SYSCFG and Ethernet clocks */
    RCC_APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    RCC_AHB1ENR |= RCC_AHB1ENR_ETHMACEN |
                   RCC_AHB1ENR_ETHMACTXEN |
                   RCC_AHB1ENR_ETHMACRXEN;

    /* Select RMII mode */
    SYSCFG_PMC |= SYSCFG_PMC_MII_RMII_SEL;

    /* Configure RMII pins */
    configure_rmii_pins();
    eth_delay(1000);

    /* Reset Ethernet DMA */
    ETH->DMABMR |= ETH_DMABMR_SR;
    timeout = 100000;
    while ((ETH->DMABMR & ETH_DMABMR_SR) && timeout--)
        ;
    if (timeout == 0)
        return -1;

    /* Reset Ethernet MAC */
    ETH->CR = 0;

    /* MAC configuration: checksum offload, automatic pad/CRC stripping,
     * inter-frame gap 96 bits. Speed/duplex applied after PHY negotiation. */
    ETH->CR = ETH_MACCR_APCS |     /* automatic pad/CRC strip */
              ETH_MACCR_IPCO |     /* IPv4 checksum offload */
              ETH_MACCR_IFG_96;

    /* Frame filter: pass all multicast + broadcast implicitly + perfect */
    ETH->FFR = ETH_MACFFR_PM;

    /* Flow control off */
    ETH->FCR = 0;

    /* Set MAC address */
    set_mac_address(eth_mac_addr);

    /* Initialize descriptor rings */
    init_descriptor_rings();

    /* DMA bus mode */
    ETH->DMABMR = ETH_DMABMR_AAB |  /* address-aligned beats */
                  ETH_DMABMR_FB  |  /* fixed burst */
                  (32U << ETH_DMABMR_PBL_Pos); /* programmable burst length */

    /* DMA operation mode: store-and-forward, thresholds */
    ETH->DMAOMR = ETH_DMAOMR_ST | ETH_DMAOMR_SR |
                  ETH_DMAOMR_FEF | ETH_DMAOMR_FUGF |
                  (0U << ETH_DMAOMR_RTC_Pos);

    /* DMA interrupts (optional) */
    ETH->DMAIER = ETH_DMAIER_NISE | ETH_DMAIER_RIE;

    /* Enable MAC receiver/transmitter after PHY is ready */
    ETH->CR |= ETH_MACCR_RE | ETH_MACCR_TE;

    /* Initialize PHY and read negotiated speed/duplex */
    if (phy_init() != 0)
        return -2;
    read_phy_speed_duplex();

    /* Resume DMA receive */
    ETH->DMARPDR = 0;

    return 0;
}

int hal_eth_link_up(void)
{
    uint16_t bsr = smii_read(LAN8742A_PHY_ADDR, PHY_BSR);
    int up = (bsr & PHY_BSR_LINK_UP) ? 1 : 0;
    eth_link_up = up;
    return up;
}

void hal_eth_poll(void)
{
    /* Update link status periodically */
    static uint32_t poll_count = 0;
    if (++poll_count > 100000) {
        poll_count = 0;
        int was_up = eth_link_up;
        int up = hal_eth_link_up();
        if (up && !was_up)
            read_phy_speed_duplex();
    }
}

int hal_eth_tx(const uint8_t *buf, size_t len)
{
    if (len == 0 || len > HAL_ETH_BUF_SIZE)
        return -1;

    eth_dma_desc_t *desc = &tx_desc[tx_idx];

    /* Wait for previous transmission on this descriptor to complete */
    uint32_t timeout = 100000;
    while ((desc->status & ETH_TDES0_OWN) && timeout--)
        ;
    if (timeout == 0)
        return -2;

    /* Copy payload into descriptor buffer and clean cache */
    memcpy((void *)desc->buf1, buf, len);
    scb_clean_dcache((const void *)desc->buf1, (uint32_t)len);

    /* Build control word */
    desc->control = (uint32_t)len;
    desc->status = ETH_TDES0_OWN | ETH_TDES0_FS | ETH_TDES0_LS |
                   ETH_TDES0_TCH | ETH_TDES0_IC;

    __asm volatile ("dsb" ::: "memory");

    /* Move to next descriptor */
    tx_idx = (tx_idx + 1) % HAL_ETH_TX_DESC_COUNT;

    /* Resume DMA transmission */
    ETH->DMATPDR = 0;

    return 0;
}

int hal_eth_rx(uint8_t *buf, size_t max_len, size_t *out_len)
{
    eth_dma_desc_t *desc = &rx_desc[rx_idx];

    if (desc->status & ETH_RDES0_OWN)
        return 0; /* no packet available */

    uint32_t status = desc->status;
    if (status & ETH_RDES0_ES) {
        /* Error: re-own descriptor */
        desc->status = ETH_RDES0_OWN;
        rx_idx = (rx_idx + 1) % HAL_ETH_RX_DESC_COUNT;
        return -1;
    }

    if (!((status & ETH_RDES0_FS) && (status & ETH_RDES0_LS))) {
        /* Fragmented/chained packet not supported in this minimal driver */
        desc->status = ETH_RDES0_OWN;
        rx_idx = (rx_idx + 1) % HAL_ETH_RX_DESC_COUNT;
        return -2;
    }

    uint32_t len = (status >> ETH_RDES0_FL_Pos) & ETH_RDES0_FL_Msk;
    if (len > max_len)
        len = (uint32_t)max_len;

    /* Invalidate cache before reading DMA buffer */
    scb_inv_dcache((void *)desc->buf1, len);
    memcpy(buf, (const void *)desc->buf1, len);

    if (out_len)
        *out_len = (size_t)len;

    /* Re-own descriptor */
    desc->status = ETH_RDES0_OWN;
    rx_idx = (rx_idx + 1) % HAL_ETH_RX_DESC_COUNT;

    /* Resume DMA receive in case it suspended */
    ETH->DMARPDR = 0;

    return 1;
}
