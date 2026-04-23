/************************************************************************************
* @file     : systask.h
* @brief    : Bare-metal system task scheduler declarations.
* @details  : Provides the cooperative task dispatcher used by the bootloader
*             main loop.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_SYSTASK_H
#define CPRSENSORBOOT_SYSTASK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYSTEM_TASK_BASE_PERIOD_MS        1U

#define SYSTEM_TASK_DISPLAY_INTERVAL_MS   20U
#define SYSTEM_TASK_DISPLAY_INIT_RETRY_MS 200U

#define SYSTEM_TASK_MEMORY_INTERVAL_MS    50U
#define SYSTEM_TASK_MEMORY_INIT_RETRY_MS  200U

#define SYSTEM_TASK_COMM_INTERVAL_MS      5U
#define SYSTEM_TASK_COMM_INIT_RETRY_MS    100U

#define SYSTEM_TASK_WIRELESS_INTERVAL_MS  5U
#define SYSTEM_TASK_WIRELESS_INIT_RETRY_MS 200U

#define SYSTEM_TASK_UPDATE_INTERVAL_MS    10U
#define SYSTEM_TASK_UPDATE_INIT_RETRY_MS  100U

typedef bool (*systemTaskInitFunc)(void);
typedef void (*systemTaskProcessFunc)(uint32_t nowTick);

typedef struct stSystemTaskSnapshot {
    uint32_t uptimeMs;
    uint32_t heartbeat;
    uint16_t displayValue;
    bool isDisplayReady;
    bool isMemoryReady;
    uint8_t memoryManufacturerId;
    uint8_t memoryTypeId;
    uint8_t memoryCapacityId;
} stSystemTaskSnapshot;

typedef struct stSystemTaskRuntimeState {
    uint32_t startTick;
    stSystemTaskSnapshot snapshot;
} stSystemTaskRuntimeState;

typedef struct stSystemTaskConfig {
    const char *name;
    uint32_t intervalMs;
    uint32_t initRetryMs;
    systemTaskInitFunc init;
    systemTaskProcessFunc process;
} stSystemTaskConfig;

typedef struct stSystemTaskState {
    bool isReady;
    uint32_t lastRunTick;
    uint32_t lastInitAttemptTick;
} stSystemTaskState;

typedef enum eSystemTaskIndex {
    E_SYSTEM_TASK_DISPLAY = 0,
    E_SYSTEM_TASK_MEMORY,
    E_SYSTEM_TASK_COMM,
    E_SYSTEM_TASK_WIRELESS,
    E_SYSTEM_TASK_UPDATE,
    E_SYSTEM_TASK_COUNT
} eSystemTaskIndex;

bool systemTaskSchedulerInit(void);
void systemTaskSchedulerProcess(void);
void systemTaskGetSnapshot(stSystemTaskSnapshot *snapshot);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_SYSTASK_H
/**************************End of file********************************/
