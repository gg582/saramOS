#ifndef SD_DISKIO_H
#define SD_DISKIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ff.h"

extern FATFS sd_fatfs;

FRESULT sd_mount(void);
void sd_unmount(void);

#ifdef __cplusplus
}
#endif

#endif /* SD_DISKIO_H */
