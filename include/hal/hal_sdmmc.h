#ifndef HAL_SDMMC_H
#define HAL_SDMMC_H

#include <hal/board.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_SDMMC_BLOCK_SIZE    512

/* SD card state returned by hal_sdmmc_init() etc. */
#define HAL_SDMMC_OK            0
#define HAL_SDMMC_ERR           -1
#define HAL_SDMMC_TIMEOUT       -2
#define HAL_SDMMC_NO_CARD       -3

/* SDMMC HAL API */
int      hal_sdmmc_init(void);
int      hal_sdmmc_card_present(void);
uint32_t hal_sdmmc_get_sector_count(void);
int      hal_sdmmc_send_cmd(uint32_t cmd, uint32_t arg, uint32_t *resp);
int      hal_sdmmc_read_blocks(uint32_t lba, uint8_t *buf, uint32_t count);
int      hal_sdmmc_write_blocks(uint32_t lba, const uint8_t *buf, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SDMMC_H */
