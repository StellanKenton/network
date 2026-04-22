/************************************************************************************
* @file     : update.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_UPDATE_H
#define REBUILDCPR_UPDATE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eUpdateState {
    eUPDATE_STATE_UNINIT = 0,
    eUPDATE_STATE_IDLE,
    eUPDATE_STATE_PENDING,
    eUPDATE_STATE_ACTIVE,
    eUPDATE_STATE_STOPPED,
} eUpdateState;

typedef struct stUpdateStatus {
    eUpdateState state;
    bool isUpdateRequested;
} stUpdateStatus;

bool updateInit(void);
void updateProcess(void);
const stUpdateStatus *updateGetStatus(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
