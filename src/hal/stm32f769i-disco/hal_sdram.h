/*
 * HAL SDRAM initialization for STM32F769I-DISCO.
 * IS42S32400F (or equivalent MT48LC4M32B2) on FMC SDRAM Bank 1
 * (0xC0000000, 16 MB).
 */
#ifndef HAL_SDRAM_H
#define HAL_SDRAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDRAM_BASE_ADDR     0xC0000000U
#define SDRAM_SIZE          (16U * 1024U * 1024U)

/* FMC/SDRAM controller registers */
#define FMC_BASE            0xA0000000U
#define FMC_BCR1            (*(volatile uint32_t *)(FMC_BASE + 0x000U))
#define FMC_SDCR1           (*(volatile uint32_t *)(FMC_BASE + 0x140U))
#define FMC_SDCR2           (*(volatile uint32_t *)(FMC_BASE + 0x144U))
#define FMC_SDTR1           (*(volatile uint32_t *)(FMC_BASE + 0x148U))
#define FMC_SDTR2           (*(volatile uint32_t *)(FMC_BASE + 0x14CU))
#define FMC_SDCMR           (*(volatile uint32_t *)(FMC_BASE + 0x150U))
#define FMC_SDRTR           (*(volatile uint32_t *)(FMC_BASE + 0x154U))
#define FMC_SDSR            (*(volatile uint32_t *)(FMC_BASE + 0x158U))

#define FMC_SDCMR_CTB1      (1U << 4)
#define FMC_SDCMR_CTB2      (1U << 3)
#define FMC_SDCMR_MODE_Pos  0U
#define FMC_SDCMR_MODE_NORMAL       0U
#define FMC_SDCMR_MODE_CLK_EN       1U
#define FMC_SDCMR_MODE_PALL         2U
#define FMC_SDCMR_MODE_AUTO_REFRESH 3U
#define FMC_SDCMR_MODE_LOAD_MODE    4U
#define FMC_SDCMR_NRFS_Pos  5U
#define FMC_SDCMR_MRD_Pos   9U

/* STM32F769I-DISCO SDRAM is on FMC SDRAM Bank 1 (0xC0000000, 16 MB). */

/* Initialize FMC SDRAM controller and memory. */
void hal_sdram_init(void);

/* Return the configured SDRAM base address. */
uint32_t hal_sdram_base(void);
uint32_t hal_sdram_size(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SDRAM_H */
