/************************************************************************************
* @file     : wireless_debug.h
* @brief    : Wireless debug helpers.
* @details  : Exposes RTT console registration for wireless module switching.
* @author   : GitHub Copilot
* @date     : 2026-04-25
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_MANAGER_WIRELESS_WIRELESS_DEBUG_H
#define NETWORK_APP_MANAGER_WIRELESS_WIRELESS_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WIRELESS_DEBUG_CONSOLE_SUPPORT
#define WIRELESS_DEBUG_CONSOLE_SUPPORT    1
#endif

bool wirelessDebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
