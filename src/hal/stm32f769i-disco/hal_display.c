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
 *
 * KNOWN REMAINING ISSUE (as of this comment): apps/colored-screen's
 * halftest command (solid-color fill confined to one half of the
 * screen, rest black) shows TOP/BOTTOM halves displaying correctly,
 * but LEFT/RIGHT halves coming in under half brightness and visibly
 * spreading across the whole screen over several seconds, with an
 * asymmetric bias (LEFT test strengthens near the top, RIGHT test
 * strengthens near the bottom). Confirmed via longtest (40s holds):
 * TOP is perfectly stable the whole time; LEFT reproduces the same
 * spreading -- so this is specifically tied to horizontal (per-line)
 * color transitions, not a general time-based drift. Confirmed
 * bitrate-independent (tested at 250 Mbps/lane, no change) -- rules
 * out a source-driver settling-time-margin explanation.
 *
 * Exhaustively cross-checked against a live, verified-stable Zephyr
 * run on this exact board and confirmed byte-for-byte IDENTICAL:
 * every DSI Host video-timing/config register (MCR, VMCR, VPCR, VCCR,
 * VNPCR, VHSACR, VHBPCR, VLCR, VVSACR, VVBPCR, VVFPCR, VVACR, LPMCR,
 * CCR, CLCR, LCOLCR, PCR -- full value, not just BTAE), the DSI
 * Wrapper (WCFGR, WPCR[0-4], WRPCR), every DSI "Current Configuration"
 * shadow register (VSCR.EN=0 on both, confirming these are genuinely
 * inactive rather than holding a stale/different value), every LTDC
 * register (SSCR/BPCR/AWCR/TWCR/GCR/BFCR/CFBAR/CFBLR and the layer
 * window registers), the RCC clock tree (main PLL and PLLSAI, both
 * enable+ready bits and the N/R/DIVR dividers), and FMC/SDRAM Bank 1
 * timing (SDCR1/SDTR1/SDRTR -- Bank 2 is unpopulated on this board and
 * correctly left unconfigured on both). Framebuffer content was
 * independently verified correct via direct SDRAM reads. If picking
 * this back up: register-level comparison against Zephyr is exhausted
 * for now -- the next useful lever is likely either real hardware
 * instrumentation (logic analyzer/scope on the DSI lanes) or a
 * different physical MB1166 unit, not another register audit.
 */
#include "hal_display.h"
#include "hal_sdram.h"
#include <hal/board.h>
#include <hal/hal_gpio.h>
#include <string.h>
#include <stdio.h>

extern void hal_uart_puts(const char *s);

/* Runtime DSI command trace: logs the actual GHCR/GPDR values as they are
 * written, at the moment they are written, rather than what the calling
 * source code appears to intend -- to catch a bug in dsi_short_write()/
 * dsi_long_write() themselves that a source read-through could miss. */
#define DSI_TRACE_ENABLE 1
#if DSI_TRACE_ENABLE
static uint32_t g_dsi_trace_seq;
#endif

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
    volatile uint32_t VSCR; /* Video Shadow Control -- live-checked
                              * (both this driver and a known-good
                              * Zephyr run): EN=0, i.e. shadow mode is
                              * off, so the *CCR "Current Configuration"
                              * mirrors below (VMCCR etc.) are inactive
                              * and read 0 -- the live video timing
                              * genuinely comes straight from VMCR/
                              * VHSACR/etc., not these. Do not re-chase
                              * this: it was checked as a candidate for
                              * halftest's horizontal-only artifact and
                              * ruled out (matches Zephyr exactly). */
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
/* Verified against the real STM32F7 CMSIS device header
 * (stm32f769xx.h): DSI_GPSR_PRDFE_Pos = 4, DSI_GPSR_RCB_Pos = 6. */
#define DSI_GPSR_PRDFE_Pos          4U
#define DSI_GPSR_PRDFE_Msk          (1U << DSI_GPSR_PRDFE_Pos)
#define DSI_GPSR_RCB_Pos            6U
#define DSI_GPSR_RCB_Msk            (1U << DSI_GPSR_RCB_Pos)
/* Verified against the real STM32F7 HAL (stm32f7xx_hal_dsi.h):
 * DSI_DCS_SHORT_PKT_READ = 0x06. */
#define DSI_DCS_SHORT_READ          0x06U
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
#define LTDC_SRCR_VBR           (1U << 1)
#define LTDC_LAYER_CR_LEN       (1U << 0)
#define LTDC_PIXEL_FORMAT_RGB565 0x00000002U
#define LTDC_ISR_LIF_Pos        0U
#define LTDC_ISR_LIF_Msk        (1U << LTDC_ISR_LIF_Pos)
#define LTDC_ICR_CLIF_Pos       0U
#define LTDC_ICR_CLIF_Msk       (1U << LTDC_ICR_CLIF_Pos)
#define LTDC_IER_LIE_Pos        0U
#define LTDC_IER_LIE_Msk        (1U << LTDC_IER_LIE_Pos)
#define LTDC_ISR_FUIF_Pos       1U
#define LTDC_ISR_FUIF_Msk       (1U << LTDC_ISR_FUIF_Pos)
#define LTDC_ISR_TERRIF_Pos     2U
#define LTDC_ISR_TERRIF_Msk     (1U << LTDC_ISR_TERRIF_Pos)
#define LTDC_ICR_CFUIF_Pos      1U
#define LTDC_ICR_CFUIF_Msk      (1U << LTDC_ICR_CFUIF_Pos)
#define LTDC_ICR_CTERRIF_Pos    2U
#define LTDC_ICR_CTERRIF_Msk    (1U << LTDC_ICR_CTERRIF_Pos)
#define LTDC_IER_FUIE_Pos       1U
#define LTDC_IER_FUIE_Msk       (1U << LTDC_IER_FUIE_Pos)
#define LTDC_IER_TERRIE_Pos     2U
#define LTDC_IER_TERRIE_Msk     (1U << LTDC_IER_TERRIE_Pos)

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
#define OTM8009A_COLMOD_RGB888  0x77U
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
#define LCD_CLOCK_KHZ       9600U

