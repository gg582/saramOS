/*
 * HAL display initialization for STM32F769I-DISCO.
 *
 * This is a register-level implementation that follows the STM32CubeF7 BSP
 * and the Zephyr STM32 LTDC/DSI driver bring-up sequence for the on-board
 * OTM8009A 800x480 landscape DSI LCD.
 *
 * The system is running from the 16 MHz HSI, so the display-specific clocks
 * are generated as follows:
 *   - HSE (25 MHz crystal) is enabled to clock the DSI PLL and PLLSAI.
 *   - DSI PLL: 25 MHz / 5 * 100 / 1 = 500 MHz -> byte clock = 62.5 MHz.
 *   - PLLSAI: 25 MHz / 25 * 384 / 7 / 2 = 27.429 MHz -> LTDC pixel clock.
 */
#include "hal_display.h"
#include "hal_sdram.h"
#include <hal/board.h>
#include <hal/hal_gpio.h>
#include <string.h>

extern void hal_uart_puts(const char *s);

/* --- Extra RCC definitions needed for the display clocks --- */
#define RCC_PLLSAICFGR      (*(volatile uint32_t *)(RCC_BASE + 0x88U))
#define RCC_DCKCFGR1        (*(volatile uint32_t *)(RCC_BASE + 0x8CU))
#define RCC_CR_HSEON        (1U << 16)
#define RCC_CR_HSERDY       (1U << 17)
#define RCC_CR_PLLSAION     (1U << 28)
#define RCC_CR_PLLSAIRDY    (1U << 29)
#define RCC_PLLCFGR_PLLSRC  (1U << 22)
#define RCC_PLLCFGR_PLLM_Pos 0U
#define RCC_PLLCFGR_PLLM_Msk (0x3FUL << RCC_PLLCFGR_PLLM_Pos)
#define RCC_PLLSAICFGR_PLLSAIN_Pos 6U
#define RCC_PLLSAICFGR_PLLSAIN_Msk (0x1FFUL << RCC_PLLSAICFGR_PLLSAIN_Pos)
#define RCC_PLLSAICFGR_PLLSAIR_Pos 28U
#define RCC_PLLSAICFGR_PLLSAIR_Msk (0x7UL << RCC_PLLSAICFGR_PLLSAIR_Pos)
#define RCC_DCKCFGR1_PLLSAIDIVR_Pos 16U
#define RCC_DCKCFGR1_PLLSAIDIVR_Msk (0x3UL << RCC_DCKCFGR1_PLLSAIDIVR_Pos)

/* --- DSI Host / Wrapper registers (CMSIS layout) --- */
#define DSI_BASE            0x40016C00U

typedef struct {
    volatile uint32_t VR;
    volatile uint32_t CR;
    volatile uint32_t CCR;
    volatile uint32_t LVCIDR;
    volatile uint32_t LCOLCR;
    volatile uint32_t LPCR;
    volatile uint32_t LPMCR;
    uint32_t      RESERVED0[4];
    volatile uint32_t PCR;
    volatile uint32_t GVCIDR;
    volatile uint32_t MCR;
    volatile uint32_t VMCR;
    volatile uint32_t VPCR;
    volatile uint32_t VCCR;
    volatile uint32_t VNPCR;
    volatile uint32_t VHSACR;
    volatile uint32_t VHBPCR;
    volatile uint32_t VLCR;
    volatile uint32_t VVSACR;
    volatile uint32_t VVBPCR;
    volatile uint32_t VVFPCR;
    volatile uint32_t VVACR;
    volatile uint32_t LCCR;
    volatile uint32_t CMCR;
    volatile uint32_t GHCR;
    volatile uint32_t GPDR;
    volatile uint32_t GPSR;
    volatile uint32_t TCCR[6];
    volatile uint32_t TDCR;
    volatile uint32_t CLCR;
    volatile uint32_t CLTCR;
    volatile uint32_t DLTCR;
    volatile uint32_t PCTLR;
    volatile uint32_t PCONFR;
    volatile uint32_t PUCR;
    volatile uint32_t PTTCR;
    volatile uint32_t PSR;
    uint32_t      RESERVED1[2];
    volatile uint32_t ISR[2];
    volatile uint32_t IER[2];
    uint32_t      RESERVED2[3];
    volatile uint32_t FIR[2];
    uint32_t      RESERVED3[8];
    volatile uint32_t VSCR;
    uint32_t      RESERVED4[2];
    volatile uint32_t LCVCIDR;
    volatile uint32_t LCCCR;
    uint32_t      RESERVED5;
    volatile uint32_t LPMCCR;
    uint32_t      RESERVED6[7];
    volatile uint32_t VMCCR;
    volatile uint32_t VPCCR;
    volatile uint32_t VCCCR;
    volatile uint32_t VNPCCR;
    volatile uint32_t VHSACCR;
    volatile uint32_t VHBPCCR;
    volatile uint32_t VLCCR;
    volatile uint32_t VVSACCR;
    volatile uint32_t VVBPCCR;
    volatile uint32_t VVFPCCR;
    volatile uint32_t VVACCR;
    uint32_t      RESERVED7[11];
    volatile uint32_t TDCCR;
    uint32_t      RESERVED8[155];
    volatile uint32_t WCFGR;
    volatile uint32_t WCR;
    volatile uint32_t WIER;
    volatile uint32_t WISR;
    volatile uint32_t WIFCR;
    uint32_t      RESERVED9;
    volatile uint32_t WPCR[5];
    uint32_t      RESERVED10;
    volatile uint32_t WRPCR;
} DSI_TypeDef;

#define DSI             ((DSI_TypeDef *)DSI_BASE)

