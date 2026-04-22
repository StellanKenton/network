/***********************************************************************************
* @file     : rtos_port.c
* @brief    : uC/OS-II adapter for the reusable RTOS abstraction.
**********************************************************************************/
#include "rtos_port.h"

#include <stddef.h>

#include "../../USER/rep_config.h"
#include "delay.h"
#include "includes.h"

static OS_CPU_SR gRtosPortCriticalState = 0u;
static uint32_t gRtosPortCriticalDepth = 0u;

static uint32_t rtosPortDelayMsToTicks(uint32_t delayMs)
{
	if (OS_TICKS_PER_SEC == 0u) {
		return 0u;
	}

	if (delayMs == 0u) {
		return 1u;
	}

	return (uint32_t)((((uint64_t)delayMs * (uint64_t)OS_TICKS_PER_SEC) + 999ULL) / 1000ULL);
}

static void rtosPortEnterBareMetalCritical(void)
{
	OS_CPU_SR cpu_sr = OS_CPU_SR_Save();

	if (gRtosPortCriticalDepth == 0u) {
		gRtosPortCriticalState = cpu_sr;
	}
	gRtosPortCriticalDepth++;
}

static eRepRtosSchedulerState rtosPortGetSchedulerStateImpl(void)
{
	return (OSRunning == OS_TRUE) ? REP_RTOS_SCHEDULER_RUNNING : REP_RTOS_SCHEDULER_STOPPED;
}

static eRepRtosStatus rtosPortDelayMsImpl(uint32_t delayMs)
{
	if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
		(void)OSTimeDly((INT32U)rtosPortDelayMsToTicks(delayMs));
	} else {
		delay_ms((u16)delayMs);
	}

	return REP_RTOS_STATUS_OK;
}

static uint32_t rtosPortGetTickMsImpl(void)
{
	if (OS_TICKS_PER_SEC == 0u) {
		return 0u;
	}

	return (uint32_t)(((uint64_t)OSTimeGet() * 1000ULL) / (uint64_t)OS_TICKS_PER_SEC);
}

static void rtosPortYieldImpl(void)
{
	if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
		(void)OSTimeDly(1u);
	}
}

static void rtosPortEnterCriticalImpl(void)
{
	rtosPortEnterBareMetalCritical();
}

static void rtosPortExitCriticalImpl(void)
{
	if (gRtosPortCriticalDepth == 0u) {
		return;
	}

	gRtosPortCriticalDepth--;
	if (gRtosPortCriticalDepth == 0u) {
		OS_CPU_SR_Restore(gRtosPortCriticalState);
	}
}

static eRepRtosStatus rtosPortMutexCreateImpl(stRepRtosMutex *mutex)
{
	if (mutex == NULL) {
		return REP_RTOS_STATUS_INVALID_PARAM;
	}

	if (mutex->isCreated) {
		return REP_RTOS_STATUS_OK;
	}

	mutex->nativeHandle = (void *)OSSemCreate(1u);
	if (mutex->nativeHandle == NULL) {
		return REP_RTOS_STATUS_ERROR;
	}

	mutex->isLocked = false;
	mutex->isCreated = true;
	return REP_RTOS_STATUS_OK;
}

static eRepRtosStatus rtosPortMutexTakeImpl(stRepRtosMutex *mutex, uint32_t timeoutMs)
{
	OS_ERR err = OS_ERR_NONE;
	INT16U ticks = 0u;

	if ((mutex == NULL) || !mutex->isCreated || (mutex->nativeHandle == NULL)) {
		return REP_RTOS_STATUS_INVALID_PARAM;
	}

	if (timeoutMs == REP_RTOS_WAIT_FOREVER) {
		ticks = 0u;
	} else {
		ticks = (INT16U)rtosPortDelayMsToTicks(timeoutMs);
	}

	OSSemPend((OS_EVENT *)mutex->nativeHandle, ticks, &err);
	if (err == OS_ERR_NONE) {
		mutex->isLocked = true;
		return REP_RTOS_STATUS_OK;
	}

	if ((err == OS_ERR_TIMEOUT) || (err == OS_ERR_PEND_ABORT)) {
		return (timeoutMs == 0u) ? REP_RTOS_STATUS_BUSY : REP_RTOS_STATUS_TIMEOUT;
	}

	return REP_RTOS_STATUS_ERROR;
}

static eRepRtosStatus rtosPortMutexGiveImpl(stRepRtosMutex *mutex)
{
	if ((mutex == NULL) || !mutex->isCreated || (mutex->nativeHandle == NULL)) {
		return REP_RTOS_STATUS_INVALID_PARAM;
	}

	mutex->isLocked = false;
	return (OSSemPost((OS_EVENT *)mutex->nativeHandle) == OS_ERR_NONE) ? REP_RTOS_STATUS_OK : REP_RTOS_STATUS_ERROR;
}

static eRepRtosStatus rtosPortQueueCreateImpl(stRepRtosQueue *queue, uint32_t itemSize, uint32_t capacity)
{
	(void)queue;
	(void)itemSize;
	(void)capacity;
	return REP_RTOS_STATUS_UNSUPPORTED;
}

static eRepRtosStatus rtosPortQueueSendImpl(stRepRtosQueue *queue, const void *item, uint32_t timeoutMs)
{
	(void)queue;
	(void)item;
	(void)timeoutMs;
	return REP_RTOS_STATUS_UNSUPPORTED;
}

static eRepRtosStatus rtosPortQueueReceiveImpl(stRepRtosQueue *queue, void *item, uint32_t timeoutMs)
{
	(void)queue;
	(void)item;
	(void)timeoutMs;
	return REP_RTOS_STATUS_UNSUPPORTED;
}

static eRepRtosStatus rtosPortQueueResetImpl(stRepRtosQueue *queue)
{
	(void)queue;
	return REP_RTOS_STATUS_UNSUPPORTED;
}

static eRepRtosStatus rtosPortTaskCreateImpl(const stRepRtosTaskConfig *config)
{
	(void)config;
	return REP_RTOS_STATUS_UNSUPPORTED;
}

static eRepRtosStatus rtosPortTaskDelayUntilMsImpl(uint32_t *lastWakeTimeMs, uint32_t periodMs)
{
	if (lastWakeTimeMs == NULL) {
		return REP_RTOS_STATUS_INVALID_PARAM;
	}

	if (rtosPortDelayMsImpl(periodMs) != REP_RTOS_STATUS_OK) {
		return REP_RTOS_STATUS_ERROR;
	}

	*lastWakeTimeMs = rtosPortGetTickMsImpl();
	return REP_RTOS_STATUS_OK;
}

static const stRepRtosOps gRepRtosOps = {
	rtosPortGetSchedulerStateImpl,
	rtosPortDelayMsImpl,
	rtosPortGetTickMsImpl,
	rtosPortYieldImpl,
	rtosPortEnterCriticalImpl,
	rtosPortExitCriticalImpl,
	rtosPortMutexCreateImpl,
	rtosPortMutexTakeImpl,
	rtosPortMutexGiveImpl,
	rtosPortQueueCreateImpl,
	rtosPortQueueSendImpl,
	rtosPortQueueReceiveImpl,
	rtosPortQueueResetImpl,
	rtosPortTaskCreateImpl,
	rtosPortTaskDelayUntilMsImpl
};

const stRepRtosOps *rtosPortGetOps(void)
{
	return &gRepRtosOps;
}

const char *rtosPortGetName(void)
{
	return "ucosii";
}

uint32_t rtosPortGetSystem(void)
{
	return REP_RTOS_SYSTEM;
}

/**************************End of file********************************/
