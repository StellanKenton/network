/***********************************************************************************
* @file     : system_debug.c
* @brief    : System debug and console command implementation.
* @details  : This file hosts optional console bindings for system debug operations.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "system_debug.h"

#include <stdint.h>
#include <string.h>

#include "../../Core/Inc/main.h"
#include "../../rep/driver/drvadc/drvadc_debug.h"
#include "../../rep/driver/drvanlogiic/drvanlogiic_debug.h"
#include "../../rep/driver/drviic/drviic_debug.h"
#include "../../rep/service/console/console.h"
#include "../../rep/service/console/log.h"
#include "../rep_config.h"
#include "system.h"

#define SYSTEM_DEBUG_LOG_TAG "system_debug"

#if (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1) && (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"

#define SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS            16U
#define SYSTEM_DEBUG_TASK_USAGE_SAMPLE_PERIOD_MS     50U
#define SYSTEM_DEBUG_TASK_USAGE_SAMPLE_COUNT         20U
#define SYSTEM_DEBUG_TASK_USAGE_TASK_STACK_SIZE      (configMINIMAL_STACK_SIZE * 4U)
#define SYSTEM_DEBUG_TASK_USAGE_TASK_PRIORITY        (tskIDLE_PRIORITY + 1U)

typedef struct {
    TaskStatus_t previousTaskStats[SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS];
    TaskStatus_t currentTaskStats[SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS];
    UBaseType_t previousTaskCount;
    UBaseType_t currentTaskCount;
    uint32_t previousTotalRunTime;
    uint32_t currentTotalRunTime;
} stSystemDebugTaskUsageContext;

static const TaskStatus_t *systemDebugFindTaskStatusByHandle(const TaskStatus_t *taskStatusArray, UBaseType_t taskCount, TaskHandle_t taskHandle);
static bool systemDebugCaptureTaskUsageSnapshot(TaskStatus_t *taskStatusArray, UBaseType_t capacity, UBaseType_t *taskCount, uint32_t *totalRunTime);
static eConsoleCommandResult systemDebugReplyTaskUsageSample(uint32_t transport, uint32_t sampleIndex, const TaskStatus_t *currentTaskStats, UBaseType_t currentTaskCount, const TaskStatus_t *previousTaskStats, UBaseType_t previousTaskCount, uint32_t totalRunTimeDelta);
static void systemDebugTaskUsageSampler(void *parameter);
#endif

static eConsoleCommandResult systemDebugConsoleVersionHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult systemDebugConsoleStatusHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult systemDebugConsoleRebootHandler(uint32_t transport, int argc, char *argv[]);
static bool gSystemDebugBackgroundServicesReady = false;

#if (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1) && (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
static eConsoleCommandResult systemDebugConsoleTaskUsageHandler(uint32_t transport, int argc, char *argv[]);

static TaskHandle_t gSystemDebugTaskUsageHandle = NULL;
static stSystemDebugTaskUsageContext gSystemDebugTaskUsageContext;
#endif

static const stConsoleCommand gSystemVersionConsoleCommand = {
    .commandName = "ver",
    .helpText = "ver - show firmware and hardware version",
    .ownerTag = "system",
    .handler = systemDebugConsoleVersionHandler,
};

static const stConsoleCommand gSystemStatusConsoleCommand = {
    .commandName = "sys",
    .helpText = "sys - show system status",
    .ownerTag = "system",
    .handler = systemDebugConsoleStatusHandler,
};

static const stConsoleCommand gSystemRebootConsoleCommand = {
    .commandName = "reboot",
    .helpText = "reboot - software reset the MCU",
    .ownerTag = "system",
    .handler = systemDebugConsoleRebootHandler,
};

#if (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1) && (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
static const stConsoleCommand gSystemTaskUsageConsoleCommand = {
    .commandName = "top",
    .helpText = "top - sample task cpu usage every 50 ms for 1 s",
    .ownerTag = "system",
    .handler = systemDebugConsoleTaskUsageHandler,
};

static const TaskStatus_t *systemDebugFindTaskStatusByHandle(const TaskStatus_t *taskStatusArray, UBaseType_t taskCount, TaskHandle_t taskHandle)
{
    UBaseType_t lIndex;

    if ((taskStatusArray == NULL) || (taskHandle == NULL)) {
        return NULL;
    }

    for (lIndex = 0U; lIndex < taskCount; lIndex++) {
        if (taskStatusArray[lIndex].xHandle == taskHandle) {
            return &taskStatusArray[lIndex];
        }
    }

    return NULL;
}

static bool systemDebugCaptureTaskUsageSnapshot(TaskStatus_t *taskStatusArray, UBaseType_t capacity, UBaseType_t *taskCount, uint32_t *totalRunTime)
{
    UBaseType_t lTaskCount;

    if ((taskStatusArray == NULL) || (taskCount == NULL) || (totalRunTime == NULL) || (capacity == 0U)) {
        return false;
    }

    lTaskCount = uxTaskGetNumberOfTasks();
    if ((lTaskCount == 0U) || (lTaskCount > capacity)) {
        return false;
    }

    *totalRunTime = 0U;
    lTaskCount = uxTaskGetSystemState(taskStatusArray, capacity, totalRunTime);
    if ((lTaskCount == 0U) || (*totalRunTime == 0U)) {
        return false;
    }

    *taskCount = lTaskCount;
    return true;
}

static eConsoleCommandResult systemDebugReplyTaskUsageSample(uint32_t transport, uint32_t sampleIndex, const TaskStatus_t *currentTaskStats, UBaseType_t currentTaskCount, const TaskStatus_t *previousTaskStats, UBaseType_t previousTaskCount, uint32_t totalRunTimeDelta)
{
    UBaseType_t lIndex;

    if ((currentTaskStats == NULL) || (previousTaskStats == NULL) || (totalRunTimeDelta == 0U)) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "sample %lu/%lu window=%lums",
        (unsigned long)(sampleIndex + 1U),
        (unsigned long)SYSTEM_DEBUG_TASK_USAGE_SAMPLE_COUNT,
        (unsigned long)SYSTEM_DEBUG_TASK_USAGE_SAMPLE_PERIOD_MS) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    for (lIndex = 0U; lIndex < currentTaskCount; lIndex++) {
        const TaskStatus_t *lPreviousTaskStatus;
        const char *lTaskName;
        uint32_t lTaskRunTimeDelta = 0U;
        uint32_t lUsagePercentX10;

        lPreviousTaskStatus = systemDebugFindTaskStatusByHandle(previousTaskStats,
            previousTaskCount,
            currentTaskStats[lIndex].xHandle);
        if ((lPreviousTaskStatus != NULL) &&
            (currentTaskStats[lIndex].ulRunTimeCounter >= lPreviousTaskStatus->ulRunTimeCounter)) {
            lTaskRunTimeDelta = currentTaskStats[lIndex].ulRunTimeCounter - lPreviousTaskStatus->ulRunTimeCounter;
        }

        lUsagePercentX10 = (uint32_t)((((uint64_t)lTaskRunTimeDelta * 1000ULL) +
            ((uint64_t)totalRunTimeDelta / 2ULL)) /
            (uint64_t)totalRunTimeDelta);
        lTaskName = (currentTaskStats[lIndex].pcTaskName != NULL) ? currentTaskStats[lIndex].pcTaskName : "unknown";
        if (consoleReply(transport,
            "%s: %lu.%lu%%",
            lTaskName,
            (unsigned long)(lUsagePercentX10 / 10U),
            (unsigned long)(lUsagePercentX10 % 10U)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static void systemDebugTaskUsageSampler(void *parameter)
{
    stSystemDebugTaskUsageContext *lContext = &gSystemDebugTaskUsageContext;
    TickType_t lLastWakeTime;
    uint32_t lSampleIndex;
    uint32_t lTransport = (uint32_t)parameter;

    (void)memset(lContext, 0, sizeof(*lContext));

    if (!systemDebugCaptureTaskUsageSnapshot(lContext->previousTaskStats,
        SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS,
        &lContext->previousTaskCount,
        &lContext->previousTotalRunTime)) {
        (void)consoleReply(lTransport, "ERROR: task runtime stats unavailable");
        gSystemDebugTaskUsageHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    lLastWakeTime = xTaskGetTickCount();
    for (lSampleIndex = 0U; lSampleIndex < SYSTEM_DEBUG_TASK_USAGE_SAMPLE_COUNT; lSampleIndex++) {
        vTaskDelayUntil(&lLastWakeTime, pdMS_TO_TICKS(SYSTEM_DEBUG_TASK_USAGE_SAMPLE_PERIOD_MS));

        if (!systemDebugCaptureTaskUsageSnapshot(lContext->currentTaskStats,
            SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS,
            &lContext->currentTaskCount,
            &lContext->currentTotalRunTime)) {
            (void)consoleReply(lTransport, "ERROR: task runtime stats unavailable");
            break;
        }

        if (lContext->currentTotalRunTime <= lContext->previousTotalRunTime) {
            (void)consoleReply(lTransport, "ERROR: invalid runtime stats window");
            break;
        }

        if (systemDebugReplyTaskUsageSample(lTransport,
            lSampleIndex,
            lContext->currentTaskStats,
            lContext->currentTaskCount,
            lContext->previousTaskStats,
            lContext->previousTaskCount,
            lContext->currentTotalRunTime - lContext->previousTotalRunTime) != CONSOLE_COMMAND_RESULT_OK) {
            break;
        }

        (void)memcpy(lContext->previousTaskStats,
            lContext->currentTaskStats,
            (size_t)lContext->currentTaskCount * sizeof(TaskStatus_t));
        lContext->previousTaskCount = lContext->currentTaskCount;
        lContext->previousTotalRunTime = lContext->currentTotalRunTime;
    }

    (void)consoleReply(lTransport, "top done");
    gSystemDebugTaskUsageHandle = NULL;
    vTaskDelete(NULL);
}

static eConsoleCommandResult systemDebugConsoleTaskUsageHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (gSystemDebugTaskUsageHandle != NULL) {
        if (consoleReply(transport, "ERROR: top sampler busy") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (xTaskCreate(systemDebugTaskUsageSampler,
        "TaskCpuMon",
        SYSTEM_DEBUG_TASK_USAGE_TASK_STACK_SIZE,
        (void *)transport,
        SYSTEM_DEBUG_TASK_USAGE_TASK_PRIORITY,
        &gSystemDebugTaskUsageHandle) != pdPASS) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "top started: %lums x %lu",
        (unsigned long)SYSTEM_DEBUG_TASK_USAGE_SAMPLE_PERIOD_MS,
        (unsigned long)SYSTEM_DEBUG_TASK_USAGE_SAMPLE_COUNT) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}
#endif

/**
* @brief : Reply with firmware and hardware version strings.
* @param : transport - console reply transport.
* @param : argc - console argument count.
* @param : argv - console argument vector.
* @return: Console command execution result.
**/
static eConsoleCommandResult systemDebugConsoleVersionHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (consoleReply(transport,
        "Firmware: %s\nVersion: %s\nHardware: %s\nOK",
        systemGetFirmwareName(),
        systemGetFirmwareVersion(),
        systemGetHardwareVersion()) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

/**
* @brief : Reply with current system runtime status.
* @param : transport - console reply transport.
* @param : argc - console argument count.
* @param : argv - console argument vector.
* @return: Console command execution result.
**/
static eConsoleCommandResult systemDebugConsoleStatusHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (consoleReply(transport,
        "Mode: %s\nOK",
        systemGetModeString(systemGetMode())) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemDebugConsoleRebootHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (consoleReply(transport, "rebooting...") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    vTaskDelay(pdMS_TO_TICKS(10U));
#else
    HAL_Delay(10U);
#endif

    __DSB();
    NVIC_SystemReset();

    return CONSOLE_COMMAND_RESULT_OK;
}

bool systemDebugBackgroundServicesInit(void)
{
    if (gSystemDebugBackgroundServicesReady) {
        return true;
    }

    if (!logInit()) {
        return false;
    }

    if (!consoleInit()) {
        return false;
    }

    if (!systemDebugConsoleRegister()) {
        return false;
    }

    if (!drvAnlogIicDebugConsoleRegister()) {
        return false;
    }

    if (!drvAdcDebugConsoleRegister()) {
        return false;
    }

    if (!drvIicDebugConsoleRegister()) {
        return false;
    }

    gSystemDebugBackgroundServicesReady = true;
    LOG_I(SYSTEM_DEBUG_LOG_TAG, "background console ready");
    return true;
}

void systemDebugBackgroundServicesProcess(void)
{
    if (!gSystemDebugBackgroundServicesReady) {
        return;
    }

    consoleProcess();
}

bool systemDebugConsoleRegister(void)
{
#if (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1) && (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    if (!consoleRegisterCommand(&gSystemVersionConsoleCommand)) {
        return false;
    }

    if (!consoleRegisterCommand(&gSystemStatusConsoleCommand)) {
        return false;
    }

    if (!consoleRegisterCommand(&gSystemRebootConsoleCommand)) {
        return false;
    }

    if (!consoleRegisterCommand(&gSystemTaskUsageConsoleCommand)) {
        return false;
    }

    return true;
#elif (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1)
    if (!consoleRegisterCommand(&gSystemVersionConsoleCommand)) {
        return false;
    }

    if (!consoleRegisterCommand(&gSystemStatusConsoleCommand)) {
        return false;
    }

    if (!consoleRegisterCommand(&gSystemRebootConsoleCommand)) {
        return false;
    }

    return true;
#else
    return true;
#endif
}
/**************************End of file********************************/