static uint32_t g_fb_addr = 0;

/* -------------------------------------------------------------------------- */
/* Minimal busy-wait delays, calibrated for the 216 MHz HCLK configured by
 * hal_system_init() (was 168 MHz; scaled by 216/168 = 1.2857).             */
/* -------------------------------------------------------------------------- */
/* Measured against saramos_tick_ms (the real SysTick-driven millisecond
 * counter) via the [WATCH] diagnostic loop in hal_display_init(): a
 * nominal disp_delay_ms(50) call was actually taking ~1468ms of real
 * elapsed time -- a ~29.4x overrun, not the ~1 cycle/iteration the old
 * "216000 iterations per ms" constant assumed. The `volatile uint32_t i`
 * loop variable forces a genuine load+store to memory every iteration
 * (not just a register compare/branch), and at this loop's actual
 * measured throughput each iteration costs far more than 1 cycle. This
 * had made every call site using these delays (panel_reset()'s
 * 10/20ms-class pulses, otm8009a_init()'s 10ms/10ms/120ms wake delays,
 * disp_clock_init()'s PLL-lock polling) run ~29x longer in real time
 * than the source intended, though not incorrectly (nothing here was
 * timing-sensitive on the *short* side -- these are minimums, not
 * windows -- so it never broke protocol correctness, just made every
 * display bring-up dramatically slower than the numbers in this file
 * suggest at a glance). Recalibrated: 216000 / (1468/50) = ~7357. */
static void disp_delay_ms(uint32_t ms)
{
    for (volatile uint32_t i = 0; i < (7357U * ms); i++)
        ;
}

static void disp_delay_us(uint32_t us)
{
    for (volatile uint32_t i = 0; i < (8U * us); i++)
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

    /* Configure PLLSAI: 25 MHz / 25 * 384 / 5 / 8 = 9.6 MHz -> LTDC.
     *
     * This was 384/7/2 = 27.429 MHz (the ST BSP's own figure) for this
     * entire investigation. Directly dumping Zephyr's LIVE, running DSI
     * register values via OpenOCD on this exact board (not reading
     * source -- reading the actual hardware state) and reverse-computing
     * them exposed this: DSI_VHSACR=13, DSI_VHBPCR=221, DSI_VLCR=5664 for
     * porches HSA=2/HBP=34/HFP=34 (matching this driver's own PANEL_HSYNC
     * etc.) and lane byte clock 62.5 MHz (matching this driver's own
     * DSI PLL, unchanged) only solve consistently for an LCD_CLOCK_KHZ of
     * 9600, not 27429 -- confirmed independently by Zephyr's own board
     * overlay: &pllsai { div-m=25 mul-n=384 div-r=5 div-divr=8 }, i.e.
     * 25/25*384/5/8 = 9.6 MHz, not the BSP's /7/2 = 27.429 MHz. This is
     * a ~2.86x difference in the actual LTDC pixel clock frequency, which
     * would explain the entire session's symptom pattern: it only
     * affects the LTDC-clocked video path (DSI byte-clock-relative porch
     * timing all derives from this), not the DSI PLL-clocked command
     * link (proven robust all session, including bidirectional reads),
     * since the video path and command path have independent clock
     * sources. LCD_CLOCK_KHZ below must move with this. */
    uint32_t pllsaicfgr = RCC_PLLSAICFGR;
    pllsaicfgr &= ~(RCC_PLLSAICFGR_PLLSAIN_Msk | RCC_PLLSAICFGR_PLLSAIR_Msk);
    pllsaicfgr |= (384U << RCC_PLLSAICFGR_PLLSAIN_Pos);
    pllsaicfgr |= (5U << RCC_PLLSAICFGR_PLLSAIR_Pos);
    RCC_PLLSAICFGR = pllsaicfgr;

    /* LTDC clock = PLLSAI / 8. */
    uint32_t dckcfgr1 = RCC_DCKCFGR1;
    dckcfgr1 &= ~RCC_DCKCFGR1_PLLSAIDIVR_Msk;
    dckcfgr1 |= (2U << RCC_DCKCFGR1_PLLSAIDIVR_Pos); /* 2 == /8 */
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
    /* This loop's iteration budget must be measured in real elapsed time,
     * not command count: with DSI->MCR.CMDM correctly cleared (real Video
     * Mode, matching Zephyr's verified-working configuration), each LP
     * command written to GHCR/GPDR only actually drains from the FIFO
     * when the DSI wrapper's live video stream reaches one of the
     * DSI_VMCR LP command windows (LPHFPE/LPHBPE/LPVAE/... -- see
     * dsi_video_mode_init()), not immediately on write. The previous
     * 100000-iteration budget (a few ms of busy-wait) was only ever
     * enough because, until the DSI->MCR CMDM fix, this driver never
     * actually left Command Mode -- LP writes in Command Mode complete
     * immediately with no windowing, so the short timeout never mattered.
     * Confirmed on real hardware: after clearing CMDM, otm8009a_init()'s
     * very first commands (#5 onward) hit FIFO-EMPTY-TIMEOUT back to
     * back, and AN4860 documents up to several seconds worst case for a
     * command to find an open LP window depending on LPSIZE/line timing.
     * Bumped generously (worth well over a second of busy-wait even at a
     * pessimistic few cycles/iteration) so genuine window-wait latency is
     * tolerated instead of misreported as a real link failure. */
    uint32_t timeout = 2000000U;
    while (!(DSI->GPSR & DSI_GPSR_CMDFE_Msk)) {
        if (--timeout == 0U)
            return -1;
    }
    return 0;
}

