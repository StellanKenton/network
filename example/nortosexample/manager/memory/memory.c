/***********************************************************************************
* @file     : flash.c
* @brief    : Flash manager implementation.
* @details  : Owns GD25Q32 probe and periodic health checks for the scheduler.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "memory.h"

#include <stddef.h>
#include <string.h>

#include "gd25qxxx.h"
#include "../../../rep/service/log/log.h"

#define FLASH_LOG_TAG               "SysFlash"
#define FLASH_HEALTH_CHECK_MS       500U

typedef struct stFlashManagerState {
	bool isReady;
	uint32_t lastHealthCheckTick;
	uint8_t manufacturerId;
	uint8_t memoryType;
	uint8_t capacityId;
} stFlashManagerState;

static stFlashManagerState gFlashManagerState;

static void flashManagerFillInfo(stFlashManagerInfo *info)
{
	if (info == NULL) {
		return;
	}

	info->isReady = gFlashManagerState.isReady;
	info->manufacturerId = gFlashManagerState.manufacturerId;
	info->memoryType = gFlashManagerState.memoryType;
	info->capacityId = gFlashManagerState.capacityId;
}

static bool flashManagerProbe(void)
{
	eGd25qxxxStatus lStatus;
    uint8_t lManufacturerId = 0U;
    uint8_t lMemoryType = 0U;
    uint8_t lCapacityId = 0U;

	lStatus = gd25qxxxInit(GD25Q32_MEM);
	if (lStatus != GD25QXXX_STATUS_OK) {
		if (gd25qxxxReadJedecId(GD25Q32_MEM, &lManufacturerId, &lMemoryType, &lCapacityId) == GD25QXXX_STATUS_OK) {
			LOG_E(FLASH_LOG_TAG,
			      "flash init failed, status=%d, jedec=%02X %02X %02X",
			      (int)lStatus,
			      (unsigned int)lManufacturerId,
			      (unsigned int)lMemoryType,
			      (unsigned int)lCapacityId);
		}
		else {
			LOG_E(FLASH_LOG_TAG, "flash init failed, status=%d", (int)lStatus);
		}
		gFlashManagerState.isReady = false;
		gFlashManagerState.manufacturerId = 0U;
		gFlashManagerState.memoryType = 0U;
		gFlashManagerState.capacityId = 0U;
		return false;
	}

	lStatus = gd25qxxxReadJedecId(GD25Q32_MEM,
								  &gFlashManagerState.manufacturerId,
								  &gFlashManagerState.memoryType,
								  &gFlashManagerState.capacityId);
	if (lStatus != GD25QXXX_STATUS_OK) {
		LOG_E(FLASH_LOG_TAG, "read JEDEC failed, status=%d", (int)lStatus);
		gFlashManagerState.isReady = false;
		gFlashManagerState.manufacturerId = 0U;
		gFlashManagerState.memoryType = 0U;
		gFlashManagerState.capacityId = 0U;
		return false;
	}

	gFlashManagerState.isReady = true;
	LOG_I(FLASH_LOG_TAG,
		  "flash ready, jedec=%02X %02X %02X",
		  gFlashManagerState.manufacturerId,
		  gFlashManagerState.memoryType,
		  gFlashManagerState.capacityId);
	return true;
}

void flashManagerReset(void)
{
	(void)memset(&gFlashManagerState, 0, sizeof(gFlashManagerState));
}

bool flashManagerInit(stFlashManagerInfo *info)
{
	flashManagerReset();
	(void)flashManagerProbe();
	flashManagerFillInfo(info);
	return gFlashManagerState.isReady;
}

void flashManagerProcess(uint32_t nowTick, stFlashManagerInfo *info)
{
	uint8_t lStatus1 = 0U;
	eGd25qxxxStatus lStatus;

	if (!gFlashManagerState.isReady) {
		(void)flashManagerProbe();
		gFlashManagerState.lastHealthCheckTick = nowTick;
		flashManagerFillInfo(info);
		return;
	}

	if ((nowTick - gFlashManagerState.lastHealthCheckTick) < FLASH_HEALTH_CHECK_MS) {
		flashManagerFillInfo(info);
		return;
	}

	gFlashManagerState.lastHealthCheckTick = nowTick;
	lStatus = gd25qxxxReadStatus1(GD25Q32_MEM, &lStatus1);
	if (lStatus != GD25QXXX_STATUS_OK) {
		gFlashManagerState.isReady = false;
		LOG_W(FLASH_LOG_TAG, "flash health check failed, status=%d", (int)lStatus);
	}

	flashManagerFillInfo(info);
}
/**************************End of file********************************/
