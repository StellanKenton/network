/************************************************************************************
* @file     : iotmanager_debug.h
* @brief    : Iot manager debug helpers.
* @details  : Exposes lightweight helpers for interface selection and status query.
* @author   : GitHub Copilot
* @date     : 2026-04-24
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_MANAGER_IOTMANAGER_IOTMANAGER_DEBUG_H
#define NETWORK_APP_MANAGER_IOTMANAGER_IOTMANAGER_DEBUG_H

#include <stdbool.h>

#include "iotmanager.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef IOT_MANAGER_DEBUG_CONSOLE_SUPPORT
#define IOT_MANAGER_DEBUG_CONSOLE_SUPPORT    1
#endif

bool iotManagerDebugSelectRoute(eIotManagerServiceId serviceId, eIotManagerLinkId linkId);
bool iotManagerDebugGetStateSnapshot(stIotManagerState *state);
const char *iotManagerDebugGetLinkName(eIotManagerLinkId linkId);
const char *iotManagerDebugGetServiceName(eIotManagerServiceId serviceId);
const char *iotManagerDebugGetLinkStateName(eIotManagerLinkState state);
const char *iotManagerDebugGetServiceStateName(eIotManagerServiceState state);
bool iotManagerDebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