static int dsi_short_write(uint8_t cmd, const uint8_t *param, uint8_t nparam)
{
    if (dsi_wait_cmd_fifo_empty() < 0) {
#if DSI_TRACE_ENABLE
        char buf[48];
        snprintf(buf, sizeof(buf), "SW#%lu FIFO-EMPTY-TIMEOUT\r\n",
                 (unsigned long)g_dsi_trace_seq);
        hal_uart_puts(buf);
        g_dsi_trace_seq++;
#endif
        return -1;
    }

    uint8_t dt = (nparam == 0U) ? DSI_DCS_SHORT_WRITE0 : DSI_DCS_SHORT_WRITE1;
    uint32_t p = (nparam > 0U) ? param[0] : 0U;
    uint32_t ghcr = (dt << DSI_GHCR_DT_Pos) |
                    (0U << DSI_GHCR_VCID_Pos) |
                    ((uint32_t)cmd << DSI_GHCR_WCLSB_Pos) |
                    (p << DSI_GHCR_WCMSB_Pos);
    DSI->GHCR = ghcr;
#if DSI_TRACE_ENABLE
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "SW#%lu dt=%02x cmd=%02x p=%02x ghcr=%08lx\r\n",
                 (unsigned long)g_dsi_trace_seq, dt, cmd, (unsigned)p,
                 (unsigned long)ghcr);
        hal_uart_puts(buf);
        g_dsi_trace_seq++;
    }
#endif
    return 0;
}

static int dsi_long_write(uint8_t cmd, const uint8_t *params, uint32_t nparams)
{
    if (dsi_wait_cmd_fifo_empty() < 0) {
#if DSI_TRACE_ENABLE
        char buf[48];
        snprintf(buf, sizeof(buf), "LW#%lu FIFO-EMPTY-TIMEOUT\r\n",
                 (unsigned long)g_dsi_trace_seq);
        hal_uart_puts(buf);
        g_dsi_trace_seq++;
#endif
        return -1;
    }

#if DSI_TRACE_ENABLE
    uint32_t words[8];
    uint32_t nwords = 0;
#endif

    uint32_t first = cmd;
    uint32_t nb = (nparams < 3U) ? nparams : 3U;
    for (uint32_t i = 0; i < nb; i++)
        first |= ((uint32_t)params[i] << (8U + 8U * i));
    DSI->GPDR = first;
#if DSI_TRACE_ENABLE
    if (nwords < 8U)
        words[nwords++] = first;
#endif

    uint32_t remaining = nparams - nb;
    const uint8_t *p = params + nb;
    while (remaining > 0U) {
        nb = (remaining < 4U) ? remaining : 4U;
        uint32_t word = 0U;
        for (uint32_t i = 0; i < nb; i++)
            word |= ((uint32_t)p[i] << (8U * i));
        DSI->GPDR = word;
#if DSI_TRACE_ENABLE
        if (nwords < 8U)
            words[nwords++] = word;
#endif
        p += nb;
        remaining -= nb;
    }

    uint32_t ghcr = (DSI_DCS_LONG_WRITE << DSI_GHCR_DT_Pos) |
                    (0U << DSI_GHCR_VCID_Pos) |
                    (((nparams + 1U) & 0xFFU) << DSI_GHCR_WCLSB_Pos) |
                    ((((nparams + 1U) >> 8U) & 0xFFU) << DSI_GHCR_WCMSB_Pos);
    DSI->GHCR = ghcr;
#if DSI_TRACE_ENABLE
    {
        char buf[160];
        int off = snprintf(buf, sizeof(buf), "LW#%lu cmd=%02x n=%lu ghcr=%08lx w=[",
                            (unsigned long)g_dsi_trace_seq, cmd,
                            (unsigned long)nparams, (unsigned long)ghcr);
        for (uint32_t i = 0; i < nwords && off < (int)sizeof(buf) - 12; i++)
            off += snprintf(buf + off, sizeof(buf) - (size_t)off, "%08lx ",
                             (unsigned long)words[i]);
        snprintf(buf + off, sizeof(buf) - (size_t)off, "]\r\n");
        hal_uart_puts(buf);
        g_dsi_trace_seq++;
    }
#endif
    return 0;
}

/* DCS short read (e.g. Read Display Power Mode, Read Display ID): every
 * verification this session has done up to this point was transmit-only.
 * This is a genuine two-way test -- if the panel answers with sane data,
 * the link is electrically healthy end to end (including the reverse/LP
 * direction and turn-around timing, none of which a write-only trace can
 * exercise); if it times out or returns garbage, that points squarely at
 * the physical link rather than anything host-side register content
 * could ever show. Algorithm matches ST's real HAL_DSI_Read()
 * (stm32f7xx_hal_dsi.c), verified against the actual vendored source
 * rather than assumed. */
