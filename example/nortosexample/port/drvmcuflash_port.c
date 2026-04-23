/***********************************************************************************
* @file     : drvmcuflash_port.c
* @brief    : Project MCU flash binding implementation.
* @details  : Binds the reusable drvmcuflash core to the STM32F1 BSP adapter and
*             exposes the bootloader application flash region as a logical area.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvmcuflash_port.h"

#include "../bsp/bspmcuflash.h"

static const stDrvMcuFlashBspInterface gDrvMcuFlashBspInterface = {
    .init = bspMcuFlashPortInit,
    .unlock = bspMcuFlashPortUnlock,
    .lock = bspMcuFlashPortLock,
    .eraseSector = bspMcuFlashPortEraseSector,
    .program = bspMcuFlashPortProgram,
    .getSectorInfo = bspMcuFlashPortGetSectorInfo,
};

const stDrvMcuFlashBspInterface *drvMcuFlashGetPlatformBspInterface(void)
{
    return &gDrvMcuFlashBspInterface;
}

uint8_t drvMcuFlashGetPlatformAreaCount(void)
{
    return (uint8_t)DRVMCUFLASH_AREA_MAX;
}

eDrvStatus drvMcuFlashGetPlatformAreaInfo(uint8_t area, stDrvMcuFlashAreaInfo *info)
{
    if (info == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    switch (area) {
        case DRVMCUFLASH_AREA_BOOT_RECORD:
            info->startAddress = DRVMCUFLASH_BOOT_RECORD_START_ADDR;
            info->size = DRVMCUFLASH_BOOT_RECORD_SIZE;
            return DRV_STATUS_OK;
        case DRVMCUFLASH_AREA_APP:
            info->startAddress = DRVMCUFLASH_APP_START_ADDR;
            info->size = DRVMCUFLASH_APP_SIZE;
            return DRV_STATUS_OK;
        default:
            return DRV_STATUS_INVALID_PARAM;
    }
}

/**************************End of file********************************/