/* DSI bit helpers */
#define DSI_CR_EN_Pos               0U
#define DSI_CR_EN_Msk               (1U << DSI_CR_EN_Pos)
#define DSI_CR_EN                   DSI_CR_EN_Msk
#define DSI_WCR_DSIEN_Pos           3U
#define DSI_WCR_DSIEN_Msk           (1U << DSI_WCR_DSIEN_Pos)
#define DSI_WCR_DSIEN               DSI_WCR_DSIEN_Msk
#define DSI_WCFGR_DSIM_Pos          0U
#define DSI_WCFGR_DSIM_Msk          (1U << DSI_WCFGR_DSIM_Pos)
#define DSI_WCFGR_DSIM              DSI_WCFGR_DSIM_Msk
#define DSI_WCFGR_COLMUX_Pos        1U
#define DSI_WCFGR_COLMUX_Msk        (0x7UL << DSI_WCFGR_COLMUX_Pos)
#define DSI_WCFGR_VSPOL_Pos         7U
#define DSI_WCFGR_VSPOL             (1U << DSI_WCFGR_VSPOL_Pos)
#define DSI_WCFGR_HSPOL_Pos         6U
#define DSI_WCFGR_HSPOL             (1U << DSI_WCFGR_HSPOL_Pos)
#define DSI_WCFGR_DEPOL_Pos         5U
#define DSI_WCFGR_DEPOL             (1U << DSI_WCFGR_DEPOL_Pos)
#define DSI_LCOLCR_COLC_Pos         0U
#define DSI_LCOLCR_COLC_Msk         (0xFUL << DSI_LCOLCR_COLC_Pos)
#define DSI_LCOLCR_LPE_Pos          8U
#define DSI_LCOLCR_LPE_Msk          (1U << DSI_LCOLCR_LPE_Pos)
#define DSI_LPCR_DEP_Pos            0U
#define DSI_LPCR_DEP_Msk            (1U << DSI_LPCR_DEP_Pos)
#define DSI_LPCR_DEP                DSI_LPCR_DEP_Msk
#define DSI_LPCR_VSP_Pos            1U
#define DSI_LPCR_VSP_Msk            (1U << DSI_LPCR_VSP_Pos)
#define DSI_LPCR_VSP                DSI_LPCR_VSP_Msk
#define DSI_LPCR_HSP_Pos            2U
#define DSI_LPCR_HSP_Msk            (1U << DSI_LPCR_HSP_Pos)
#define DSI_LPCR_HSP                DSI_LPCR_HSP_Msk
#define DSI_VMCR_VMT_Pos            0U
#define DSI_VMCR_VMT_Msk            (0x3UL << DSI_VMCR_VMT_Pos)
#define DSI_VMCR_LPCE_Pos           15U
#define DSI_VMCR_LPCE_Msk           (1U << DSI_VMCR_LPCE_Pos)
#define DSI_VMCR_FBTAAE_Pos         14U
#define DSI_VMCR_FBTAAE_Msk         (1U << DSI_VMCR_FBTAAE_Pos)
#define DSI_VMCR_LPHFPE_Pos         13U
#define DSI_VMCR_LPHFPE_Msk         (1U << DSI_VMCR_LPHFPE_Pos)
#define DSI_VMCR_LPHBPE_Pos         12U
#define DSI_VMCR_LPHBPE_Msk         (1U << DSI_VMCR_LPHBPE_Pos)
#define DSI_VMCR_LPVAE_Pos          11U
#define DSI_VMCR_LPVAE_Msk          (1U << DSI_VMCR_LPVAE_Pos)
#define DSI_VMCR_LPVFPE_Pos         10U
#define DSI_VMCR_LPVFPE_Msk         (1U << DSI_VMCR_LPVFPE_Pos)
#define DSI_VMCR_LPVBPE_Pos         9U
#define DSI_VMCR_LPVBPE_Msk         (1U << DSI_VMCR_LPVBPE_Pos)
#define DSI_VMCR_LPVSAE_Pos         8U
#define DSI_VMCR_LPVSAE_Msk         (1U << DSI_VMCR_LPVSAE_Pos)
#define DSI_VPCR_VPSIZE_Pos         0U
#define DSI_VPCR_VPSIZE_Msk         (0x3FFFUL << DSI_VPCR_VPSIZE_Pos)
#define DSI_VCCR_NUMC_Pos           0U
#define DSI_VCCR_NUMC_Msk           (0x1FFFUL << DSI_VCCR_NUMC_Pos)
#define DSI_VNPCR_NPSIZE_Pos        0U
#define DSI_VNPCR_NPSIZE_Msk        (0x1FFFUL << DSI_VNPCR_NPSIZE_Pos)
#define DSI_VHSACR_HSA_Pos          0U
#define DSI_VHSACR_HSA_Msk          (0xFFFUL << DSI_VHSACR_HSA_Pos)
#define DSI_VHBPCR_HBP_Pos          0U
#define DSI_VHBPCR_HBP_Msk          (0xFFFUL << DSI_VHBPCR_HBP_Pos)
#define DSI_VLCR_HLINE_Pos          0U
#define DSI_VLCR_HLINE_Msk          (0x7FFFUL << DSI_VLCR_HLINE_Pos)
#define DSI_VVSACR_VSA_Pos          0U
#define DSI_VVSACR_VSA_Msk          (0x3FFUL << DSI_VVSACR_VSA_Pos)
#define DSI_VVBPCR_VBP_Pos          0U
#define DSI_VVBPCR_VBP_Msk          (0x3FFUL << DSI_VVBPCR_VBP_Pos)
#define DSI_VVFPCR_VFP_Pos          0U
#define DSI_VVFPCR_VFP_Msk          (0x3FFUL << DSI_VVFPCR_VFP_Pos)
#define DSI_VVACR_VA_Pos            0U
#define DSI_VVACR_VA_Msk            (0x3FFFUL << DSI_VVACR_VA_Pos)
#define DSI_LPMCR_LPSIZE_Pos        16U
#define DSI_LPMCR_LPSIZE_Msk        (0xFFUL << DSI_LPMCR_LPSIZE_Pos)
#define DSI_LPMCR_VLPSIZE_Pos       0U
#define DSI_LPMCR_VLPSIZE_Msk       (0xFFUL << DSI_LPMCR_VLPSIZE_Pos)
#define DSI_CLCR_DPCC_Pos           0U
#define DSI_CLCR_DPCC_Msk           (1U << DSI_CLCR_DPCC_Pos)
#define DSI_CLCR_ACR_Pos            1U
#define DSI_CLCR_ACR_Msk            (1U << DSI_CLCR_ACR_Pos)
#define DSI_CLTCR_LP2HS_TIME_Pos    0U
#define DSI_CLTCR_LP2HS_TIME_Msk    (0x3FFUL << DSI_CLTCR_LP2HS_TIME_Pos)
#define DSI_CLTCR_HS2LP_TIME_Pos    16U
#define DSI_CLTCR_HS2LP_TIME_Msk    (0x3FFUL << DSI_CLTCR_HS2LP_TIME_Pos)
#define DSI_DLTCR_MRD_TIME_Pos      0U
#define DSI_DLTCR_MRD_TIME_Msk      (0x7FFFUL << DSI_DLTCR_MRD_TIME_Pos)
#define DSI_DLTCR_LP2HS_TIME_Pos    16U
#define DSI_DLTCR_LP2HS_TIME_Msk    (0xFFUL << DSI_DLTCR_LP2HS_TIME_Pos)
#define DSI_DLTCR_HS2LP_TIME_Pos    24U
#define DSI_DLTCR_HS2LP_TIME_Msk    (0xFFUL << DSI_DLTCR_HS2LP_TIME_Pos)
#define DSI_PCTLR_CKE_Pos           2U
#define DSI_PCTLR_CKE_Msk           (1U << DSI_PCTLR_CKE_Pos)
#define DSI_PCTLR_DEN_Pos           1U
#define DSI_PCTLR_DEN_Msk           (1U << DSI_PCTLR_DEN_Pos)
#define DSI_PCONFR_NL_Pos           0U
#define DSI_PCONFR_NL_Msk           (0x3UL << DSI_PCONFR_NL_Pos)
#define DSI_PCONFR_SW_TIME_Pos      8U
#define DSI_PCONFR_SW_TIME_Msk      (0xFFUL << DSI_PCONFR_SW_TIME_Pos)
#define DSI_PCR_BTAE_Pos            2U
#define DSI_PCR_BTAE_Msk            (1U << DSI_PCR_BTAE_Pos)
#define DSI_GPSR_CMDFE_Pos          0U
#define DSI_GPSR_CMDFE_Msk          (1U << DSI_GPSR_CMDFE_Pos)
#define DSI_GPSR_CMDFF_Pos          1U
#define DSI_GPSR_CMDFF_Msk          (1U << DSI_GPSR_CMDFF_Pos)
#define DSI_GHCR_DT_Pos             0U
#define DSI_GHCR_DT_Msk             (0x3FUL << DSI_GHCR_DT_Pos)
#define DSI_GHCR_VCID_Pos           6U
#define DSI_GHCR_VCID_Msk           (0x3UL << DSI_GHCR_VCID_Pos)
#define DSI_GHCR_WCLSB_Pos          8U
#define DSI_GHCR_WCLSB_Msk          (0xFFUL << DSI_GHCR_WCLSB_Pos)
#define DSI_GHCR_WCMSB_Pos          16U
#define DSI_GHCR_WCMSB_Msk          (0xFFUL << DSI_GHCR_WCMSB_Pos)
#define DSI_WRPCR_PLL_NDIV_Pos      2U
#define DSI_WRPCR_PLL_NDIV_Msk      (0x7FUL << DSI_WRPCR_PLL_NDIV_Pos)
#define DSI_WRPCR_PLL_IDF_Pos       11U
#define DSI_WRPCR_PLL_IDF_Msk       (0xFUL << DSI_WRPCR_PLL_IDF_Pos)
#define DSI_WRPCR_PLL_ODF_Pos       16U
#define DSI_WRPCR_PLL_ODF_Msk       (0x3UL << DSI_WRPCR_PLL_ODF_Pos)
#define DSI_WRPCR_REGEN_Pos         24U
#define DSI_WRPCR_REGEN_Msk         (1U << DSI_WRPCR_REGEN_Pos)
#define DSI_WRPCR_PLLEN_Pos         0U
#define DSI_WRPCR_PLLEN_Msk         (1U << DSI_WRPCR_PLLEN_Pos)
#define DSI_WISR_RRS_Pos            12U
#define DSI_WISR_RRS_Msk            (1U << DSI_WISR_RRS_Pos)
#define DSI_WISR_PLLLS_Pos          8U
#define DSI_WISR_PLLLS_Msk          (1U << DSI_WISR_PLLLS_Pos)
#define DSI_PSR_PSSC_Pos            2U
#define DSI_PSR_PSSC_Msk            (1U << DSI_PSR_PSSC_Pos)
#define DSI_PSR_PSS0_Pos            4U
#define DSI_PSR_PSS0_Msk            (1U << DSI_PSR_PSS0_Pos)
#define DSI_PSR_PSS1_Pos            7U
#define DSI_PSR_PSS1_Msk            (1U << DSI_PSR_PSS1_Pos)
#define DSI_WPCR0_UIX4_Pos          0U
#define DSI_WPCR0_UIX4_Msk          (0x3FUL << DSI_WPCR0_UIX4_Pos)
#define DSI_CCR_TXECKDIV_Pos        0U
#define DSI_CCR_TXECKDIV_Msk        (0xFFUL << DSI_CCR_TXECKDIV_Pos)
#define DSI_MCR_CMDM_Pos            0U
#define DSI_MCR_CMDM_Msk            (1U << DSI_MCR_CMDM_Pos)

