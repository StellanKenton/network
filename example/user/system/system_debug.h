/************************************************************************************
* @file     : system_debug.h
* @brief    : System debug helpers.
* @details  : Exposes optional console registration for system debug commands.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef SYSTEM_DEBUG_H
#define SYSTEM_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SYSTEM_DEBUG_CONSOLE_SUPPORT
#define SYSTEM_DEBUG_CONSOLE_SUPPORT    1
#endif

bool systemDebugBackgroundServicesInit(void);
void systemDebugBackgroundServicesProcess(void);
bool systemDebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif  // SYSTEM_DEBUG_H
/**************************End of file********************************/
