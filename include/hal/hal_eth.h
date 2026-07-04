#ifndef HAL_ETH_H
#define HAL_ETH_H

#include <hal/stm32f769i-disc1.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_ETH_TX_DESC_COUNT   4
#define HAL_ETH_RX_DESC_COUNT   8
#define HAL_ETH_BUF_SIZE        1524

#define LAN8742A_PHY_ADDR       0

/* --- Ethernet DMA descriptor --- */
typedef struct eth_dma_desc {
    volatile uint32_t status;
    volatile uint32_t control;
    volatile uint32_t buf1;
    struct eth_dma_desc *next;
} eth_dma_desc_t;

/* --- Ethernet register base --- */
#define ETH_BASE        0x40028000U

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t FFR;
    volatile uint32_t FCR;
    volatile uint32_t RESERVED0;
    volatile uint32_t MIIAR;
    volatile uint32_t MIIDR;
    volatile uint32_t RESERVED1[10];
    volatile uint32_t A0HR;
    volatile uint32_t A0LR;
    volatile uint32_t RESERVED2[1010]; /* pad to 0x1000 */
    volatile uint32_t DMABMR;
    volatile uint32_t DMATPDR;
    volatile uint32_t DMARPDR;
    volatile uint32_t DMARDLAR;
    volatile uint32_t DMATDLAR;
    volatile uint32_t DMASR;
    volatile uint32_t DMAOMR;
    volatile uint32_t DMAIER;
} ETH_TypeDef;

#define ETH             ((ETH_TypeDef *)ETH_BASE)

/* MAC control register */
#define ETH_MACCR_RE        (1U << 2)
#define ETH_MACCR_TE        (1U << 3)
#define ETH_MACCR_APCS      (1U << 7)
#define ETH_MACCR_IPCO      (1U << 10)
#define ETH_MACCR_DM        (1U << 11)
#define ETH_MACCR_FES       (1U << 14)
#define ETH_MACCR_IFG_96    (0U << 17)

/* MAC frame filter register */
#define ETH_MACFFR_PM       (1U << 0)

/* MAC MII address register */
#define ETH_MACMIIAR_MB     (1U << 0)
#define ETH_MACMIIAR_MW     (1U << 1)
#define ETH_MACMIIAR_CR_DIV16 (2U << 2)
#define ETH_MACMIIAR_MR_Pos 6U
#define ETH_MACMIIAR_PA_Pos 11U

/* DMA bus mode register */
#define ETH_DMABMR_SR       (1U << 0)
#define ETH_DMABMR_FB       (1U << 16)
#define ETH_DMABMR_PBL_Pos  8U
#define ETH_DMABMR_AAB      (1U << 25)

/* DMA operation mode register */
#define ETH_DMAOMR_SR       (1U << 1)
#define ETH_DMAOMR_RTC_Pos  3U
#define ETH_DMAOMR_FUGF     (1U << 6)
#define ETH_DMAOMR_FEF      (1U << 7)
#define ETH_DMAOMR_ST       (1U << 13)

/* DMA interrupt enable register */
#define ETH_DMAIER_RIE      (1U << 6)
#define ETH_DMAIER_NISE     (1U << 16)

/* TX descriptor status */
#define ETH_TDES0_OWN       (1U << 31)
#define ETH_TDES0_IC        (1U << 30)
#define ETH_TDES0_LS        (1U << 29)
#define ETH_TDES0_FS        (1U << 28)
#define ETH_TDES0_TCH       (1U << 20)

/* RX descriptor status */
#define ETH_RDES0_OWN       (1U << 31)
#define ETH_RDES0_FL_Pos    16U
#define ETH_RDES0_FL_Msk    0x3FFFU
#define ETH_RDES0_ES        (1U << 15)
#define ETH_RDES0_FS        (1U << 9)
#define ETH_RDES0_LS        (1U << 8)

/* RX descriptor control */
#define ETH_RDES1_RCH       (1U << 14)
#define ETH_RDES1_RER       (1U << 15)
#define ETH_RDES1_RBS1_Pos  0U

/* Ethernet HAL API */
int  hal_eth_init(const uint8_t *mac_addr);
int  hal_eth_link_up(void);
void hal_eth_poll(void);
int  hal_eth_tx(const uint8_t *buf, size_t len);
int  hal_eth_rx(uint8_t *buf, size_t max_len, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* HAL_ETH_H */