static int dsi_dcs_read(uint8_t cmd, uint8_t *out, uint32_t nbytes)
{
    if (dsi_wait_cmd_fifo_empty() < 0)
        return -1;

    DSI->GHCR = (DSI_DCS_SHORT_READ << DSI_GHCR_DT_Pos) |
                (0U << DSI_GHCR_VCID_Pos) |
                ((uint32_t)cmd << DSI_GHCR_WCLSB_Pos) |
                (0U << DSI_GHCR_WCMSB_Pos);

    uint32_t remaining = nbytes;
    uint8_t *p = out;
    uint32_t timeout = 1000000U;
    while (remaining > 0U) {
        if (!(DSI->GPSR & DSI_GPSR_PRDFE_Msk)) {
            uint32_t word = DSI->GPDR;
            uint32_t nb = (remaining < 4U) ? remaining : 4U;
            for (uint32_t i = 0; i < nb; i++) {
                p[i] = (uint8_t)(word >> (8U * i));
            }
            p += nb;
            remaining -= nb;
        }
        if (--timeout == 0U)
            return -1;
    }
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
        0xD0, 0x90, 0xE0, 0xF0, 0x00, OTM8009A_COLMOD_RGB888, 0x77, 0x7F, 0x2C, 0x02, 0xFF, 0x00,
        0x00, 0x00, 0x66, 0xB6, 0x06, 0xB1, 0x05
    };

    int ret = 0;

    /* Enter CMD2 mode. */
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[1], 0);
    ret += otm8009a_write_reg(0xFF, lcd_reg_data1, sizeof(lcd_reg_data1));
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[2], 0);
    ret += otm8009a_write_reg(0xFF, lcd_reg_data2, sizeof(lcd_reg_data2));

    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[2], 0);
    ret += otm8009a_write_reg(0xC4, &short_reg_data[3], 0);
    disp_delay_ms(50);
    ret += otm8009a_write_reg(OTM8009A_CMD_NOP, &short_reg_data[4], 0);
    ret += otm8009a_write_reg(0xC4, &short_reg_data[5], 0);
    disp_delay_ms(50);

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
    disp_delay_ms(300);

    /* RGB565 pixel format. */
    ret += otm8009a_write_reg(OTM8009A_CMD_COLMOD, &short_reg_data[37], 1);

    /* Landscape orientation. */
    uint8_t madctr = OTM8009A_MADCTR_LANDSCAPE;
    ret += otm8009a_write_reg(OTM8009A_CMD_MADCTR, &madctr, 1);
    ret += otm8009a_write_reg(OTM8009A_CMD_CASET, lcd_reg_data27, sizeof(lcd_reg_data27));
    ret += otm8009a_write_reg(OTM8009A_CMD_PASET, lcd_reg_data28, sizeof(lcd_reg_data28));

    /* CABC / brightness. WRDISBV/WRCTRLD/WRCABCMB keep the original ST
     * BSP values, but WRCABC's parameter (short_reg_data[41]) was 0x02
     * ("User Interface Image" CABC mode -- the panel adaptively dims
     * itself based on displayed content) and is now 0x00 (CABC off).
     * Cross-checked against a third independent implementation of this
     * exact panel -- the mainline Linux DRM driver
     * (drivers/gpu/drm/panel/panel-orisetech-otm8009a.c) -- whose
     * otm8009a_init_sequence() writes MIPI_DCS_WRITE_POWER_SAVE (this
     * same command, 0x55) with 0x00, i.e. CABC explicitly disabled, and
     * never touches WRDISBV/WRCTRLD/WRCABCMB at all. Every other DCS
     * command byte in this function was independently byte-for-byte
     * matched against that same Linux driver's init sequence -- this
     * was the one real difference. Content-adaptive dimming is a
     * concrete, plausible mechanism for exactly the "picture appears
     * then fades to black/color bands" symptom this driver has shown
     * repeatedly: the panel's own internal CABC algorithm reducing
     * brightness in response to the actual image data, independent of
     * anything on the DSI/LTDC signal path (which live testing has
     * shown to be clean -- zero ISR errors, correct framebuffer
     * content, backlight GPIO genuinely driven high). */
    ret += otm8009a_write_reg(OTM8009A_CMD_WRDISBV, &short_reg_data[39], 1);
    /* Tried clearing WRCTRLD entirely (BCTRL/DD/BL all off, 0x00) after
     * a stripetest run (a single 40px-wide white stripe against an
     * otherwise all-black screen) showed the whole screen going full
     * white regardless of the stripe's position -- on the theory that
     * DD (Display Dimming) applies its own content-based boosting
     * independent of the WRCABC mode selector. Measured on hardware:
     * worse, not better -- nothing displayed at all, not even the
     * stripe, suggesting BL (bit2) is a basic display-output enable
     * this panel actually needs, not purely a "content-adaptive" extra.
     * Reverted to the original ST BSP value (0x2C). */
    ret += otm8009a_write_reg(OTM8009A_CMD_WRCTRLD, &short_reg_data[40], 1);
    {
        static const uint8_t wrcabc_off = 0x00U;
        ret += otm8009a_write_reg(OTM8009A_CMD_WRCABC, &wrcabc_off, 1);
    }
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
    disp_delay_ms(100);
    hal_gpio_write(DISP_RESET_PORT, DISP_RESET_PIN, 1);
    disp_delay_ms(100);
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

    /* Tried ODF=1 (250 Mbps/lane) instead of 0 (500 Mbps/lane) this
     * session -- on the theory that halftest's horizontal-split-only
     * artifact (under-half brightness + spreading on LEFT/RIGHT,
     * perfectly stable on TOP/BOTTOM even given 40s) was a marginal
     * source-driver settling-time issue that more per-bit time might
     * fix. Measured: zero change -- the artifact is bitrate-
     * independent, ruling out a settling-time-margin explanation
     * (a genuine settling problem would improve with more time per
     * bit). This is on top of an earlier, pre-this-session test of the
     * same halving that also found no improvement. Reverted to the
     * reference 500 Mbps/lane value (matches ST BSP/AN4860/Zephyr). */
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

    /* D-PHY clock control enabled, but Automatic Clock lane Control
     * (ACR) LEFT OFF -- i.e. CONTINUOUS clock, not non-continuous.
     *
     * Found by reading the devicetree all the way through: Zephyr's DSI
     * host binding has a "non-continuous" boolean property
     * (dsi_stm32.c maps it straight to AutomaticClockLaneControl /
     * DSI_CLCR.ACR), and neither
     * boards/shields/st_b_lcd40_dsi1_mb1166/boards/stm32f769i_disco.overlay
     * nor the shield's own overlay set it for this exact panel -- a
     * devicetree boolean property that is absent is false, so Zephyr's
     * actual verified-working configuration runs the clock lane
     * CONTINUOUSLY (always HS, never toggling to LP between
     * transmissions) rather than switching it to LP automatically
     * between HS bursts. This driver had ACR enabled (non-continuous),
     * which was never checked against anything before now. Continuous
     * clock gives the panel's D-PHY receiver a stable, always-present HS
     * clock to stay locked to, instead of having to re-lock every time
     * the clock lane comes back from LP; with dozens of LP command
     * insertions and blanking-period transitions throughout every
     * frame's video stream, needing to re-lock that often is a plausible
     * source of exactly this driver's symptom (intermittent/degrading
     * video content while the LP-only command link stays perfectly
     * healthy in both directions). */
    DSI->CLCR &= ~(DSI_CLCR_DPCC_Msk | DSI_CLCR_ACR_Msk);
    DSI->CLCR |= DSI_CLCR_DPCC_Msk;

    /* Flow control: BTA enabled so commands can be acknowledged. Live-
     * checked in full (not just this one bit): PCR = 0x00000004 on both
     * this driver and a known-good Zephyr run -- only BTAE set, every
     * other flow-control bit (ECC/CRC RX, EOTP RX/TX) matches (both
     * clear). Ruled out as a candidate for halftest's horizontal-only
     * artifact. */
    DSI->PCR |= DSI_FLOW_CONTROL_BTAE;

    /* Real HAL_DSI_Init() (stm32f7xx_hal_dsi.c) disables the DSI Host
     * again here (__HAL_DSI_DISABLE) after using CR.EN briefly, earlier
     * in this same function, just to configure the PHY/PCONFR/lane
     * setup -- it stays disabled until HAL_DSI_Start() is explicitly
     * called later. Tried matching that exactly (disabling CR.EN here);
     * measured worse on hardware (nothing visible at all, vs. a real
     * half-width picture without this change), so left un-disabled. Real
     * HAL fidelity on this specific point doesn't help in this driver's
     * actual (structurally different) bring-up sequence, same lesson as
     * the LTDC enable-timing experiment right below this function. */

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

    /* DSI->MCR.CMDM is deliberately NOT cleared in this function.
     *
     * Traced Zephyr's actual driver source (not just its final register
     * values): drivers/display/display_otm8009a.c's otm8009a_init()
     * calls mipi_dsi_attach() -- HAL_DSI_Init()+ConfigVideoMode()+Start()
     * -- which enters real Video Mode BEFORE a single panel command is
     * sent, so every OTM8009A init command in Zephyr's proven-working
     * flow goes out as an LP command embedded in already-running HS
     * video. This was tried directly on this hardware, matching that
     * real order exactly (CMDM cleared here, then otm8009a_init() called
     * with DSI already in Video Mode) -- twice: once with every VMCR LP
     * command window disabled (immediate failure, no window ever open
     * for a command to drain into), and once more after correcting VMCR
     * to 0x0000ff02 (every LP window + FBTAAE enabled, live-verified
     * against a known-good Zephyr run -- see the LP command window
     * comment below). Both times: FIFO-EMPTY-TIMEOUT starting within the
     * first handful of commands, several-hundred-ms settle delays before
     * the first command included. Whatever mechanism actually lets
     * Zephyr's mipi_dsi_generic_write() succeed here that this driver's
     * blocking dsi_wait_cmd_fifo_empty()-then-write approach does not
     * remains unidentified -- possibly multi-frame retry/interrupt-driven
     * completion on Zephyr's side vs. a single blocking poll here. CMDM
     * is cleared later instead, in hal_display_start_video(), via a full
     * DSI+LTDC peripheral reset-and-restart after otm8009a_init() has
     * already completed in Command Mode -- see that function's own
     * comment for why, and how it was verified to avoid the DSI protocol
     * errors this reset used to cause. */

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

    /* Polarity: all active high -- which means these bits CLEAR, not set.
     *
     * This was backwards for the entire investigation. The actual ST HAL
     * constants (stm32f7xx_hal_dsi.h) are:
     *   DSI_HSYNC_ACTIVE_HIGH/DSI_VSYNC_ACTIVE_HIGH/
     *   DSI_DATA_ENABLE_ACTIVE_HIGH = 0x00000000 (bit CLEAR)
     *   DSI_*_ACTIVE_LOW = DSI_LPCR_HSP/VSP/DEP (bit SET)
     * i.e. setting DSI_LPCR's HSP/VSP/DEP bits selects ACTIVE LOW, and
     * clearing them selects ACTIVE HIGH -- the opposite of what the
     * previous version of this code (and comment) assumed. Confirmed by
     * dumping Zephyr's live, running DSI_LPCR register on this exact
     * board: 0x00000000, with its devicetree requesting
     * hs-active-high/vs-active-high/de-active-high (matching this
     * driver's own intent) -- i.e. all bits clear is what "active high"
     * actually is on real hardware. This driver previously computed the
     * mask correctly but then set the bits instead of leaving them
     * clear, silently configuring active-low sync/DE on the DSI host
     * side (independent of, and in addition to, the LTDC-side polarity
     * bug already fixed earlier in this investigation). */
    DSI->LPCR &= ~(DSI_LPCR_DEP_Msk | DSI_LPCR_VSP_Msk | DSI_LPCR_HSP_Msk);

    /* Color coding: RGB888 on the wire (matches the ST reference for this
     * exact panel/BSP), independent of the LTDC layer's own RGB565 memory
     * pixel format -- the LTDC always expands each layer to a full
     * internal ARGB representation before driving its parallel RGB bus
     * into the wrapper, so the framebuffer can stay RGB565 either way.
     * The OTM8009A's own COLMOD register is set to match (0x77). */
    DSI->LCOLCR &= ~DSI_LCOLCR_COLC_Msk;
    DSI->LCOLCR |= (DSI_RGB888 << DSI_LCOLCR_COLC_Pos);
    DSI->LCOLCR &= ~DSI_LCOLCR_LPE_Msk;

    DSI->WCFGR &= ~DSI_WCFGR_COLMUX_Msk;
    DSI->WCFGR |= (DSI_RGB888 << DSI_WCFGR_COLMUX_Pos);

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

    /* Low-power command settings.
     *
     * LPSIZE/VLPSIZE previously set to 28/8, derived from AN4860's ("LP
     * command packet size") formulas for this exact line/porch timing.
     * That calculation was never actually checked against a working
     * reference, though -- reading Zephyr's dsi_stm32.c devicetree
     * handling all the way through shows its "largest-packet-size"
     * property (which sets BOTH LPLargestPacketSize/LPSIZE and
     * LPVACTLargestPacketSize/VLPSIZE to the same value) defaults to 4
     * when absent, via DT_INST_PROP_OR(..., 4) -- and this exact
     * board+panel's devicetree does not set it, so the actual
     * verified-working value is 4/4, not a formula-derived 28/8. */
    /* LP command windows + FBTAAE: previously disabled entirely (see git
     * history), reasoning from indirect signals (source reading,
     * inference from devicetree property defaults) that went back and
     * forth without a real fix -- disabling everything gave a picture
     * squeezed into the left half; enabling just LP windows without
     * FBTAAE gave ~2px; enabling everything except FBTAAE still showed
     * DSI->ISR1 bit7 (LPWRE) firing continuously post-boot, correlated
     * with the picture fading to color bands shortly after appearing.
     *
     * Settled by direct evidence instead: halted this exact board live
     * via OpenOCD while running a known-good Zephyr build (its own
     * DSI+LTDC+otm8009a driver, verified visually stable/non-fading on
     * this hardware) and dumped its actual running DSI->VMCR: 0x0000ff02.
     * Every
     * LP window bit is set (LPCE/LPHFPE/LPHBPE/LPVAE/LPVFPE/LPVBPE/
     * LPVSAE) together with FBTAAE, on top of VMT=Burst (bits0-1=2) --
     * i.e. the real verified-working config enables all of it together,
     * not the partial/none combinations tried by inference alone. Every
     * other DSI video-timing register this driver computes (VHSACR,
     * VHBPCR, VLCR, VVSACR, VVBPCR, VVFPCR, VVACR, LPMCR, CCR, WPCR[0],
     * WRPCR) was cross-checked against this same live dump and already
     * matches exactly -- VMCR's LP/FBTAA bits were the one real
     * remaining discrepancy. */
    DSI->VMCR |= (DSI_VMCR_LPCE_Msk | DSI_VMCR_LPHFPE_Msk | DSI_VMCR_LPHBPE_Msk |
                  DSI_VMCR_LPVAE_Msk | DSI_VMCR_LPVFPE_Msk | DSI_VMCR_LPVBPE_Msk |
                  DSI_VMCR_LPVSAE_Msk | DSI_VMCR_FBTAAE_Msk);

    DSI->LPMCR &= ~(DSI_LPMCR_LPSIZE_Msk | DSI_LPMCR_VLPSIZE_Msk);
    DSI->LPMCR |= (4U << DSI_LPMCR_LPSIZE_Pos);
    DSI->LPMCR |= (4U << DSI_LPMCR_VLPSIZE_Pos);

    {
        char dbuf[160];
        snprintf(dbuf, sizeof(dbuf),
                 "[DSI] timing hsa=%lu hbp=%lu hline=%lu vsa=%lu vbp=%lu vfp=%lu vact=%lu MCR=%08lx VMCR=%08lx\r\n",
                 (unsigned long)hsa_byte, (unsigned long)hbp_byte, (unsigned long)hline_byte,
                 (unsigned long)vsa, (unsigned long)vbp, (unsigned long)vfp, (unsigned long)vact,
                 (unsigned long)DSI->MCR, (unsigned long)DSI->VMCR);
        hal_uart_puts(dbuf);
    }

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

    /* Enable LTDC right after the global timing registers, matching real
     * HAL_LTDC_Init()'s own order (enables GCR.LTDCEN as its last step,
     * BEFORE any layer is configured) and Zephyr's driver, which calls
     * HAL_LTDC_Init() then HAL_LTDC_ConfigLayer() as two separate calls
     * in that order. A live GCR dump from a known-good Zephyr run on
     * this exact board also showed HSPOL/VSPOL/DEPOL/PCPOL (GCR's top
     * nibble) all clear -- i.e. active-high, matching this driver's own
     * DSI-side polarity (DSI_LPCR) -- contradicting an earlier comment
     * here (removed) that inferred active-low from devicetree reading
     * alone without ever live-checking the register; this driver never
     * actually wrote those bits either way, so nothing to change there,
     * just correcting the record. */
    LTDC->GCR = LTDC_GCR_LTDCEN_Msk;

    /* Layer 1: RGB565, full screen. */
    LTDC_LAYER1->CR = 0U;
    LTDC_LAYER1->WHPCR = ((hsync + hbp + DISPLAY_WIDTH - 1U) << 16) |
                         (hsync + hbp);
    LTDC_LAYER1->WVPCR = ((vsync + vbp + DISPLAY_HEIGHT - 1U) << 16) |
                         (vsync + vbp);
    LTDC_LAYER1->PFCR = LTDC_PIXEL_FORMAT_RGB565;
    LTDC_LAYER1->CACR = 0xFFU;
    /* Blend factors: live-captured from a known-good Zephyr run on this
     * exact board as 0x0607 (BF1=6 pixel-alpha *
     * constant-alpha, BF2=7 1-(pixel-alpha*constant-alpha)), not
     * 0x0405 (BF1=4/BF2=5, constant-alpha only / its inverse) as this
     * driver had it. For RGB565 (no per-pixel alpha channel) the LTDC
     * synthesizes pixel-alpha=0xFF internally so the two are usually
     * equivalent in output, but matched exactly now that every other
     * LTDC register has been cross-checked against the same live dump. */
    LTDC_LAYER1->BFCR = (6U << 8) | (7U << 0);
    LTDC_LAYER1->CFBAR = fb_addr;
    LTDC_LAYER1->CFBLR = ((DISPLAY_WIDTH * DISPLAY_BPP) << 16) |
                         (DISPLAY_WIDTH * DISPLAY_BPP + 3U);
    LTDC_LAYER1->CFBLNR = DISPLAY_HEIGHT;
    LTDC_LAYER1->CR = LTDC_LAYER_CR_LEN;

    LTDC->SRCR = LTDC_SRCR_IMR;
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
    hal_uart_puts("[DISP] reset done, DSI/LTDC not yet touched\r\n");

    /* hal_display_init() stops here -- no DSI/LTDC/panel work at all.
     * Callers must draw the desired first-frame content into the
     * framebuffer (hal_display_fb_addr()) and then call
     * hal_display_start_video() once, which now does the ENTIRE DSI/
     * LTDC/panel bring-up (including otm8009a_init()) in one place,
     * matching the real ST BSP's BSP_LCD_InitEx() structure exactly --
     * see that function's comment for the priming Start/Stop cycle this
     * driver was missing for this whole investigation. This still
     * avoids the tearing race that motivated the original split: LTDC
     * used to start scanning the framebuffer before the caller had
     * drawn into it -- see git history for that original fix. */
}

