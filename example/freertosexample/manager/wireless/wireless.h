/************************************************************************************
* @file     : wireless.h
* @brief    : Project-side wireless manager.
* @details  : Wraps the FC41D module for BLE initialization, periodic processing
*             and status query in the current product.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_WIRELESS_H
#define REBUILDCPR_WIRELESS_H

#include <stdbool.h>

#include "../../../rep/module/fc41d/fc41d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIRELESS_LOG_TAG                     "wireless"
#define WIRELESS_FC41D_DEVICE                FC41D_DEV0
#define WIRELESS_BOOT_READY_TIMEOUT_MS       3000U

typedef enum eWirelessState {
    eWIRELESS_STATE_INIT = 0,
    eWIRELESS_STATE_NORMAL,
    eWIRELESS_STATE_ERROR,
} eWirelessState;

bool wirelessInit(void);
void wirelessProcess(void);
const eWirelessState *wirelessGetStatus(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
