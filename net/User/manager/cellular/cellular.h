/************************************************************************************
* @file     : cellular.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_MANAGER_CELLULAR_CELLULAR_H
#define NETWORK_APP_MANAGER_CELLULAR_CELLULAR_H

#include <stdbool.h>
#include <stdint.h>

#include "../../../rep/module/ec800m/ec800m.h"

typedef struct stCellularDebugStatus {
	bool initialized;
	bool started;
	bool infoReadPending;
	bool infoReadActive;
	eEc800mStatus initStatus;
	eEc800mStatus lastProcessStatus;
	eEc800mRunState runState;
	bool moduleReady;
	bool busy;
	bool atReady;
	bool simChecked;
	bool signalChecked;
	int16_t signalStrength;
	uint32_t lastOkTick;
	uint32_t lastFailTick;
} stCellularDebugStatus;

#ifdef __cplusplus
extern "C" {
#endif

bool cellularSendData(const uint8_t *buffer, uint16_t length);
bool cellularRequestInfoRead(void);
bool cellularGetDebugStatus(stCellularDebugStatus *status);
void cellularProcess(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
