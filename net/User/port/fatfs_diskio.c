/***********************************************************************************
* @file     : fatfs_diskio.c
* @brief    : FatFs diskio glue for the project SD card device.
**********************************************************************************/
#include <stdlib.h>

#include "../../rep/lib/fatfs/diskio.h"
#include "../../rep/lib/fatfs/ff.h"
#include "../../rep/module/sdcard/sdcard.h"
#include "../../rep/service/log/log.h"

#define FATFS_SDCARD_PDRV  0U

static eSdcardStatus gFatfsLastInitStatus = SDCARD_STATUS_OK;
static bool gFatfsHasLastInitStatus = false;

DSTATUS disk_initialize(BYTE pdrv)
{
    eSdcardStatus lStatus;

    if (pdrv != FATFS_SDCARD_PDRV) {
        return STA_NOINIT;
    }

    lStatus = sdcardInit(SDCARD_DEV0);
    if (lStatus == SDCARD_STATUS_OK) {
        gFatfsHasLastInitStatus = false;
        return 0U;
    }

    if (!gFatfsHasLastInitStatus || (gFatfsLastInitStatus != lStatus)) {
        LOG_W("fatfs", "sd init fail status=%d", (int)lStatus);
        gFatfsLastInitStatus = lStatus;
        gFatfsHasLastInitStatus = true;
    }

    if (lStatus == SDCARD_STATUS_NO_MEDIUM) {
        return (STA_NOINIT | STA_NODISK);
    }
    if (lStatus == SDCARD_STATUS_WRITE_PROTECTED) {
        return (STA_NOINIT | STA_PROTECT);
    }
    return STA_NOINIT;
}

DSTATUS disk_status(BYTE pdrv)
{
    bool lIsPresent = false;
    bool lIsWriteProtected = false;
    DSTATUS lDiskStatus = 0U;
    eSdcardStatus lStatus;

    if (pdrv != FATFS_SDCARD_PDRV) {
        return STA_NOINIT;
    }

    lStatus = sdcardGetStatus(SDCARD_DEV0, &lIsPresent, &lIsWriteProtected);
    if ((lStatus != SDCARD_STATUS_OK) && (lStatus != SDCARD_STATUS_NO_MEDIUM)) {
        return STA_NOINIT;
    }
    if (!lIsPresent) {
        return (STA_NOINIT | STA_NODISK);
    }

    if (!sdcardIsReady(SDCARD_DEV0)) {
        lDiskStatus |= STA_NOINIT;
    }

    if (lIsWriteProtected) {
        lDiskStatus |= STA_PROTECT;
    }

    return lDiskStatus;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    eSdcardStatus lStatus;

    if ((pdrv != FATFS_SDCARD_PDRV) || (buff == NULL) || (count == 0U)) {
        return RES_PARERR;
    }

    lStatus = sdcardReadBlocks(SDCARD_DEV0, sector, buff, count);
    if (lStatus == SDCARD_STATUS_OK) {
        return RES_OK;
    }
    if (lStatus == SDCARD_STATUS_NO_MEDIUM) {
        return RES_NOTRDY;
    }
    if (lStatus == SDCARD_STATUS_INVALID_PARAM) {
        return RES_PARERR;
    }
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    eSdcardStatus lStatus;

    if ((pdrv != FATFS_SDCARD_PDRV) || (buff == NULL) || (count == 0U)) {
        return RES_PARERR;
    }

    lStatus = sdcardWriteBlocks(SDCARD_DEV0, sector, buff, count);
    if (lStatus == SDCARD_STATUS_OK) {
        return RES_OK;
    }
    LOG_W("fatfs", "sd write fail sector=%lu count=%u status=%d", (unsigned long)sector, (unsigned int)count, (int)lStatus);
    if (lStatus == SDCARD_STATUS_WRITE_PROTECTED) {
        return RES_WRPRT;
    }
    if (lStatus == SDCARD_STATUS_NO_MEDIUM) {
        return RES_NOTRDY;
    }
    if (lStatus == SDCARD_STATUS_INVALID_PARAM) {
        return RES_PARERR;
    }
    return RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    const stSdcardInfo *lInfo;
    eSdcardStatus lStatus;

    if (pdrv != FATFS_SDCARD_PDRV) {
        return RES_PARERR;
    }

    lInfo = sdcardGetInfo(SDCARD_DEV0);
    if ((lInfo == NULL) && (cmd != CTRL_SYNC)) {
        return RES_NOTRDY;
    }

    switch (cmd) {
        case CTRL_SYNC:
            lStatus = sdcardSync(SDCARD_DEV0);
            return (lStatus == SDCARD_STATUS_OK) ? RES_OK : RES_ERROR;

        case GET_SECTOR_COUNT:
            if (buff == NULL) {
                return RES_PARERR;
            }
            *(DWORD *)buff = lInfo->blockCount;
            return RES_OK;

        case GET_SECTOR_SIZE:
            if (buff == NULL) {
                return RES_PARERR;
            }
            *(WORD *)buff = (WORD)lInfo->blockSize;
            return RES_OK;

        case GET_BLOCK_SIZE:
            if (buff == NULL) {
                return RES_PARERR;
            }
            *(DWORD *)buff = (lInfo->eraseBlockSize == 0UL) ? 1UL : (lInfo->eraseBlockSize / lInfo->blockSize);
            return RES_OK;

        default:
            return RES_PARERR;
    }
}

DWORD get_fattime(void)
{
    return ((DWORD)(2026U - 1980U) << 25) |
           ((DWORD)4U << 21) |
           ((DWORD)22U << 16);
}

void *ff_memalloc(UINT msize)
{
    return malloc(msize);
}

void ff_memfree(void *mblock)
{
    free(mblock);
}
/**************************End of file********************************/