#define DSI_RGB565                  0x00000000U
#define DSI_RGB888                  0x00000005U
#define DSI_VID_MODE_BURST          2U
#define DSI_TWO_DATA_LANES          1U
#define DSI_LP_COMMAND_ENABLE       DSI_VMCR_LPCE_Msk
#define DSI_FLOW_CONTROL_BTAE       DSI_PCR_BTAE_Msk

#define DSI_DCS_SHORT_WRITE0        0x05U
#define DSI_DCS_SHORT_WRITE1        0x15U
#define DSI_DCS_LONG_WRITE          0x39U

/* --- LTDC registers --- */
#define LTDC_BASE       0x40016800U

typedef struct {
    uint32_t      RESERVED0[2];
    volatile uint32_t SSCR;
    volatile uint32_t BPCR;
    volatile uint32_t AWCR;
    volatile uint32_t TWCR;
    volatile uint32_t GCR;
    uint32_t      RESERVED1[2];
    volatile uint32_t SRCR;
    uint32_t      RESERVED2;
    volatile uint32_t BCCR;
    uint32_t      RESERVED3;
    volatile uint32_t IER;
    volatile uint32_t ISR;
    volatile uint32_t ICR;
    volatile uint32_t LIPCR;
    volatile uint32_t CPSR;
    volatile uint32_t CDSR;
} LTDC_TypeDef;

#define LTDC            ((LTDC_TypeDef *)LTDC_BASE)

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t WHPCR;
    volatile uint32_t WVPCR;
    volatile uint32_t CKCR;
    volatile uint32_t PFCR;
    volatile uint32_t CACR;
    volatile uint32_t DCCR;
    volatile uint32_t BFCR;
    uint32_t      RESERVED0[2];
    volatile uint32_t CFBAR;
    volatile uint32_t CFBLR;
    volatile uint32_t CFBLNR;
    uint32_t      RESERVED1[3];
    volatile uint32_t CLUTWR;
} LTDC_Layer_TypeDef;

#define LTDC_LAYER1     ((LTDC_Layer_TypeDef *)(LTDC_BASE + 0x84U))

#define LTDC_GCR_LTDCEN_Pos     0U
#define LTDC_GCR_LTDCEN_Msk     (1U << LTDC_GCR_LTDCEN_Pos)
#define LTDC_GCR_HSPOL_Pos      31U
#define LTDC_GCR_HSPOL_Msk      (1U << LTDC_GCR_HSPOL_Pos)
#define LTDC_GCR_VSPOL_Pos      30U
#define LTDC_GCR_VSPOL_Msk      (1U << LTDC_GCR_VSPOL_Pos)
#define LTDC_GCR_DEPOL_Pos      29U
#define LTDC_GCR_DEPOL_Msk      (1U << LTDC_GCR_DEPOL_Pos)
#define LTDC_GCR_PCPOL_Pos      28U
#define LTDC_GCR_PCPOL_Msk      (1U << LTDC_GCR_PCPOL_Pos)
#define LTDC_SRCR_IMR           (1U << 0)
#define LTDC_LAYER_CR_LEN       (1U << 0)
#define LTDC_PIXEL_FORMAT_RGB565 0x00000002U

