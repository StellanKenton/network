/************************************************************************************
* @file     : protcolmgr.h
* @brief    : CPR sensor protocol manager.
* @details  : Handles protocol frame parsing, protocol state, and reply generation.
* @author   : GitHub Copilot
* @date     : 2026-04-25
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_MANAGER_IOTMANAGER_PROTCOLMGR_H
#define NETWORK_APP_MANAGER_IOTMANAGER_PROTCOLMGR_H

#include "iotmanager.h"

#ifdef __cplusplus
extern "C" {
#endif

bool protcolMgrPushReceivedData(eIotManagerLinkId linkId, const uint8_t *buffer, uint16_t length);
bool protcolMgrTryInitCipherKey(void);
void protcolMgrProcess(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
