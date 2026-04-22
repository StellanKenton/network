/************************************************************************************
* @file     : fc41dport.h
* @brief    : Project-side FC41D binding helpers.
* @details  : Provides FC41D platform hook declarations and startup helpers used
*             by the project wireless manager.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_FC41DPORT_H
#define REBUILDCPR_FC41DPORT_H

#include <stdint.h>

#include "../../../rep/module/fc41d/fc41d_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eFc41dPortBootState {
    FC41DPORT_BOOT_WAITING = 0,
    FC41DPORT_BOOT_READY,
    FC41DPORT_BOOT_TIMEOUT,
} eFc41dPortBootState;

void fc41dPortResetBootWaitState(void);
eFc41dPortBootState fc41dPortPollBootReady(eFc41dMapType device, uint32_t timeoutMs);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