/* --- OTM8009A commands --- */
#define OTM8009A_CMD_NOP        0x00U
#define OTM8009A_CMD_SLPOUT     0x11U
#define OTM8009A_CMD_DISPON     0x29U
#define OTM8009A_CMD_MADCTR     0x36U
#define OTM8009A_CMD_COLMOD     0x3AU
#define OTM8009A_CMD_CASET      0x2AU
#define OTM8009A_CMD_PASET      0x2BU
#define OTM8009A_CMD_RAMWR      0x2CU
#define OTM8009A_CMD_WRDISBV    0x51U
#define OTM8009A_CMD_WRCTRLD    0x53U
#define OTM8009A_CMD_WRCABC     0x55U
#define OTM8009A_CMD_WRCABCMB   0x5EU

#define OTM8009A_COLMOD_RGB565  0x55U
#define OTM8009A_MADCTR_LANDSCAPE 0x60U

/* --- Panel timings (OTM8009A 800x480 landscape) --- */
#define PANEL_HSYNC     2U
#define PANEL_HBP       34U
#define PANEL_HFP       34U
#define PANEL_VSYNC     1U
#define PANEL_VBP       15U
#define PANEL_VFP       16U

/* DSI byte clock and LTDC pixel clock (kHz) used by the BSP timing formula. */
#define LANE_BYTE_CLK_KHZ   62500U
#define LCD_CLOCK_KHZ       27429U

static uint32_t g_fb_addr = 0;

/* -------------------------------------------------------------------------- */
/* Minimal busy-wait delays (assumes 16 MHz HSI).                             */
/* -------------------------------------------------------------------------- */
static void disp_delay_ms(uint32_t ms)
{
    for (volatile uint32_t i = 0; i < (168000U * ms); i++)
        ;
}

static void disp_delay_us(uint32_t us)
{
    for (volatile uint32_t i = 0; i < (168U * us); i++)
        ;
}

/* -------------------------------------------------------------------------- */
/* Display-specific clock tree: HSE, DSI PLL and PLLSAI for LTDC.             */
/* -------------------------------------------------------------------------- */
static int disp_clock_init(void)
{
    /* The system clock code has already enabled HSE and set PLLM=25.
     * Here we just configure/enable PLLSAI for the LTDC pixel clock and
     * wait for HSE readiness as a safety check.
     */
    hal_uart_puts("[DISP] wait HSE\r\n");
    uint32_t timeout = 100000U;
    while (!(RCC_CR & RCC_CR_HSERDY)) {
        if (--timeout == 0U) {
            hal_uart_puts("[DISP] HSE timeout\r\n");
            return -1;
        }
    }

    /* Configure PLLSAI: 25 MHz / 25 * 384 / 7 / 2 = 27.429 MHz -> LTDC. */
    uint32_t pllsaicfgr = RCC_PLLSAICFGR;
    pllsaicfgr &= ~(RCC_PLLSAICFGR_PLLSAIN_Msk | RCC_PLLSAICFGR_PLLSAIR_Msk);
    pllsaicfgr |= (384U << RCC_PLLSAICFGR_PLLSAIN_Pos);
    pllsaicfgr |= (7U << RCC_PLLSAICFGR_PLLSAIR_Pos);
    RCC_PLLSAICFGR = pllsaicfgr;

    /* LTDC clock = PLLSAI / 2. */
    uint32_t dckcfgr1 = RCC_DCKCFGR1;
    dckcfgr1 &= ~RCC_DCKCFGR1_PLLSAIDIVR_Msk;
    dckcfgr1 |= (0U << RCC_DCKCFGR1_PLLSAIDIVR_Pos); /* 0 == /2 */
    RCC_DCKCFGR1 = dckcfgr1;

    /* Enable PLLSAI and wait. */
    RCC_CR |= RCC_CR_PLLSAION;
    timeout = 100000U;
    while (!(RCC_CR & RCC_CR_PLLSAIRDY)) {
        if (--timeout == 0U) {
            hal_uart_puts("[DISP] PLLSAI timeout\r\n");
            return -1;
        }
    }

    hal_uart_puts("[DISP] clocks ready\r\n");
    return 0;
}

/* -------------------------------------------------------------------------- */
/* DSI generic command helpers.                                               */
/* -------------------------------------------------------------------------- */
static int dsi_wait_cmd_fifo_empty(void)
{
    uint32_t timeout = 100000U;
    while (!(DSI->GPSR & DSI_GPSR_CMDFE_Msk)) {
        if (--timeout == 0U)
            return -1;
    }
    return 0;
}

static int dsi_short_write(uint8_t cmd, const uint8_t *param, uint8_t nparam)
{
    if (dsi_wait_cmd_fifo_empty() < 0)
        return -1;

    uint8_t dt = (nparam == 0U) ? DSI_DCS_SHORT_WRITE0 : DSI_DCS_SHORT_WRITE1;
    uint32_t p = (nparam > 0U) ? param[0] : 0U;
    DSI->GHCR = (dt << DSI_GHCR_DT_Pos) |
                (0U << DSI_GHCR_VCID_Pos) |
                ((uint32_t)cmd << DSI_GHCR_WCLSB_Pos) |
                (p << DSI_GHCR_WCMSB_Pos);
    return 0;
}

static int dsi_long_write(uint8_t cmd, const uint8_t *params, uint32_t nparams)
{
    if (dsi_wait_cmd_fifo_empty() < 0)
        return -1;

    uint32_t first = cmd;
    uint32_t nb = (nparams < 3U) ? nparams : 3U;
    for (uint32_t i = 0; i < nb; i++)
        first |= ((uint32_t)params[i] << (8U + 8U * i));
    DSI->GPDR = first;

    uint32_t remaining = nparams - nb;
    const uint8_t *p = params + nb;
    while (remaining > 0U) {
        nb = (remaining < 4U) ? remaining : 4U;
        uint32_t word = 0U;
        for (uint32_t i = 0; i < nb; i++)
            word |= ((uint32_t)p[i] << (8U * i));
        DSI->GPDR = word;
        p += nb;
        remaining -= nb;
    }

    DSI->GHCR = (DSI_DCS_LONG_WRITE << DSI_GHCR_DT_Pos) |
                (0U << DSI_GHCR_VCID_Pos) |
                (((nparams + 1U) & 0xFFU) << DSI_GHCR_WCLSB_Pos) |
                ((((nparams + 1U) >> 8U) & 0xFFU) << DSI_GHCR_WCMSB_Pos);
    return 0;
}

static int otm8009a_write_reg(uint8_t reg, const uint8_t *params, uint32_t nparams)
{
    /* Verified against ST's reference OTM8009A driver (otm8009a.c /
     * stm32f769i_discovery_lcd.c): every "short" register write in this
     * init table -- including the address-shift NOP prefixes and the
     * nominally zero-parameter DCS commands (SLPOUT, DISPON, RAMWR) -- is
     * always sent as a DCS Short Write with ONE parameter byte
     * (0x15 / DSI_DCS_SHORT_WRITE1). The reference tables pair every such
     * command with a data byte (e.g. {NOP, 0x00}, {DISPON, 0x00}) and the
     * IO layer always transmits pParams[1] regardless of the caller's
     * "NbrParams" value. Selecting the 0-parameter packet type (0x05)
     * here for nparams==0 silently drops that byte and leaves the panel
     * unconfigured, which is why gfxshell produced no picture. */
    if (nparams <= 1U)
        return dsi_short_write(reg, params, 1U);
    else
        return dsi_long_write(reg, params, nparams);
}

