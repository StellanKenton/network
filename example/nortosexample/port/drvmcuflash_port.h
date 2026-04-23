/************************************************************************************
* @file     : drvmcuflash_port.h
* @brief    : Project MCU flash area binding for drvmcuflash.
* @details  : Declares the logical area map and default writable application
*             region used by the bootloader update flow.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVMCUFLASH_PORT_H
#define DRVMCUFLASH_PORT_H

#include "../../rep/driver/drvmcuflash/drvmcuflash.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvMcuFlashAreaMap {
    DRVMCUFLASH_AREA_BOOT_RECORD = 0,
    DRVMCUFLASH_AREA_APP,
    DRVMCUFLASH_AREA_MAX,
} eDrvMcuFlashAreaMap;

#define DRVMCUFLASH_BOOT_RECORD_START_ADDR 0x0801F000UL
#define DRVMCUFLASH_BOOT_RECORD_SIZE       0x00001000UL
#define DRVMCUFLASH_APP_START_ADDR         0x08020000UL
#define DRVMCUFLASH_APP_SIZE               0x00060000UL

const stDrvMcuFlashBspInterface *drvMcuFlashGetPlatformBspInterface(void);
uint8_t drvMcuFlashGetPlatformAreaCount(void);
eDrvStatus drvMcuFlashGetPlatformAreaInfo(uint8_t area, stDrvMcuFlashAreaInfo *info);

#ifdef __cplusplus
}
#endif

#endif  // DRVMCUFLASH_PORT_H
/**************************End of file********************************/
