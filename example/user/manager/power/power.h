/************************************************************************************
* @file     : power.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_POWER_H
#define REBUILDCPR_POWER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ePowerState {
    ePOWER_STATE_UNINIT = 0,
    ePOWER_STATE_READY,
    ePOWER_STATE_ACTIVE,
    ePOWER_STATE_LOW_POWER,
    ePOWER_STATE_STOPPED,
    ePOWER_STATE_FAULT,
} ePowerState;

typedef struct stPowerStatus {
    ePowerState state;
    bool isLowPowerRequested;
} stPowerStatus;

bool powerInit(void);
bool powerStart(void);
void powerStop(void);
void powerProcess(void);
bool powerRequestLowPower(bool isEnabled);
const stPowerStatus *powerGetStatus(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