/* -------------------------------------------------------------------------- */
/* OTM8009A panel initialization (adapted from STM32Cube BSP).                */
/* -------------------------------------------------------------------------- */
static int otm8009a_init(void)
{
    /* Manufacturer-specific register tables. */
    static const uint8_t lcd_reg_data1[]  = {0x80, 0x09, 0x01};
    static const uint8_t lcd_reg_data2[]  = {0x80, 0x09};
    static const uint8_t lcd_reg_data3[]  = {0x00, 0x09, 0x0F, 0x0E, 0x07, 0x10, 0x0B, 0x0A, 0x04, 0x07, 0x0B, 0x08, 0x0F, 0x10, 0x0A, 0x01};
    static const uint8_t lcd_reg_data4[]  = {0x00, 0x09, 0x0F, 0x0E, 0x07, 0x10, 0x0B, 0x0A, 0x04, 0x07, 0x0B, 0x08, 0x0F, 0x10, 0x0A, 0x01};
    static const uint8_t lcd_reg_data5[]  = {0x79, 0x79};
    static const uint8_t lcd_reg_data6[]  = {0x00, 0x01};
    static const uint8_t lcd_reg_data7[]  = {0x85, 0x01, 0x00, 0x84, 0x01, 0x00};
    static const uint8_t lcd_reg_data8[]  = {0x18, 0x04, 0x03, 0x39, 0x00, 0x00, 0x00, 0x18, 0x03, 0x03, 0x3A, 0x00, 0x00, 0x00};
    static const uint8_t lcd_reg_data9[]  = {0x18, 0x02, 0x03, 0x3B, 0x00, 0x00, 0x00, 0x18, 0x01, 0x03, 0x3C, 0x00, 0x00, 0x00};
    static const uint8_t lcd_reg_data10[] = {0x01, 0x01, 0x20, 0x20, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00};
    static const uint8_t lcd_reg_data11[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t lcd_reg_data12[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t lcd_reg_data13[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t lcd_reg_data14[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t lcd_reg_data15[] = {0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t lcd_reg_data16[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t lcd_reg_data17[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t lcd_reg_data18[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static const uint8_t lcd_reg_data19[] = {0x00, 0x26, 0x09, 0x0B, 0x01, 0x25, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t lcd_reg_data20[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x0A, 0x0C, 0x02};
    static const uint8_t lcd_reg_data21[] = {0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t lcd_reg_data22[] = {0x00, 0x25, 0x0C, 0x0A, 0x02, 0x26, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t lcd_reg_data23[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x25, 0x0B, 0x09, 0x01};
    static const uint8_t lcd_reg_data24[] = {0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t lcd_reg_data25[] = {0xFF, 0xFF, 0xFF};
    static const uint8_t lcd_reg_data27[] = {0x00, 0x00, 0x03, 0x1F}; /* landscape CASET */
    static const uint8_t lcd_reg_data28[] = {0x00, 0x00, 0x01, 0xDF}; /* landscape PASET */

    static const uint8_t short_reg_data[] = {
        0x00, 0x00, 0x80, 0x30, 0x8A, 0x40, 0xB1, 0xA9, 0x91, 0x34, 0xB4, 0x50, 0x4E, 0x81, 0x66, 0xA1,
        0x08, 0x92, 0x01, 0x95, 0x94, 0x33, 0xA3, 0x1B, 0x82, 0x83, 0x83, 0x0E, 0xA6, 0xA0, 0xB0, 0xC0,
        0xD0, 0x90, 0xE0, 0xF0, 0x00, OTM8009A_COLMOD_RGB565, 0x77, 0x7F, 0x2C, 0x02, 0xFF, 0x00,
        0x00, 0x00, 0x66, 0xB6, 0x06, 0xB1, 0x06
    };

    int ret = 0;

    /* Enter CMD2 mode. */
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[1], 0);
    ret += otm8009a_write_reg(0xFF, lcd_reg_data1, sizeof(lcd_reg_data1));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[2], 0);
    ret += otm8009a_write_reg(0xFF, lcd_reg_data2, sizeof(lcd_reg_data2));

    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[2], 0);
    ret += otm8009a_write_reg(0xC4, &short_reg_data[3], 0);
    disp_delay_ms(10);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[4], 0);
    ret += otm8009a_write_reg(0xC4, &short_reg_data[5], 0);
    disp_delay_ms(10);

    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[6], 0);
    ret += otm8009a_write_reg(0xC5, &short_reg_data[7], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[8], 0);
    ret += otm8009a_write_reg(0xC5, &short_reg_data[9], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[10], 0);
    ret += otm8009a_write_reg(0xC0, &short_reg_data[11], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[1], 0);
    ret += otm8009a_write_reg(0xD9, &short_reg_data[12], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[13], 0);
    ret += otm8009a_write_reg(0xC1, &short_reg_data[14], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[15], 0);
    ret += otm8009a_write_reg(0xC1, &short_reg_data[16], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[17], 0);
    ret += otm8009a_write_reg(0xC5, &short_reg_data[18], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[19], 0);
    ret += otm8009a_write_reg(0xC5, &short_reg_data[9], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[1], 0);
    ret += otm8009a_write_reg(0xD8, lcd_reg_data5, sizeof(lcd_reg_data5));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[20], 0);
    ret += otm8009a_write_reg(0xC5, &short_reg_data[21], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[22], 0);
    ret += otm8009a_write_reg(0xC0, &short_reg_data[23], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[24], 0);
    ret += otm8009a_write_reg(0xC5, &short_reg_data[25], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[13], 0);
    ret += otm8009a_write_reg(0xC4, &short_reg_data[26], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[15], 0);
    ret += otm8009a_write_reg(0xC1, &short_reg_data[27], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[28], 0);
    ret += otm8009a_write_reg(0xB3, lcd_reg_data6, sizeof(lcd_reg_data6));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[2], 0);
    ret += otm8009a_write_reg(0xCE, lcd_reg_data7, sizeof(lcd_reg_data7));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[29], 0);
    ret += otm8009a_write_reg(0xCE, lcd_reg_data8, sizeof(lcd_reg_data8));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[30], 0);
    ret += otm8009a_write_reg(0xCE, lcd_reg_data9, sizeof(lcd_reg_data9));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[31], 0);
    ret += otm8009a_write_reg(0xCF, lcd_reg_data10, sizeof(lcd_reg_data10));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[32], 0);
    ret += otm8009a_write_reg(0xCF, &short_reg_data[45], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[2], 0);
    ret += otm8009a_write_reg(0xCB, lcd_reg_data11, sizeof(lcd_reg_data11));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[33], 0);
    ret += otm8009a_write_reg(0xCB, lcd_reg_data12, sizeof(lcd_reg_data12));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[29], 0);
    ret += otm8009a_write_reg(0xCB, lcd_reg_data13, sizeof(lcd_reg_data13));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[30], 0);
    ret += otm8009a_write_reg(0xCB, lcd_reg_data14, sizeof(lcd_reg_data14));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[31], 0);
    ret += otm8009a_write_reg(0xCB, lcd_reg_data15, sizeof(lcd_reg_data15));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[32], 0);
    ret += otm8009a_write_reg(0xCB, lcd_reg_data16, sizeof(lcd_reg_data16));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[34], 0);
    ret += otm8009a_write_reg(0xCB, lcd_reg_data17, sizeof(lcd_reg_data17));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[35], 0);
    ret += otm8009a_write_reg(0xCB, lcd_reg_data18, sizeof(lcd_reg_data18));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[2], 0);
    ret += otm8009a_write_reg(0xCC, lcd_reg_data19, sizeof(lcd_reg_data19));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[33], 0);
    ret += otm8009a_write_reg(0xCC, lcd_reg_data20, sizeof(lcd_reg_data20));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[29], 0);
    ret += otm8009a_write_reg(0xCC, lcd_reg_data21, sizeof(lcd_reg_data21));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[30], 0);
    ret += otm8009a_write_reg(0xCC, lcd_reg_data22, sizeof(lcd_reg_data22));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[31], 0);
    ret += otm8009a_write_reg(0xCC, lcd_reg_data23, sizeof(lcd_reg_data23));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[32], 0);
    ret += otm8009a_write_reg(0xCC, lcd_reg_data24, sizeof(lcd_reg_data24));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[13], 0);
    ret += otm8009a_write_reg(0xC5, &short_reg_data[46], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[47], 0);
    ret += otm8009a_write_reg(0xF5, &short_reg_data[48], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[49], 0);
    ret += otm8009a_write_reg(0xC6, &short_reg_data[50], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[1], 0);
    ret += otm8009a_write_reg(0xFF, lcd_reg_data25, sizeof(lcd_reg_data25));

    /* Standard DCS init. */
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[1], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[1], 0);
    ret += otm8009a_write_reg(0xE1, lcd_reg_data3, sizeof(lcd_reg_data3));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[1], 0);
    ret += otm8009a_write_reg(0xE2, lcd_reg_data4, sizeof(lcd_reg_data4));
    ret += otm8009a_write_reg(OTM8009A_CMD_SLPOUT, &short_reg_data[36], 0);
    disp_delay_ms(120);

    /* RGB565 pixel format. */
    ret += otm8009a_write_reg(OTM8009A_CMD_COLMOD, &short_reg_data[37], 1);

    /* Landscape orientation. */
    uint8_t madctr = OTM8009A_MADCTR_LANDSCAPE;
    ret += otm8009a_write_reg(OTM8009A_CMD_MADCTR, &madctr, 1);
    ret += otm8009a_write_reg(OTM8009A_CMD_CASET, lcd_reg_data27, sizeof(lcd_reg_data27));
    ret += otm8009a_write_reg(OTM8009A_CMD_PASET, lcd_reg_data28, sizeof(lcd_reg_data28));

    /* CABC / brightness (use the original ST BSP values). */
    ret += otm8009a_write_reg(OTM8009A_CMD_WRDISBV, &short_reg_data[39], 1);
    ret += otm8009a_write_reg(OTM8009A_CMD_WRCTRLD, &short_reg_data[40], 1);
    ret += otm8009a_write_reg(OTM8009A_CMD_WRCABC, &short_reg_data[41], 1);
    ret += otm8009a_write_reg(OTM8009A_CMD_WRCABCMB, &short_reg_data[42], 1);

    ret += otm8009a_write_reg(OTM8009A_CMD_DISPON, &short_reg_data[43], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[1], 0);
    ret += otm8009a_write_reg(OTM8009A_CMD_RAMWR, &short_reg_data[44], 0);

    if (ret != 0) {
        hal_uart_puts("[OTM] init command failed\r\n");
        return -1;
    }
    return 0;
}