void hal_display_start_video(void)
{
    hal_uart_puts("[DISP] starting video\r\n");

    if (dsi_host_init() < 0) {
        hal_uart_puts("[DISP] dsi_host_init failed\r\n");
        return;
    }

    /* Priming Start/Stop cycle, matching the REAL ST BSP's
     * BSP_LCD_InitEx() (Drivers/BSP/STM32F769I-Discovery/
     * stm32f769i_discovery_lcd.c) structure exactly -- fetched and read
     * directly from ST's own GitHub source this session, not inferred:
     * it calls HAL_DSI_Init() then HAL_DSI_Start() immediately (BEFORE
     * HAL_DSI_ConfigVideoMode() is ever called, so still Command Mode,
     * MCR.CMDM at its power-on-reset value), does a panel ID read,
     * then HAL_DSI_Stop() -- all before the real video-mode bring-up
     * (ConfigVideoMode + LTDC_Init + a SECOND Start) even begins. This
     * driver had never replicated that priming cycle before -- every
     * earlier attempt at a single-incarnation, video-mode-from-the-
     * start approach (this driver's own architecture, or Zephyr's, or
     * just matching Zephyr's final register values) went straight from
     * dsi_host_init() into video-mode config with no priming step, and
     * reproducibly failed with FIFO-EMPTY-TIMEOUT once otm8009a_init()
     * tried to send commands as LP-in-video. Testing whether this
     * priming cycle -- which conditions the D-PHY/PLL analog state with
     * one real enable/disable cycle before the bring-up that actually
     * matters -- is what every earlier attempt was missing. */
    DSI->CR |= DSI_CR_EN_Msk;
    DSI->WCR |= DSI_WCR_DSIEN_Msk;
    disp_delay_ms(10U);
    {
        uint8_t power_mode = 0;
        if (dsi_dcs_read(0x0AU, &power_mode, 1U) == 0) {
            char buf[48];
            snprintf(buf, sizeof(buf), "[DISP] priming power mode read: 0x%02x\r\n",
                     power_mode);
            hal_uart_puts(buf);
        } else {
            hal_uart_puts("[DISP] priming power mode read: TIMEOUT\r\n");
        }
    }
    DSI->WCR &= ~DSI_WCR_DSIEN_Msk;
    DSI->CR &= ~DSI_CR_EN_Msk;
    disp_delay_ms(10U);

    /* Real video-mode bring-up now. dsi_video_mode_init() configures
     * VMCR/VHSACR/etc.; MCR.CMDM is cleared right after, matching
     * HAL_DSI_ConfigVideoMode()'s own first step (it clears MCR.CMDM
     * and WCFGR.DSIM before touching any of the video timing
     * registers -- WCFGR.DSIM is already 0 by reset default on this
     * hardware, live-verified against Zephyr, so only CMDM needs an
     * explicit clear here). */
    if (dsi_video_mode_init() < 0) {
        hal_uart_puts("[DISP] dsi_video_mode_init failed\r\n");
        return;
    }
    DSI->MCR &= ~DSI_MCR_CMDM_Msk;
    /* dsi_phy_timers_init() is deliberately not called -- live-verified
     * against a known-good Zephyr run that CLTCR/DLTCR should stay at
     * their power-on-reset value here, and ST's own BSP_LCD_InitEx()
     * never calls HAL_DSI_ConfigPhyTimer() for this panel either. */

    ltdc_init(g_fb_addr); /* enables LTDC (GCR.LTDCEN) inline, matching
                            * real HAL_LTDC_Init()'s own order -- ST's
                            * BSP_LCD_InitEx() also calls this (via
                            * HAL_LTDC_StructInitFromVideoConfig() +
                            * HAL_LTDC_Init()) BEFORE its second/real
                            * HAL_DSI_Start(), with the comment "Enable
                            * the DSI host and wrapper after the LTDC
                            * initialization. To avoid any
                            * synchronization issue, the DSI shall be
                            * started after enabling the LTDC" -- this
                            * driver already does exactly that. */

    /* The real Start -- this DSI Host/Wrapper incarnation is now "born"
     * already in Video Mode (CMDM cleared above, before this enable),
     * with the priming cycle already having exercised a full enable/
     * disable transition once beforehand. */
    DSI->CR |= DSI_CR_EN_Msk;
    DSI->WCR |= DSI_WCR_DSIEN_Msk;

    /* Settle gap mirroring the incidental delay ST's own reference gets
     * for free here: BSP_LCD_InitEx() calls BSP_SDRAM_Init() at exactly
     * this point in its sequence (between the real HAL_DSI_Start() and
     * OTM8009A_Init()) when SDRAM isn't already set up by the caller.
     * This driver's SDRAM is already initialized by this point (the
     * caller does it before hal_display_init()), so there's no
     * equivalent real work to insert here -- an explicit delay stands
     * in for it. */
    disp_delay_ms(50U);

    hal_uart_puts("[DISP] sending OTM init (as LP-in-video, single incarnation, ST BSP order)\r\n");
    __asm volatile ("cpsid i" ::: "memory");
    int otm_ret = otm8009a_init();
    __asm volatile ("cpsie i" ::: "memory");
    if (otm_ret < 0) {
        hal_uart_puts("[DISP] otm8009a_init failed\r\n");
        return;
    }

    LTDC->ICR = LTDC_ICR_CFUIF_Msk | LTDC_ICR_CTERRIF_Msk;
    LTDC->IER |= LTDC_IER_FUIE_Msk | LTDC_IER_TERRIE_Msk;

    hal_display_backlight_on();
    hal_uart_puts("[DISP] backlight on\r\n");

    /* Diagnostic: the picture has repeatedly been observed to appear
     * briefly then fade/turn off within roughly a second, both before and
     * after this window's changes -- most recently as "color bands that
     * appear then extinguish" (a Zephyr-confirmed-good picture stays
     * stable indefinitely on this exact hardware, so this fade is a real
     * saramOS-side defect). Poll DSI->ISR0/ISR1 and (now) LTDC->ISR's
     * FUIF/TERRIF (real, read-to-clear error flags -- see
     * dsi_wait_cmd_fifo_empty()'s comment for why these, not visual
     * symptoms, are the trustworthy signal) every ~100ms for two seconds
     * right after backlight-on, printing any flags seen, to catch
     * whatever hardware error correlates with the moment the picture
     * actually fades/bands. */
    for (uint32_t i = 0; i < 20U; i++) {
        disp_delay_ms(100U);
        uint32_t isr0 = DSI->ISR[0];
        uint32_t isr1 = DSI->ISR[1];
        uint32_t ltdc_isr = LTDC->ISR & (LTDC_ISR_FUIF_Msk | LTDC_ISR_TERRIF_Msk);
        if (isr0 != 0U || isr1 != 0U || ltdc_isr != 0U) {
            char buf[96];
            snprintf(buf, sizeof(buf), "[DISP] t=%lums ISR0=%08lx ISR1=%08lx LTDC_ISR=%08lx\r\n",
                     (unsigned long)(i + 1U) * 100UL,
                     (unsigned long)isr0, (unsigned long)isr1, (unsigned long)ltdc_isr);
            hal_uart_puts(buf);
        }
        LTDC->ICR = LTDC_ICR_CFUIF_Msk | LTDC_ICR_CTERRIF_Msk;
    }
    hal_uart_puts("[DISP] isr poll done\r\n");
}

