#include "sd_diskio.h"
#include "diskio.h"
#include <hal/hal_sdmmc.h>
#include <hal/board.h>

FATFS sd_fatfs;
static DSTATUS sd_status = STA_NOINIT;

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0)
        return STA_NOINIT;

    if (hal_sdmmc_init() != HAL_SDMMC_OK)
        return STA_NOINIT;

    sd_status &= ~STA_NOINIT;
    return sd_status;
}

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0)
        return STA_NOINIT;
    return sd_status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buf, LBA_t sector, UINT count)
{
    if (pdrv != 0 || (sd_status & STA_NOINIT))
        return RES_NOTRDY;

    if (hal_sdmmc_read_blocks((uint32_t)sector, buf, count) != HAL_SDMMC_OK)
        return RES_ERROR;

    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buf, LBA_t sector, UINT count)
{
    if (pdrv != 0 || (sd_status & STA_NOINIT))
        return RES_NOTRDY;

    if (hal_sdmmc_write_blocks((uint32_t)sector, buf, count) != HAL_SDMMC_OK)
        return RES_ERROR;

    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buf)
{
    if (pdrv != 0)
        return RES_PARERR;
    if (sd_status & STA_NOINIT)
        return RES_NOTRDY;

    switch (cmd) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        *(DWORD *)buf = hal_sdmmc_get_sector_count();
        return RES_OK;
    case GET_SECTOR_SIZE:
        *(WORD *)buf = HAL_SDMMC_BLOCK_SIZE;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)buf = 1;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}

FRESULT sd_mount(void)
{
    if (disk_initialize(0) & STA_NOINIT)
        return FR_NOT_READY;
    return f_mount(&sd_fatfs, "0:", 1);
}

void sd_unmount(void)
{
    f_mount(NULL, "0:", 1);
    sd_status |= STA_NOINIT;
}