/* -------------------------------------------------------------------------- */
/* DSI host / LTDC bring-up.                                                  */
/* -------------------------------------------------------------------------- */
static void panel_reset_gpio_init(void)
{
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOIEN | RCC_AHB1ENR_GPIOJEN;
    (void)RCC_AHB1ENR;

    /* Backlight: PI14, push-pull output. */
    hal_gpio_init_output(DISP_BACKLIGHT_PORT, DISP_BACKLIGHT_PIN, GPIO_SPEED_MEDIUM);
    hal_gpio_write(DISP_BACKLIGHT_PORT, DISP_BACKLIGHT_PIN, 0);

    /* LCD reset: PJ15, push-pull output. */
    hal_gpio_init_output(DISP_RESET_PORT, DISP_RESET_PIN, GPIO_SPEED_MEDIUM);
    hal_gpio_write(DISP_RESET_PORT, DISP_RESET_PIN, 1);
}

static void panel_reset(void)
{
    hal_gpio_write(DISP_RESET_PORT, DISP_RESET_PIN, 0);
    disp_delay_ms(20);
    hal_gpio_write(DISP_RESET_PORT, DISP_RESET_PIN, 1);
    disp_delay_ms(10);
}

static int dsi_host_init(void)
{
    /* Enable clocks and reset DSI/LTDC. */
    RCC_APB2ENR |= RCC_APB2ENR_LTDCEN | RCC_APB2ENR_DSIHOSTEN;
    (void)RCC_APB2ENR;

    RCC_APB2RSTR |= RCC_APB2RSTR_LTDCRST | RCC_APB2RSTR_DSIHOSTRST;
    disp_delay_us(10);
    RCC_APB2RSTR &= ~(RCC_APB2RSTR_LTDCRST | RCC_APB2RSTR_DSIHOSTRST);

    hal_uart_puts("[DSI] regulator enable\r\n");

    /* Enable DSI regulator and wait for ready flag. */
    DSI->WRPCR |= DSI_WRPCR_REGEN_Msk;
    uint32_t timeout = 100000U;
    while (!(DSI->WISR & DSI_WISR_RRS_Msk)) {
        if (--timeout == 0U) {
            hal_uart_puts("[DSI] regulator ready timeout\r\n");
            return -1;
        }
    }

    /* Configure DSI PLL: 25 MHz / 5 * 100 / 1 = 500 MHz -> byte clock 62.5 MHz. */
    DSI->WRPCR &= ~(DSI_WRPCR_PLL_NDIV_Msk | DSI_WRPCR_PLL_IDF_Msk | DSI_WRPCR_PLL_ODF_Msk);
    DSI->WRPCR |= (100U << DSI_WRPCR_PLL_NDIV_Pos) |
                  (5U   << DSI_WRPCR_PLL_IDF_Pos)  |
                  (0U   << DSI_WRPCR_PLL_ODF_Pos);

    DSI->WRPCR |= DSI_WRPCR_PLLEN_Msk;
    disp_delay_ms(1);
    timeout = 100000U;
    while (!(DSI->WISR & DSI_WISR_PLLLS_Msk)) {
        if (--timeout == 0U) {
            hal_uart_puts("[DSI] PLL lock timeout\r\n");
            return -1;
        }
    }

    hal_uart_puts("[DSI] PLL locked\r\n");

    /* Enable the DSI host briefly to configure the clock/PHY, then disable. */
    DSI->CR |= DSI_CR_EN_Msk;

    DSI->CCR &= ~DSI_CCR_TXECKDIV_Msk;
    DSI->CCR |= (4U << DSI_CCR_TXECKDIV_Pos); /* 62500/15620 ~= 4 */

    /* Enable D-PHY digital and clock. */
    DSI->PCTLR |= DSI_PCTLR_DEN_Msk | DSI_PCTLR_CKE_Msk;

    /* Two data lanes. */
    DSI->PCONFR &= ~DSI_PCONFR_NL_Msk;
    DSI->PCONFR |= (DSI_TWO_DATA_LANES << DSI_PCONFR_NL_Pos);

    timeout = 100000U;
    while ((DSI->PSR & (DSI_PSR_PSS0_Msk | DSI_PSR_PSS1_Msk | DSI_PSR_PSSC_Msk)) !=
           (DSI_PSR_PSS0_Msk | DSI_PSR_PSS1_Msk | DSI_PSR_PSSC_Msk)) {
        if (--timeout == 0U) {
            hal_uart_puts("[DSI] PHY stop-state timeout\r\n");
            return -1;
        }
    }

    /* Unit interval x4 for 500 Mbps/lane: (1000/500)*4 = 8. */
    DSI->WPCR[0] &= ~DSI_WPCR0_UIX4_Msk;
    DSI->WPCR[0] |= (8U << DSI_WPCR0_UIX4_Pos);

    /* Keep the DSI host enabled; command mode will be selected before OTM
     * initialization and switched to video mode afterwards.
     */

    /* Automatic clock lane control + D-PHY clock control. */
    DSI->CLCR &= ~(DSI_CLCR_DPCC_Msk | DSI_CLCR_ACR_Msk);
    DSI->CLCR |= DSI_CLCR_DPCC_Msk | DSI_CLCR_ACR_Msk;

    /* Flow control: BTA enabled so commands can be acknowledged. */
    DSI->PCR |= DSI_FLOW_CONTROL_BTAE;

    hal_uart_puts("[DSI] host init done\r\n");
    return 0;
}

