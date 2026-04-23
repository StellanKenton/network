/************************************************************************************
* @file     : sysmgr.h
* @brief    : System manager declarations.
* @details  : Owns bootloader system startup, mode switching, watchdog feeding,
*             and console integration.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_SYSMGR_H
#define CPRSENSORBOOT_SYSMGR_H

#include <stdbool.h>
#include <stdint.h>

#include "system.h"

#ifdef __cplusplus
extern "C" {
#endif
#define SYSTEM_MANAGER_LOG_TAG              "SysMgr"
#define SYSTEM_WDG_FEED_INTERVAL_MS         1000U
#define SYSTEM_BACKGROUND_TASK_INTERVAL_MS  5U
#define SYSTEM_FSM_INTERVAL_MS              1U
#define SYSTEM_RTT_ANNOUNCE_INTERVAL_MS     1000U
#define SYSTEM_RTT_ANNOUNCE_REPEAT_COUNT    5U

typedef struct stSystemManagerState {
    bool isInitialized;
    bool isConsoleReady;
    bool isWatchdogActive;
    uint32_t watchdogTick;
    uint32_t lastBackgroundTaskTick;
    uint32_t lastFsmTick;
    uint32_t lastConsoleAnnounceTick;
    uint8_t consoleAnnounceCount;
} stSystemManagerState;

bool systemManagerInit(void);
void systemManagerProcess(void);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_SYSMGR_H
/**************************End of file********************************/
