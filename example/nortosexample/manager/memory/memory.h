/***********************************************************************************
* @file     : flash.h
* @brief    : Flash manager declarations.
* @details  : Provides scheduler-facing flash init and health-check hooks.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_FLASH_H
#define CPRSENSORBOOT_FLASH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stFlashManagerInfo {
	bool isReady;
	uint8_t manufacturerId;
	uint8_t memoryType;
	uint8_t capacityId;
} stFlashManagerInfo;

void flashManagerReset(void);
bool flashManagerInit(stFlashManagerInfo *info);
void flashManagerProcess(uint32_t nowTick, stFlashManagerInfo *info);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_FLASH_H
/**************************End of file********************************/