static int dsi_video_mode_init(void)
{
    /* Landscape 800x480 timing from the OTM8009A BSP. */
    const uint32_t hsa  = PANEL_HSYNC;
    const uint32_t hbp  = PANEL_HBP;
    const uint32_t hfp  = PANEL_HFP;
    const uint32_t hact = DISPLAY_WIDTH;
    const uint32_t vsa  = PANEL_VSYNC;
    const uint32_t vbp  = PANEL_VBP;
    const uint32_t vfp  = PANEL_VFP;
    const uint32_t vact = DISPLAY_HEIGHT;

    uint32_t hsa_byte  = (hsa  * LANE_BYTE_CLK_KHZ) / LCD_CLOCK_KHZ;
    uint32_t hbp_byte  = (hbp  * LANE_BYTE_CLK_KHZ) / LCD_CLOCK_KHZ;
    uint32_t hline_byte = ((hact + hsa + hbp + hfp) * LANE_BYTE_CLK_KHZ) / LCD_CLOCK_KHZ;

    /* This only programs the video-mode timing/format registers; DSI is
     * never put into Command Mode at all (see hal_display_init()), so
     * there is nothing to switch out of here. */

    /* Video mode type: burst. */
    DSI->VMCR &= ~DSI_VMCR_VMT_Msk;
    DSI->VMCR |= (DSI_VID_MODE_BURST << DSI_VMCR_VMT_Pos);

    DSI->VPCR &= ~DSI_VPCR_VPSIZE_Msk;
    DSI->VPCR |= (hact << DSI_VPCR_VPSIZE_Pos);

    DSI->VCCR &= ~DSI_VCCR_NUMC_Msk;
    DSI->VCCR |= (0U << DSI_VCCR_NUMC_Pos);

    DSI->VNPCR &= ~DSI_VNPCR_NPSIZE_Msk;
    DSI->VNPCR |= (0x0FFFU << DSI_VNPCR_NPSIZE_Pos);

    DSI->LVCIDR &= ~0x3U;
    DSI->LVCIDR |= 0U; /* virtual channel 0 */

    /* Polarity: all active high. */
    DSI->LPCR &= ~(DSI_LPCR_DEP_Msk | DSI_LPCR_VSP_Msk | DSI_LPCR_HSP_Msk);
    DSI->LPCR |= DSI_LPCR_DEP_Msk | DSI_LPCR_VSP_Msk | DSI_LPCR_HSP_Msk;

    /* Color coding: RGB565 for both DSI host and wrapper. */
    DSI->LCOLCR &= ~DSI_LCOLCR_COLC_Msk;
    DSI->LCOLCR |= (DSI_RGB565 << DSI_LCOLCR_COLC_Pos);
    DSI->LCOLCR &= ~DSI_LCOLCR_LPE_Msk;

    DSI->WCFGR &= ~DSI_WCFGR_COLMUX_Msk;
    DSI->WCFGR |= (DSI_RGB565 << DSI_WCFGR_COLMUX_Pos);

    DSI->VHSACR &= ~DSI_VHSACR_HSA_Msk;
    DSI->VHSACR |= (hsa_byte << DSI_VHSACR_HSA_Pos);

    DSI->VHBPCR &= ~DSI_VHBPCR_HBP_Msk;
    DSI->VHBPCR |= (hbp_byte << DSI_VHBPCR_HBP_Pos);

    DSI->VLCR &= ~DSI_VLCR_HLINE_Msk;
    DSI->VLCR |= (hline_byte << DSI_VLCR_HLINE_Pos);

    DSI->VVSACR &= ~DSI_VVSACR_VSA_Msk;
    DSI->VVSACR |= (vsa << DSI_VVSACR_VSA_Pos);

    DSI->VVBPCR &= ~DSI_VVBPCR_VBP_Msk;
    DSI->VVBPCR |= (vbp << DSI_VVBPCR_VBP_Pos);

    DSI->VVFPCR &= ~DSI_VVFPCR_VFP_Msk;
    DSI->VVFPCR |= (vfp << DSI_VVFPCR_VFP_Pos);

    DSI->VVACR &= ~DSI_VVACR_VA_Msk;
    DSI->VVACR |= (vact << DSI_VVACR_VA_Pos);

    /* Low-power command settings (follow the ST BSP). */
    DSI->VMCR &= ~DSI_VMCR_LPCE_Msk;
    DSI->VMCR |= DSI_LP_COMMAND_ENABLE;

    DSI->LPMCR &= ~(DSI_LPMCR_LPSIZE_Msk | DSI_LPMCR_VLPSIZE_Msk);
    DSI->LPMCR |= (16U << DSI_LPMCR_LPSIZE_Pos);
    DSI->LPMCR |= 0U;

    DSI->VMCR |= DSI_VMCR_LPHFPE_Msk |
                 DSI_VMCR_LPHBPE_Msk |
                 DSI_VMCR_LPVAE_Msk  |
                 DSI_VMCR_LPVFPE_Msk |
                 DSI_VMCR_LPVBPE_Msk |
                 DSI_VMCR_LPVSAE_Msk;
    DSI->VMCR &= ~DSI_VMCR_FBTAAE_Msk;

    hal_uart_puts("[DSI] video mode done\r\n");
    return 0;
}

