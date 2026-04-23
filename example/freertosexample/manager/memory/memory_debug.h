/************************************************************************************
* @file     : memory_debug.h
* @brief    : Memory debug console helpers.
* @details  : Exposes console registration for vfs-backed memory debug
*             commands such as mkdir, ls, cd, and pwd.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_MEMORY_DEBUG_H
#define REBUILDCPR_MEMORY_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool memoryDebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_MEMORY_DEBUG_H
/**************************End of file********************************/
