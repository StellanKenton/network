/************************************************************************************
* @file     : update_port.h
* @brief    : Update service platform binding contract.
* @details  : Declares logical storage bindings, region maps, and optional
*             platform hooks used by the reusable update core.
* @author   : GitHub Copilot
* @date     : 2026-04-16
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REP_SERVICE_UPDATE_PORT_H
#define REP_SERVICE_UPDATE_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "../../rep/service/update/update.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eUpdateStorageId {
    E_UPDATE_STORAGE_INTERNAL_FLASH = 0,
    E_UPDATE_STORAGE_EXT_FLASH1,
    E_UPDATE_STORAGE_EXT_FLASH2,
    E_UPDATE_STORAGE_MAX,
} eUpdateStorageId;

typedef bool (*pfUpdateStorageInit)(void);
typedef bool (*pfUpdateStorageRead)(uint32_t address, uint8_t *buffer, uint32_t length);
typedef bool (*pfUpdateStorageWrite)(uint32_t address, const uint8_t *buffer, uint32_t length);
typedef bool (*pfUpdateStorageErase)(uint32_t address, uint32_t length);
typedef bool (*pfUpdateStorageIsRangeValid)(uint32_t address, uint32_t length);

typedef struct stUpdateStorageOps {
    pfUpdateStorageInit init;
    pfUpdateStorageRead read;
    pfUpdateStorageWrite write;
    pfUpdateStorageErase erase;
    pfUpdateStorageIsRangeValid isRangeValid;
} stUpdateStorageOps;

typedef struct stUpdateRegionCfg {
    uint8_t storageId;
    uint32_t startAddress;
    uint32_t size;
    uint32_t eraseUnit;
    uint32_t progUnit;
    uint32_t headerReserveSize;
    bool isReadable;
    bool isWritable;
    bool isExecutable;
} stUpdateRegionCfg;

typedef struct stUpdateCfg {
    stUpdateRegionCfg regions[E_UPDATE_REGION_MAX];
    uint32_t processChunkSize;
    uint32_t metaRecordMagic;
    uint32_t metaCommitMarker;
    bool enableRollback;
} stUpdateCfg;

bool updatePortLoadDefaultCfg(stUpdateCfg *cfg);
const stUpdateStorageOps *updatePortGetStorageOps(uint8_t storageId);
bool updatePortGetRegionMap(uint8_t regionId, stUpdateRegionCfg *cfg);
uint32_t updatePortGetTickMs(void);
void updatePortFeedWatchdog(void);
bool updatePortJumpToRegion(uint8_t regionId);

#ifdef __cplusplus
}
#endif

#endif  // REP_SERVICE_UPDATE_PORT_H
/**************************End of file********************************/