static void dsi_phy_timers_init(void)
{
    /* Clock lane: max(HS2LP, LP2HS) in both fields (ST workaround). */
    DSI->CLTCR &= ~(DSI_CLTCR_LP2HS_TIME_Msk | DSI_CLTCR_HS2LP_TIME_Msk);
    DSI->CLTCR |= (0x14U << DSI_CLTCR_LP2HS_TIME_Pos) |
                  (0x14U << DSI_CLTCR_HS2LP_TIME_Pos);

    DSI->DLTCR &= ~(DSI_DLTCR_MRD_TIME_Msk |
                    DSI_DLTCR_LP2HS_TIME_Msk |
                    DSI_DLTCR_HS2LP_TIME_Msk);
    DSI->DLTCR |= (0U   << DSI_DLTCR_MRD_TIME_Pos)  |
                  (0x0AU << DSI_DLTCR_LP2HS_TIME_Pos) |
                  (0x0AU << DSI_DLTCR_HS2LP_TIME_Pos);

    DSI->PCONFR &= ~DSI_PCONFR_SW_TIME_Msk;
    DSI->PCONFR |= (0U << DSI_PCONFR_SW_TIME_Pos);
}

static void ltdc_init(uint32_t fb_addr)
{
    const uint32_t hsync = PANEL_HSYNC;
    const uint32_t hbp   = PANEL_HBP;
    const uint32_t hfp   = PANEL_HFP;
    const uint32_t vsync = PANEL_VSYNC;
    const uint32_t vbp   = PANEL_VBP;
    const uint32_t vfp   = PANEL_VFP;

    LTDC->GCR = 0U;

    LTDC->SSCR = ((hsync - 1U) << 16) | (vsync - 1U);
    LTDC->BPCR = ((hsync + hbp - 1U) << 16) | (vsync + vbp - 1U);
    LTDC->AWCR = ((hsync + hbp + DISPLAY_WIDTH - 1U) << 16) |
                 (vsync + vbp + DISPLAY_HEIGHT - 1U);
    LTDC->TWCR = ((hsync + hbp + DISPLAY_WIDTH + hfp - 1U) << 16) |
                 (vsync + vbp + DISPLAY_HEIGHT + vfp - 1U);
    LTDC->BCCR = 0x00000000U;

    /* Layer 1: RGB565, full screen. */
    LTDC_LAYER1->CR = 0U;
    LTDC_LAYER1->WHPCR = ((hsync + hbp + DISPLAY_WIDTH - 1U) << 16) |
                         (hsync + hbp);
    LTDC_LAYER1->WVPCR = ((vsync + vbp + DISPLAY_HEIGHT - 1U) << 16) |
                         (vsync + vbp);
    LTDC_LAYER1->PFCR = LTDC_PIXEL_FORMAT_RGB565;
    LTDC_LAYER1->CACR = 0xFFU;
    LTDC_LAYER1->BFCR = (4U << 8) | (5U << 0); /* Constant alpha / constant alpha inverse */
    LTDC_LAYER1->CFBAR = fb_addr;
    LTDC_LAYER1->CFBLR = ((DISPLAY_WIDTH * DISPLAY_BPP) << 16) |
                         (DISPLAY_WIDTH * DISPLAY_BPP + 3U);
    LTDC_LAYER1->CFBLNR = DISPLAY_HEIGHT;
    LTDC_LAYER1->CR = LTDC_LAYER_CR_LEN;

    LTDC->SRCR = LTDC_SRCR_IMR;

    /* Enable LTDC with active-high control signals. */
    LTDC->GCR = LTDC_GCR_LTDCEN_Msk |
                LTDC_GCR_HSPOL_Msk |
                LTDC_GCR_VSPOL_Msk |
                LTDC_GCR_DEPOL_Msk;
}

/* -------------------------------------------------------------------------- */
/* Public API.                                                                */
/* -------------------------------------------------------------------------- */
void hal_display_init(void)
{
    g_fb_addr = hal_sdram_base();
    hal_uart_puts("[DISP] clearing fb\r\n");
    memset((void *)g_fb_addr, 0, DISPLAY_FB_SIZE);

    if (disp_clock_init() < 0)
        return;

    panel_reset_gpio_init();
    panel_reset();
    hal_uart_puts("[DISP] reset done\r\n");

    if (dsi_host_init() < 0)
        return;

    /* Match the ST reference bring-up order (stm32f769i_discovery_lcd.c):
     * configure DSI Video Mode and LTDC and START the video stream FIRST,
     * and only THEN send the OTM8009A panel init commands -- as Low-Power
     * DCS commands inserted into the already-running video stream (video
     * mode's LPCE/LPxxE bits, set below, are what makes this possible).
     *
     * The previous order (send all OTM8009A commands first, in a manual
     * Command-Mode session with no active video stream, then switch to
     * Video Mode afterward) is not how the panel is meant to be brought
     * up: the panel briefly showed whatever was in its own GRAM/garbage
     * state as soon as a stream appeared (the solid color bands the user
     * saw), then blanked itself, even though the host's own DSI/LTDC
     * registers and the SDRAM framebuffer content remained perfectly
     * valid throughout -- i.e. the fault was on the panel side, not
     * something a host-side register readback could catch.
     */
    if (dsi_video_mode_init() < 0)
        return;
    dsi_phy_timers_init();

    hal_uart_puts("[DISP] ltdc init\r\n");
    ltdc_init(g_fb_addr);

    /* Start DSI host + wrapper together so LTDC pixels flow to the panel
     * before any panel commands are sent. */
    DSI->CR |= DSI_CR_EN_Msk;
    DSI->WCR |= DSI_WCR_DSIEN_Msk;

    hal_uart_puts("[DISP] sending OTM init\r\n");
    if (otm8009a_init() < 0)
        return;

    hal_display_backlight_on();
    hal_uart_puts("[DISP] backlight on\r\n");
}

uint32_t hal_display_fb_addr(void)
{
    return g_fb_addr;
}

void hal_display_backlight_on(void)
{
    hal_gpio_write(DISP_BACKLIGHT_PORT, DISP_BACKLIGHT_PIN, 1);
}

void hal_display_backlight_off(void)
{
    hal_gpio_write(DISP_BACKLIGHT_PORT, DISP_BACKLIGHT_PIN, 0);
}
