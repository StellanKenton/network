/***********************************************************************************
* @file     : update_port.c
* @brief    : Default project binding for the reusable update service.
* @details  : Binds the generic update core to the current project internal
*             flash and GD25Q32 storage layout while keeping the core free from
*             board-specific dependencies.
* @author   : GitHub Copilot
* @date     : 2026-04-16
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "update_port.h"

#include <string.h>

#include "../bsp/bspmcuflash.h"
#include "../../rep/module/gd25qxxx/gd25qxxx.h"
#include "../../rep/service/rtos/rtos.h"

#define UPDATE_PORT_RUN_APP_START_ADDR             0x08020000UL
#define UPDATE_PORT_RUN_APP_SIZE                   0x00060000UL

#define UPDATE_PORT_BOOT_RECORD_START_ADDR         0x0801F000UL
#define UPDATE_PORT_BOOT_RECORD_SIZE               0x00001000UL

#define UPDATE_PORT_BACKUP_HEADER_START_ADDR       0x00300000UL
#define UPDATE_PORT_BACKUP_HEADER_SIZE             (2UL * GD25QXXX_SECTOR_SIZE)
#define UPDATE_PORT_BACKUP_APP_START_ADDR          (UPDATE_PORT_BACKUP_HEADER_START_ADDR + UPDATE_PORT_BACKUP_HEADER_SIZE)
#define UPDATE_PORT_BACKUP_APP_END_ADDR            0x00380000UL
#define UPDATE_PORT_BACKUP_APP_SIZE                (UPDATE_PORT_BACKUP_APP_END_ADDR - UPDATE_PORT_BACKUP_APP_START_ADDR)

#define UPDATE_PORT_STAGING_HEADER_START_ADDR      0x00380000UL
#define UPDATE_PORT_STAGING_HEADER_SIZE            (2UL * GD25QXXX_SECTOR_SIZE)
#define UPDATE_PORT_STAGING_APP_START_ADDR         (UPDATE_PORT_STAGING_HEADER_START_ADDR + UPDATE_PORT_STAGING_HEADER_SIZE)
#define UPDATE_PORT_STAGING_APP_TOTAL_SIZE         (4UL * 1024UL * 1024UL)
#define UPDATE_PORT_STAGING_APP_SIZE               (UPDATE_PORT_STAGING_APP_TOTAL_SIZE - UPDATE_PORT_STAGING_APP_START_ADDR)

static bool updatePortInitExtFlash1(void);
static bool updatePortReadExtFlash1(uint32_t address, uint8_t *buffer, uint32_t length);
static bool updatePortWriteExtFlash1(uint32_t address, const uint8_t *buffer, uint32_t length);
static bool updatePortEraseExtFlash1(uint32_t address, uint32_t length);
static bool updatePortValidateExtFlash1Range(uint32_t address, uint32_t length);

static const stUpdateStorageOps gUpdateStorageOps[E_UPDATE_STORAGE_MAX] = {
    [E_UPDATE_STORAGE_INTERNAL_FLASH] = {
        .init = bspMcuFlashInit,
        .read = bspMcuFlashRead,
        .write = bspMcuFlashWrite,
        .erase = bspMcuFlashErase,
        .isRangeValid = bspMcuFlashIsRangeValid,
    },
    [E_UPDATE_STORAGE_EXT_FLASH1] = {
        .init = updatePortInitExtFlash1,
        .read = updatePortReadExtFlash1,
        .write = updatePortWriteExtFlash1,
        .erase = updatePortEraseExtFlash1,
        .isRangeValid = updatePortValidateExtFlash1Range,
    },
};

static const stUpdateRegionCfg gUpdateRegionMap[E_UPDATE_REGION_MAX] = {
    [E_UPDATE_REGION_BOOT_RECORD] = {
        .storageId = E_UPDATE_STORAGE_INTERNAL_FLASH,
        .startAddress = UPDATE_PORT_BOOT_RECORD_START_ADDR,
        .size = UPDATE_PORT_BOOT_RECORD_SIZE,
        .eraseUnit = 2048U,
        .progUnit = 1U,
        .headerReserveSize = 0U,
        .isReadable = true,
        .isWritable = true,
        .isExecutable = false,
    },
    [E_UPDATE_REGION_RUN_APP] = {
        .storageId = E_UPDATE_STORAGE_INTERNAL_FLASH,
        .startAddress = UPDATE_PORT_RUN_APP_START_ADDR,
        .size = UPDATE_PORT_RUN_APP_SIZE,
        .eraseUnit = 2048U,
        .progUnit = 2U,
        .headerReserveSize = 0U,
        .isReadable = true,
        .isWritable = true,
        .isExecutable = true,
    },
    [E_UPDATE_REGION_STAGING_APP] = {
        .storageId = E_UPDATE_STORAGE_EXT_FLASH1,
        .startAddress = UPDATE_PORT_STAGING_APP_START_ADDR,
        .size = UPDATE_PORT_STAGING_APP_SIZE,
        .eraseUnit = GD25QXXX_SECTOR_SIZE,
        .progUnit = 1U,
        .headerReserveSize = 0U,
        .isReadable = true,
        .isWritable = true,
        .isExecutable = false,
    },
    [E_UPDATE_REGION_STAGING_APP_HEADER] = {
        .storageId = E_UPDATE_STORAGE_EXT_FLASH1,
        .startAddress = UPDATE_PORT_STAGING_HEADER_START_ADDR,
        .size = UPDATE_PORT_STAGING_HEADER_SIZE,
        .eraseUnit = GD25QXXX_SECTOR_SIZE,
        .progUnit = 1U,
        .headerReserveSize = 0U,
        .isReadable = true,
        .isWritable = true,
        .isExecutable = false,
    },
    [E_UPDATE_REGION_BACKUP_APP] = {
        .storageId = E_UPDATE_STORAGE_EXT_FLASH1,
        .startAddress = UPDATE_PORT_BACKUP_APP_START_ADDR,
        .size = UPDATE_PORT_BACKUP_APP_SIZE,
        .eraseUnit = GD25QXXX_SECTOR_SIZE,
        .progUnit = 1U,
        .headerReserveSize = 0U,
        .isReadable = true,
        .isWritable = true,
        .isExecutable = false,
    },
    [E_UPDATE_REGION_BACKUP_APP_HEADER] = {
        .storageId = E_UPDATE_STORAGE_EXT_FLASH1,
        .startAddress = UPDATE_PORT_BACKUP_HEADER_START_ADDR,
        .size = UPDATE_PORT_BACKUP_HEADER_SIZE,
        .eraseUnit = GD25QXXX_SECTOR_SIZE,
        .progUnit = 1U,
        .headerReserveSize = 0U,
        .isReadable = true,
        .isWritable = true,
        .isExecutable = false,
    },
};

static bool updatePortInitExtFlash1(void)
{
    return gd25qxxxInit(GD25Q32_MEM) == GD25QXXX_STATUS_OK;
}

static bool updatePortReadExtFlash1(uint32_t address, uint8_t *buffer, uint32_t length)
{
    if (!updatePortValidateExtFlash1Range(address, length)) {
        return false;
    }

    return gd25qxxxRead(GD25Q32_MEM, address, buffer, length) == GD25QXXX_STATUS_OK;
}

static bool updatePortWriteExtFlash1(uint32_t address, const uint8_t *buffer, uint32_t length)
{
    if (!updatePortValidateExtFlash1Range(address, length)) {
        return false;
    }

    return gd25qxxxWrite(GD25Q32_MEM, address, buffer, length) == GD25QXXX_STATUS_OK;
}

static bool updatePortEraseExtFlash1(uint32_t address, uint32_t length)
{
    uint32_t lEraseAddress;
    uint32_t lEraseEnd;
    uint32_t lRemaining;

    if (!updatePortValidateExtFlash1Range(address, length)) {
        return false;
    }

    lEraseAddress = address & ~(GD25QXXX_SECTOR_SIZE - 1UL);
    lEraseEnd = (address + length + GD25QXXX_SECTOR_SIZE - 1UL) & ~(GD25QXXX_SECTOR_SIZE - 1UL);

    while (lEraseAddress < lEraseEnd) {
        lRemaining = lEraseEnd - lEraseAddress;
        if (((lEraseAddress % GD25QXXX_BLOCK64K_SIZE) == 0U) && (lRemaining >= GD25QXXX_BLOCK64K_SIZE)) {
            if (gd25qxxxEraseBlock64k(GD25Q32_MEM, lEraseAddress) != GD25QXXX_STATUS_OK) {
                return false;
            }
            lEraseAddress += GD25QXXX_BLOCK64K_SIZE;
        } else {
            if (gd25qxxxEraseSector(GD25Q32_MEM, lEraseAddress) != GD25QXXX_STATUS_OK) {
                return false;
            }
            lEraseAddress += GD25QXXX_SECTOR_SIZE;
        }
    }

    return true;
}

static bool updatePortValidateExtFlash1Range(uint32_t address, uint32_t length)
{
    if ((length == 0U) || (address >= UPDATE_PORT_STAGING_APP_TOTAL_SIZE)) {
        return false;
    }

    return (address + length) <= UPDATE_PORT_STAGING_APP_TOTAL_SIZE;
}

bool updatePortLoadDefaultCfg(stUpdateCfg *cfg)
{
    uint32_t lIndex;

    if (cfg == NULL) {
        return false;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    for (lIndex = 0U; lIndex < E_UPDATE_REGION_MAX; lIndex++) {
        cfg->regions[lIndex] = gUpdateRegionMap[lIndex];
    }

    cfg->processChunkSize = UPDATE_PROCESS_CHUNK_SIZE;
    cfg->metaRecordMagic = UPDATE_META_RECORD_MAGIC;
    cfg->metaCommitMarker = UPDATE_META_COMMIT_MARKER;
    cfg->enableRollback = true;
    return true;
}

const stUpdateStorageOps *updatePortGetStorageOps(uint8_t storageId)
{
    if (storageId >= E_UPDATE_STORAGE_MAX) {
        return NULL;
    }

    if (gUpdateStorageOps[storageId].read == NULL) {
        return NULL;
    }

    return &gUpdateStorageOps[storageId];
}

bool updatePortGetRegionMap(uint8_t regionId, stUpdateRegionCfg *cfg)
{
    if ((cfg == NULL) || (regionId >= E_UPDATE_REGION_MAX)) {
        return false;
    }

    *cfg = gUpdateRegionMap[regionId];
    return gUpdateRegionMap[regionId].size > 0U;
}

__attribute__((weak)) uint32_t updatePortGetTickMs(void)
{
    return repRtosGetTickMs();
}

__attribute__((weak)) void updatePortFeedWatchdog(void)
{
    
}

__attribute__((weak)) bool updatePortJumpToRegion(uint8_t regionId)
{
    (void)regionId;
    return false;
}

/**************************End of file********************************/
