/***********************************************************************************
* @file     : bspmcuflash.c
* @brief    : Board-specific MCU flash adapter.
* @details  : Implements STM32F103 internal flash BSP hooks and exposes a thin
*             project storage wrapper for update bindings.
* @author   : GitHub Copilot
* @date     : 2026-04-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bspmcuflash.h"

#include "stm32f10x_flash.h"

#define BSPMCUFLASH_FLASH_BASE           0x08000000UL
#define BSPMCUFLASH_FLASH_SIZE           0x00080000UL
#define BSPMCUFLASH_PAGE_SIZE            2048UL

bool bspMcuFlashInit(void)
{
    return drvMcuFlashInit();
}

bool bspMcuFlashRead(uint32_t address, uint8_t *buffer, uint32_t length)
{
    return drvMcuFlashRead(address, buffer, length);
}

bool bspMcuFlashWrite(uint32_t address, const uint8_t *buffer, uint32_t length)
{
    return drvMcuFlashWrite(address, buffer, length);
}

bool bspMcuFlashErase(uint32_t address, uint32_t length)
{
    return drvMcuFlashErase(address, length);
}

bool bspMcuFlashIsRangeValid(uint32_t address, uint32_t length)
{
    return drvMcuFlashIsRangeValid(address, length);
}

eDrvStatus bspMcuFlashPortInit(void)
{
    return DRV_STATUS_OK;
}

eDrvStatus bspMcuFlashPortUnlock(void)
{
    FLASH_Unlock();
    return DRV_STATUS_OK;
}

eDrvStatus bspMcuFlashPortLock(void)
{
    FLASH_Lock();
    return DRV_STATUS_OK;
}

eDrvStatus bspMcuFlashPortEraseSector(uint32_t sectorIndex)
{
    uint32_t lPageAddress;

    lPageAddress = BSPMCUFLASH_FLASH_BASE + (sectorIndex * BSPMCUFLASH_PAGE_SIZE);
    if (FLASH_ErasePage(lPageAddress) != FLASH_COMPLETE) {
        return DRV_STATUS_ERROR;
    }

    return DRV_STATUS_OK;
}

eDrvStatus bspMcuFlashPortProgram(uint32_t address, const uint8_t *buffer, uint32_t length)
{
    uint32_t lOffset;
    uint16_t lHalfWord;

    if ((buffer == NULL) || (length == 0U) || ((address & 0x1U) != 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    for (lOffset = 0U; lOffset < length; lOffset += 2U) {
        lHalfWord = buffer[lOffset];
        if ((lOffset + 1U) < length) {
            lHalfWord |= (uint16_t)((uint16_t)buffer[lOffset + 1U] << 8);
        } else {
            lHalfWord |= (uint16_t)((uint16_t)(*((const uint8_t *)(address + lOffset + 1U))) << 8);
        }

        if (FLASH_ProgramHalfWord(address + lOffset, lHalfWord) != FLASH_COMPLETE) {
            return DRV_STATUS_ERROR;
        }
    }

    return DRV_STATUS_OK;
}

eDrvStatus bspMcuFlashPortGetSectorInfo(uint32_t address,
                                        uint32_t *sectorIndex,
                                        uint32_t *sectorStart,
                                        uint32_t *sectorSize)
{
    uint32_t lIndex;

    if ((sectorIndex == NULL) || (sectorStart == NULL) || (sectorSize == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if ((address < BSPMCUFLASH_FLASH_BASE) ||
        (address >= (BSPMCUFLASH_FLASH_BASE + BSPMCUFLASH_FLASH_SIZE))) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lIndex = (address - BSPMCUFLASH_FLASH_BASE) / BSPMCUFLASH_PAGE_SIZE;
    *sectorIndex = lIndex;
    *sectorStart = BSPMCUFLASH_FLASH_BASE + (lIndex * BSPMCUFLASH_PAGE_SIZE);
    *sectorSize = BSPMCUFLASH_PAGE_SIZE;
    return DRV_STATUS_OK;
}

/**************************End of file********************************/
