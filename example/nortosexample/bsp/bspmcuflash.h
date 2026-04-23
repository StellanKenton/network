/************************************************************************************
* @file     : bspmcuflash.h
* @brief    : Board-specific MCU flash adapter.
* @details  : Provides the project BSP hooks for internal flash access and a thin
*             storage wrapper used by project-side update bindings.
* @author   : GitHub Copilot
* @date     : 2026-04-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef BSPMCUFLASH_H
#define BSPMCUFLASH_H

#include <stdbool.h>
#include <stdint.h>

#include "../../rep/driver/drvmcuflash/drvmcuflash.h"

#ifdef __cplusplus
extern "C" {
#endif

bool bspMcuFlashInit(void);
bool bspMcuFlashRead(uint32_t address, uint8_t *buffer, uint32_t length);
bool bspMcuFlashWrite(uint32_t address, const uint8_t *buffer, uint32_t length);
bool bspMcuFlashErase(uint32_t address, uint32_t length);
bool bspMcuFlashIsRangeValid(uint32_t address, uint32_t length);

eDrvStatus bspMcuFlashPortInit(void);
eDrvStatus bspMcuFlashPortUnlock(void);
eDrvStatus bspMcuFlashPortLock(void);
eDrvStatus bspMcuFlashPortEraseSector(uint32_t sectorIndex);
eDrvStatus bspMcuFlashPortProgram(uint32_t address, const uint8_t *buffer, uint32_t length);
eDrvStatus bspMcuFlashPortGetSectorInfo(uint32_t address, uint32_t *sectorIndex, uint32_t *sectorStart, uint32_t *sectorSize);

#ifdef __cplusplus
}
#endif

#endif  // BSPMCUFLASH_H
/**************************End of file********************************/