/* Point LTDC Layer 1 at a different framebuffer address, taking effect
 * only at the LTDC's own next vertical blanking period (LTDC_SRCR_VBR
 * -- Vertical Blanking Reload -- instead of LTDC_SRCR_IMR's immediate
 * reload). This is the real fix for tearing on redraw, replacing an
 * earlier version of this function that busy-waited for
 * LTDC->ISR.LIF (see hal_display_start_video()'s comment for why
 * IER.LIE has to be set for that flag to latch at all) and then wrote
 * CFBAR immediately: even confined to the blanking window that way, the
 * actual REDRAW (this port's minimal LVGL walking every glyph pixel via
 * put_pixel(), or any full-buffer rewrite) still commonly takes longer
 * than the blanking window itself to complete, so a live buffer could
 * still be caught mid-redraw regardless of when the wait released.
 *
 * The real fix is double buffering: render into a buffer that is NOT
 * the one currently being scanned out at all (as long as rendering
 * takes), then hand this function the finished buffer's address --
 * VBR guarantees the switch itself only ever happens between frames,
 * so scan-out is always reading one complete, finished buffer or the
 * other, never a buffer mid-write. See lvgl_port.c for the two-buffer
 * setup and lv_timer_handler() (lvgl.c) for the alternation between
 * them on every redraw. */
void hal_display_flip(uint32_t fb_addr)
{
    LTDC_LAYER1->CFBAR = fb_addr;
    LTDC->SRCR = LTDC_SRCR_VBR;
}

uint32_t hal_display_fb_addr(void)
{
    return g_fb_addr;
}

int hal_display_read_power_mode(uint8_t *out)
{
    return dsi_dcs_read(0x0AU, out, 1U);
}

void hal_display_backlight_on(void)
{
    hal_gpio_write(DISP_BACKLIGHT_PORT, DISP_BACKLIGHT_PIN, 1);
}

void hal_display_backlight_off(void)
{
    hal_gpio_write(DISP_BACKLIGHT_PORT, DISP_BACKLIGHT_PIN, 0);
}
