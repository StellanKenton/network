/***********************************************************************************
* @file     : rtos_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "rtos_port.h"

#include <stddef.h>

#include "../rep_config.h"
#include "../system/system.h"

#if ((REP_RTOS_SYSTEM == REP_RTOS_FREERTOS) || (defined(REP_RTOS_CUBEMX_FREERTOS) && (REP_RTOS_SYSTEM == REP_RTOS_CUBEMX_FREERTOS)))
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#define REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE 1
#endif

#if defined(REP_RTOS_CUBEMX_FREERTOS) && (REP_RTOS_SYSTEM == REP_RTOS_CUBEMX_FREERTOS)
#include "cmsis_os.h"
#define REBUILDCPR_RTOS_HAS_CMSIS_V1 1
#endif

#if (REP_RTOS_SYSTEM == REP_RTOS_UCOSII)
#include "ucos_ii.h"
#define REBUILDCPR_RTOS_HAS_UCOSII 1
#endif

#ifndef REBUILDCPR_RTOS_HAS_CMSIS_V1
#define REBUILDCPR_RTOS_HAS_CMSIS_V1 0
#endif

#ifndef REBUILDCPR_RTOS_HAS_CMSIS_V2
#define REBUILDCPR_RTOS_HAS_CMSIS_V2 0
#endif

#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE != 1)
typedef uint32_t TickType_t;
typedef int32_t BaseType_t;
typedef void *TaskHandle_t;
#endif

#ifndef REBUILDCPR_RTOS_HAS_UCOSII
#define REBUILDCPR_RTOS_HAS_UCOSII 0
#endif

#ifndef REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE
#define REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE 0
#endif

#if defined(__get_PRIMASK) && defined(__set_PRIMASK)
static uint32_t gRtosPortCriticalState = 0U;
#endif
static uint32_t gRtosPortCriticalDepth = 0U;

#if (REP_RTOS_SYSTEM == REP_RTOS_UCOSII)
static uint32_t rtosPortDelayMsToTicks(uint32_t delayMs, uint32_t tickHz)
{
    if (tickHz == 0U) {
        return 0U;
    }

    if (delayMs == 0U) {
        return 1U;
    }

    return (uint32_t)((((uint64_t)delayMs * (uint64_t)tickHz) + 999ULL) / 1000ULL);
}
#endif

static void rtosPortEnterBareMetalCritical(void)
{
#if defined(__get_PRIMASK) && defined(__set_PRIMASK)
    uint32_t primask = __get_PRIMASK();

    __set_PRIMASK(1U);
    if (gRtosPortCriticalDepth == 0U) {
        gRtosPortCriticalState = primask;
    }
    gRtosPortCriticalDepth++;
#else
    gRtosPortCriticalDepth++;
#endif
}

static void rtosPortExitBareMetalCritical(void)
{
#if defined(__get_PRIMASK) && defined(__set_PRIMASK)
    if (gRtosPortCriticalDepth > 0U) {
        gRtosPortCriticalDepth--;
        if (gRtosPortCriticalDepth == 0U) {
            __set_PRIMASK(gRtosPortCriticalState);
        }
    }
#else
    if (gRtosPortCriticalDepth > 0U) {
        gRtosPortCriticalDepth--;
    }
#endif
}

#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
static TickType_t rtosPortTimeoutToTicks(uint32_t timeoutMs)
{
    if (!repRtosIsSchedulerRunning()) {
        return 0U;
    }

    if (timeoutMs == REP_RTOS_WAIT_FOREVER) {
        return portMAX_DELAY;
    }

    if (timeoutMs == 0U) {
        return 0U;
    }

    return pdMS_TO_TICKS(timeoutMs);
}
#endif

static eRepRtosSchedulerState rtosPortGetSchedulerStateImpl(void)
{
#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    switch (xTaskGetSchedulerState()) {
        case taskSCHEDULER_NOT_STARTED:
            return REP_RTOS_SCHEDULER_STOPPED;
        case taskSCHEDULER_RUNNING:
            return REP_RTOS_SCHEDULER_RUNNING;
        case taskSCHEDULER_SUSPENDED:
            return REP_RTOS_SCHEDULER_SUSPENDED;
        default:
            return REP_RTOS_SCHEDULER_UNKNOWN;
    }
#elif defined(REP_RTOS_CUBEMX_FREERTOS) && (REP_RTOS_SYSTEM == REP_RTOS_CUBEMX_FREERTOS) && (REBUILDCPR_RTOS_HAS_CMSIS_V2 == 1)
    switch (osKernelGetState()) {
        case osKernelInactive:
            return REP_RTOS_SCHEDULER_STOPPED;
        case osKernelReady:
            return REP_RTOS_SCHEDULER_READY;
        case osKernelRunning:
            return REP_RTOS_SCHEDULER_RUNNING;
        case osKernelLocked:
            return REP_RTOS_SCHEDULER_LOCKED;
        case osKernelSuspended:
            return REP_RTOS_SCHEDULER_SUSPENDED;
        default:
            return REP_RTOS_SCHEDULER_UNKNOWN;
    }
#elif defined(REP_RTOS_CUBEMX_FREERTOS) && (REP_RTOS_SYSTEM == REP_RTOS_CUBEMX_FREERTOS) && (REBUILDCPR_RTOS_HAS_CMSIS_V1 == 1)
    return osKernelRunning() ? REP_RTOS_SCHEDULER_RUNNING : REP_RTOS_SCHEDULER_STOPPED;
#elif (REP_RTOS_SYSTEM == REP_RTOS_UCOSII) && (REBUILDCPR_RTOS_HAS_UCOSII == 1)
    return (OSRunning == OS_TRUE) ? REP_RTOS_SCHEDULER_RUNNING : REP_RTOS_SCHEDULER_STOPPED;
#elif (REP_RTOS_SYSTEM == REP_RTOS_NONE)
    return REP_RTOS_SCHEDULER_STOPPED;
#else
    return REP_RTOS_SCHEDULER_UNKNOWN;
#endif
}

static eRepRtosStatus rtosPortDelayMsImpl(uint32_t delayMs)
{
#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(delayMs == 0U ? 1U : delayMs));
    } else {
        systemDelayMs(delayMs);
    }
    return REP_RTOS_STATUS_OK;
#elif defined(REP_RTOS_CUBEMX_FREERTOS) && (REP_RTOS_SYSTEM == REP_RTOS_CUBEMX_FREERTOS) && ((REBUILDCPR_RTOS_HAS_CMSIS_V1 == 1) || (REBUILDCPR_RTOS_HAS_CMSIS_V2 == 1))
    if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
        (void)osDelay(delayMs == 0U ? 1U : delayMs);
    } else {
        systemDelayMs(delayMs);
    }
    return REP_RTOS_STATUS_OK;
#elif (REP_RTOS_SYSTEM == REP_RTOS_UCOSII) && (REBUILDCPR_RTOS_HAS_UCOSII == 1)
    if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
        OSTimeDly((INT32U)rtosPortDelayMsToTicks(delayMs, OS_TICKS_PER_SEC));
    } else {
        systemDelayMs(delayMs);
    }
    return REP_RTOS_STATUS_OK;
#elif (REP_RTOS_SYSTEM == REP_RTOS_NONE)
    systemDelayMs(delayMs);
    return REP_RTOS_STATUS_OK;
#else
    (void)delayMs;
    return REP_RTOS_STATUS_UNSUPPORTED;
#endif
}

static uint32_t rtosPortGetTickMsImpl(void)
{
#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
        return (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
    }
    return systemGetTickMs();
#elif defined(REP_RTOS_CUBEMX_FREERTOS) && (REP_RTOS_SYSTEM == REP_RTOS_CUBEMX_FREERTOS) && (REBUILDCPR_RTOS_HAS_CMSIS_V2 == 1)
    if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
        return osKernelGetTickCount();
    }
    return systemGetTickMs();
#elif defined(REP_RTOS_CUBEMX_FREERTOS) && (REP_RTOS_SYSTEM == REP_RTOS_CUBEMX_FREERTOS) && (REBUILDCPR_RTOS_HAS_CMSIS_V1 == 1)
    if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
        return osKernelSysTick();
    }
    return systemGetTickMs();
#elif (REP_RTOS_SYSTEM == REP_RTOS_UCOSII) && (REBUILDCPR_RTOS_HAS_UCOSII == 1)
    if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
        return (uint32_t)(((uint64_t)OSTimeGet() * 1000ULL) / (uint64_t)OS_TICKS_PER_SEC);
    }
    return systemGetTickMs();
#else
    return systemGetTickMs();
#endif
}

static void rtosPortYieldImpl(void)
{
#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
        taskYIELD();
    }
#elif defined(REP_RTOS_CUBEMX_FREERTOS) && (REP_RTOS_SYSTEM == REP_RTOS_CUBEMX_FREERTOS) && ((REBUILDCPR_RTOS_HAS_CMSIS_V1 == 1) || (REBUILDCPR_RTOS_HAS_CMSIS_V2 == 1))
    if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
        (void)osThreadYield();
    }
#else
#endif
}

static void rtosPortEnterCriticalImpl(void)
{
#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
        taskENTER_CRITICAL();
        return;
    }
#endif

    rtosPortEnterBareMetalCritical();
}

static void rtosPortExitCriticalImpl(void)
{
#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
        taskEXIT_CRITICAL();
        return;
    }
#endif

    rtosPortExitBareMetalCritical();
}

static eRepRtosStatus rtosPortMutexCreateImpl(stRepRtosMutex *mutex)
{
    if (mutex == NULL) {
        return REP_RTOS_STATUS_INVALID_PARAM;
    }

    if (mutex->isCreated) {
        return REP_RTOS_STATUS_OK;
    }

#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    mutex->nativeHandle = xSemaphoreCreateMutex();
    if (mutex->nativeHandle == NULL) {
        return REP_RTOS_STATUS_ERROR;
    }
#else
    mutex->nativeHandle = NULL;
#endif

    mutex->isLocked = false;
    mutex->isCreated = true;
    return REP_RTOS_STATUS_OK;
}

static eRepRtosStatus rtosPortMutexTakeImpl(stRepRtosMutex *mutex, uint32_t timeoutMs)
{
    if ((mutex == NULL) || !mutex->isCreated) {
        return REP_RTOS_STATUS_INVALID_PARAM;
    }

#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    if (xSemaphoreTake((SemaphoreHandle_t)mutex->nativeHandle, rtosPortTimeoutToTicks(timeoutMs)) == pdTRUE) {
        return REP_RTOS_STATUS_OK;
    }

    return (timeoutMs == 0U) ? REP_RTOS_STATUS_BUSY : REP_RTOS_STATUS_TIMEOUT;
#else
    (void)timeoutMs;
    rtosPortEnterBareMetalCritical();
    if (mutex->isLocked) {
        rtosPortExitBareMetalCritical();
        return REP_RTOS_STATUS_BUSY;
    }

    mutex->isLocked = true;
    rtosPortExitBareMetalCritical();
    return REP_RTOS_STATUS_OK;
#endif
}

static eRepRtosStatus rtosPortMutexGiveImpl(stRepRtosMutex *mutex)
{
    if ((mutex == NULL) || !mutex->isCreated) {
        return REP_RTOS_STATUS_INVALID_PARAM;
    }

#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    return (xSemaphoreGive((SemaphoreHandle_t)mutex->nativeHandle) == pdTRUE) ? REP_RTOS_STATUS_OK : REP_RTOS_STATUS_ERROR;
#else
    rtosPortEnterBareMetalCritical();
    mutex->isLocked = false;
    rtosPortExitBareMetalCritical();
    return REP_RTOS_STATUS_OK;
#endif
}

static eRepRtosStatus rtosPortQueueCreateImpl(stRepRtosQueue *queue, uint32_t itemSize, uint32_t capacity)
{
    if ((queue == NULL) || (itemSize == 0U) || (capacity == 0U)) {
        return REP_RTOS_STATUS_INVALID_PARAM;
    }

    if (queue->isCreated) {
        return REP_RTOS_STATUS_OK;
    }

#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    queue->nativeHandle = xQueueCreate((UBaseType_t)capacity, (UBaseType_t)itemSize);
    if (queue->nativeHandle == NULL) {
        return REP_RTOS_STATUS_ERROR;
    }

    queue->itemSize = itemSize;
    queue->capacity = capacity;
    queue->isCreated = true;
    return REP_RTOS_STATUS_OK;
#else
    (void)itemSize;
    (void)capacity;
    return REP_RTOS_STATUS_UNSUPPORTED;
#endif
}

static eRepRtosStatus rtosPortQueueSendImpl(stRepRtosQueue *queue, const void *item, uint32_t timeoutMs)
{
    if ((queue == NULL) || (item == NULL) || !queue->isCreated) {
        return REP_RTOS_STATUS_INVALID_PARAM;
    }

#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    if (xQueueSend((QueueHandle_t)queue->nativeHandle, item, rtosPortTimeoutToTicks(timeoutMs)) == pdTRUE) {
        return REP_RTOS_STATUS_OK;
    }

    return (timeoutMs == 0U) ? REP_RTOS_STATUS_BUSY : REP_RTOS_STATUS_TIMEOUT;
#else
    (void)timeoutMs;
    return REP_RTOS_STATUS_UNSUPPORTED;
#endif
}

static eRepRtosStatus rtosPortQueueReceiveImpl(stRepRtosQueue *queue, void *item, uint32_t timeoutMs)
{
    if ((queue == NULL) || (item == NULL) || !queue->isCreated) {
        return REP_RTOS_STATUS_INVALID_PARAM;
    }

#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    if (xQueueReceive((QueueHandle_t)queue->nativeHandle, item, rtosPortTimeoutToTicks(timeoutMs)) == pdTRUE) {
        return REP_RTOS_STATUS_OK;
    }

    return (timeoutMs == 0U) ? REP_RTOS_STATUS_BUSY : REP_RTOS_STATUS_TIMEOUT;
#else
    (void)timeoutMs;
    return REP_RTOS_STATUS_UNSUPPORTED;
#endif
}

static eRepRtosStatus rtosPortQueueResetImpl(stRepRtosQueue *queue)
{
    if ((queue == NULL) || !queue->isCreated) {
        return REP_RTOS_STATUS_INVALID_PARAM;
    }

#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    return (xQueueReset((QueueHandle_t)queue->nativeHandle) == pdPASS) ? REP_RTOS_STATUS_OK : REP_RTOS_STATUS_ERROR;
#else
    return REP_RTOS_STATUS_UNSUPPORTED;
#endif
}

static eRepRtosStatus rtosPortTaskCreateImpl(const stRepRtosTaskConfig *config)
{
    BaseType_t createStatus;
    TaskHandle_t createdHandle = NULL;

    if ((config == NULL) || (config->entry == NULL) || (config->name == NULL)) {
        return REP_RTOS_STATUS_INVALID_PARAM;
    }

#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    if ((config->handle != NULL) && (*config->handle != NULL)) {
        return REP_RTOS_STATUS_OK;
    }

    createStatus = xTaskCreate((TaskFunction_t)config->entry,
                               config->name,
                               (configSTACK_DEPTH_TYPE)config->stackSize,
                               config->argument,
                               (UBaseType_t)config->priority,
                               &createdHandle);
    if (createStatus != pdPASS) {
        return REP_RTOS_STATUS_ERROR;
    }

    if (config->handle != NULL) {
        *config->handle = (repRtosTaskHandle)createdHandle;
    }
    return REP_RTOS_STATUS_OK;
#else
    (void)createStatus;
    (void)createdHandle;
    return REP_RTOS_STATUS_UNSUPPORTED;
#endif
}

static eRepRtosStatus rtosPortTaskDelayUntilMsImpl(uint32_t *lastWakeTimeMs, uint32_t periodMs)
{
    if (lastWakeTimeMs == NULL) {
        return REP_RTOS_STATUS_INVALID_PARAM;
    }

#if (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
        TickType_t lastWakeTicks = pdMS_TO_TICKS(*lastWakeTimeMs);

        if (*lastWakeTimeMs == 0U) {
            lastWakeTicks = xTaskGetTickCount();
        }

        vTaskDelayUntil(&lastWakeTicks, pdMS_TO_TICKS(periodMs == 0U ? 1U : periodMs));
        *lastWakeTimeMs = (uint32_t)lastWakeTicks * (uint32_t)portTICK_PERIOD_MS;
        return REP_RTOS_STATUS_OK;
    }
#endif

    if (rtosPortDelayMsImpl(periodMs) != REP_RTOS_STATUS_OK) {
        return REP_RTOS_STATUS_UNSUPPORTED;
    }

    *lastWakeTimeMs = rtosPortGetTickMsImpl();
    return REP_RTOS_STATUS_OK;
}

static const stRepRtosOps gRepRtosOps = {
    .getSchedulerState = rtosPortGetSchedulerStateImpl,
    .delayMs = rtosPortDelayMsImpl,
    .getTickMs = rtosPortGetTickMsImpl,
    .yield = rtosPortYieldImpl,
    .enterCritical = rtosPortEnterCriticalImpl,
    .exitCritical = rtosPortExitCriticalImpl,
    .mutexCreate = rtosPortMutexCreateImpl,
    .mutexTake = rtosPortMutexTakeImpl,
    .mutexGive = rtosPortMutexGiveImpl,
    .queueCreate = rtosPortQueueCreateImpl,
    .queueSend = rtosPortQueueSendImpl,
    .queueReceive = rtosPortQueueReceiveImpl,
    .queueReset = rtosPortQueueResetImpl,
    .taskCreate = rtosPortTaskCreateImpl,
    .taskDelayUntilMs = rtosPortTaskDelayUntilMsImpl,
};

const stRepRtosOps *rtosPortGetOps(void)
{
    return &gRepRtosOps;
}

const char *rtosPortGetName(void)
{
#if ((REP_RTOS_SYSTEM == REP_RTOS_FREERTOS) || (defined(REP_RTOS_CUBEMX_FREERTOS) && (REP_RTOS_SYSTEM == REP_RTOS_CUBEMX_FREERTOS))) && (REBUILDCPR_RTOS_HAS_FREERTOS_NATIVE == 1)
    return "freertos";
#elif defined(REP_RTOS_CUBEMX_FREERTOS) && (REP_RTOS_SYSTEM == REP_RTOS_CUBEMX_FREERTOS)
    return "cubemx-freertos";
#elif (REP_RTOS_SYSTEM == REP_RTOS_UCOSII)
    return "ucosii";
#elif (REP_RTOS_SYSTEM == REP_RTOS_UCOSIII)
    return "ucosiii";
#elif (REP_RTOS_SYSTEM == REP_RTOS_NONE)
    return "baremetal";
#else
    return "unknown";
#endif
}

uint32_t rtosPortGetSystem(void)
{
    return REP_RTOS_SYSTEM;
}

/**************************End of file********************************/
