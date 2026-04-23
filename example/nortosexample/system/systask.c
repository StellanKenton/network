/***********************************************************************************
* @file     : systask.c
* @brief    : Bare-metal system task scheduler implementation.
* @details  : Dispatches display and flash tasks with independent periods on
*             top of a 1 ms cooperative base tick while maintaining a shared
*             runtime snapshot.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "systask.h"

#include <stddef.h>

#include "../manager/comm/comm_mgr.h"
#include "../manager/display/display.h"
#include "../manager/memory/memory.h"
#include "../manager/update/update_mgr.h"
#include "../manager/wireless/wirelss_mgr.h"
#include "system.h"

static bool gSystemTaskSchedulerInitialized = false;
static stSystemTaskRuntimeState gSystemTaskRuntimeState;
static stSystemTaskState gSystemTaskStates[E_SYSTEM_TASK_COUNT];

static bool systemTaskDisplayInit(void);
static bool systemTaskMemoryInit(void);
static bool systemTaskCommInit(void);
static bool systemTaskWirelessInit(void);
static bool systemTaskUpdateInit(void);
static void systemTaskDisplayProcess(uint32_t nowTick);
static void systemTaskMemoryProcess(uint32_t nowTick);
static void systemTaskCommProcess(uint32_t nowTick);
static void systemTaskWirelessProcess(uint32_t nowTick);
static void systemTaskUpdateProcess(uint32_t nowTick);
static void systemTaskApplyMemoryInfo(const stFlashManagerInfo *info);
static void systemTaskStatesReset(uint32_t nowTick);
static void systemTaskProcessEntry(eSystemTaskIndex index, uint32_t nowTick);

static void systemTaskSnapshotReset(void);

static const stSystemTaskConfig gSystemTaskConfigs[E_SYSTEM_TASK_COUNT] = {
    [E_SYSTEM_TASK_DISPLAY] = {
        .name = "display",
        .intervalMs = SYSTEM_TASK_DISPLAY_INTERVAL_MS,
        .initRetryMs = SYSTEM_TASK_DISPLAY_INIT_RETRY_MS,
        .init = systemTaskDisplayInit,
        .process = systemTaskDisplayProcess,
    },
    [E_SYSTEM_TASK_MEMORY] = {
        .name = "memory",
        .intervalMs = SYSTEM_TASK_MEMORY_INTERVAL_MS,
        .initRetryMs = SYSTEM_TASK_MEMORY_INIT_RETRY_MS,
        .init = systemTaskMemoryInit,
        .process = systemTaskMemoryProcess,
    },
    [E_SYSTEM_TASK_COMM] = {
        .name = "comm",
        .intervalMs = SYSTEM_TASK_COMM_INTERVAL_MS,
        .initRetryMs = SYSTEM_TASK_COMM_INIT_RETRY_MS,
        .init = systemTaskCommInit,
        .process = systemTaskCommProcess,
    },
    [E_SYSTEM_TASK_WIRELESS] = {
        .name = "wireless",
        .intervalMs = SYSTEM_TASK_WIRELESS_INTERVAL_MS,
        .initRetryMs = SYSTEM_TASK_WIRELESS_INIT_RETRY_MS,
        .init = systemTaskWirelessInit,
        .process = systemTaskWirelessProcess,
    },
    [E_SYSTEM_TASK_UPDATE] = {
        .name = "update",
        .intervalMs = SYSTEM_TASK_UPDATE_INTERVAL_MS,
        .initRetryMs = SYSTEM_TASK_UPDATE_INIT_RETRY_MS,
        .init = systemTaskUpdateInit,
        .process = systemTaskUpdateProcess,
    },
};

bool systemTaskSchedulerInit(void)
{
    uint32_t lNowTick;

    lNowTick = systemGetTickMs();
    systemTaskSnapshotReset();
    gSystemTaskRuntimeState.startTick = lNowTick;
    systemTaskStatesReset(lNowTick);
    gSystemTaskSchedulerInitialized = true;
    return true;
}

void systemTaskGetSnapshot(stSystemTaskSnapshot *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    *snapshot = gSystemTaskRuntimeState.snapshot;
}

void systemTaskSchedulerProcess(void)
{
    uint32_t lNowTick;

    if (!gSystemTaskSchedulerInitialized) {
        return;
    }

    lNowTick = systemGetTickMs();

    gSystemTaskRuntimeState.snapshot.uptimeMs = lNowTick - gSystemTaskRuntimeState.startTick;
    gSystemTaskRuntimeState.snapshot.heartbeat++;

    systemTaskProcessEntry(E_SYSTEM_TASK_MEMORY, lNowTick);
    systemTaskProcessEntry(E_SYSTEM_TASK_COMM, lNowTick);
    systemTaskProcessEntry(E_SYSTEM_TASK_WIRELESS, lNowTick);
    systemTaskProcessEntry(E_SYSTEM_TASK_UPDATE, lNowTick);
    systemTaskProcessEntry(E_SYSTEM_TASK_DISPLAY, lNowTick);
}

static bool systemTaskDisplayInit(void)
{
    displayManagerReset();
    return displayManagerInit();
}

static bool systemTaskMemoryInit(void)
{
    stFlashManagerInfo lInfo;
    bool lIsReady;

    flashManagerReset();
    lIsReady = flashManagerInit(&lInfo);
    systemTaskApplyMemoryInfo(&lInfo);
    return lIsReady;
}

static bool systemTaskCommInit(void)
{
    commMgrReset();
    return commMgrInit();
}

static bool systemTaskWirelessInit(void)
{
    wirelessMgrReset();
    return wirelessMgrInit();
}

static bool systemTaskUpdateInit(void)
{
    updateManagerReset();
    return updateManagerInit();
}

static void systemTaskDisplayProcess(uint32_t nowTick)
{
    stDisplayManagerInput lInput;
    uint16_t lDisplayValue;

    (void)nowTick;

    lInput.heartbeat = gSystemTaskRuntimeState.snapshot.heartbeat;
    lInput.displayValue = (uint16_t)((gSystemTaskRuntimeState.snapshot.heartbeat / 10U) % 1000U);
    lInput.isMemoryReady = gSystemTaskRuntimeState.snapshot.isMemoryReady;

    if (!displayManagerProcess(&lInput, &lDisplayValue)) {
        gSystemTaskRuntimeState.snapshot.isDisplayReady = false;
        gSystemTaskStates[E_SYSTEM_TASK_DISPLAY].isReady = false;
        return;
    }

    gSystemTaskRuntimeState.snapshot.displayValue = lDisplayValue;
    gSystemTaskRuntimeState.snapshot.isDisplayReady = true;
}

static void systemTaskMemoryProcess(uint32_t nowTick)
{
    stFlashManagerInfo lInfo;

    flashManagerProcess(nowTick, &lInfo);
    systemTaskApplyMemoryInfo(&lInfo);
    gSystemTaskStates[E_SYSTEM_TASK_MEMORY].isReady = lInfo.isReady;
}

static void systemTaskCommProcess(uint32_t nowTick)
{
    (void)nowTick;
    commMgrProcess();
}

static void systemTaskWirelessProcess(uint32_t nowTick)
{
    wirelessMgrProcess(nowTick);
}

static void systemTaskUpdateProcess(uint32_t nowTick)
{
    updateManagerProcess(nowTick);
}

static void systemTaskApplyMemoryInfo(const stFlashManagerInfo *info)
{
    if (info == NULL) {
        return;
    }

    gSystemTaskRuntimeState.snapshot.isMemoryReady = info->isReady;
    gSystemTaskRuntimeState.snapshot.memoryManufacturerId = info->manufacturerId;
    gSystemTaskRuntimeState.snapshot.memoryTypeId = info->memoryType;
    gSystemTaskRuntimeState.snapshot.memoryCapacityId = info->capacityId;
}

static void systemTaskStatesReset(uint32_t nowTick)
{
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < (uint32_t)E_SYSTEM_TASK_COUNT; lIndex++) {
        gSystemTaskStates[lIndex].isReady = false;
        gSystemTaskStates[lIndex].lastRunTick = nowTick;
        gSystemTaskStates[lIndex].lastInitAttemptTick = nowTick - gSystemTaskConfigs[lIndex].initRetryMs;
    }
}

static void systemTaskProcessEntry(eSystemTaskIndex index, uint32_t nowTick)
{
    stSystemTaskState *lState;
    const stSystemTaskConfig *lConfig;

    lState = &gSystemTaskStates[index];
    lConfig = &gSystemTaskConfigs[index];

    if (!lState->isReady) {
        if ((nowTick - lState->lastInitAttemptTick) < lConfig->initRetryMs) {
            return;
        }

        lState->lastInitAttemptTick = nowTick;
        lState->isReady = lConfig->init();
        if (!lState->isReady) {
            return;
        }

        lState->lastRunTick = nowTick;
        lConfig->process(nowTick);
        return;
    }

    if ((nowTick - lState->lastRunTick) < lConfig->intervalMs) {
        return;
    }

    lState->lastRunTick = nowTick;
    lConfig->process(nowTick);
}

static void systemTaskSnapshotReset(void)
{
    gSystemTaskRuntimeState.startTick = 0U;
    gSystemTaskRuntimeState.snapshot.uptimeMs = 0U;
    gSystemTaskRuntimeState.snapshot.heartbeat = 0U;
    gSystemTaskRuntimeState.snapshot.displayValue = 0U;
    gSystemTaskRuntimeState.snapshot.isDisplayReady = false;
    gSystemTaskRuntimeState.snapshot.isMemoryReady = false;
    gSystemTaskRuntimeState.snapshot.memoryManufacturerId = 0U;
    gSystemTaskRuntimeState.snapshot.memoryTypeId = 0U;
    gSystemTaskRuntimeState.snapshot.memoryCapacityId = 0U;
}
/**************************End of file********************************/

