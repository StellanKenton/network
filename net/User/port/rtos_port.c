/***********************************************************************************
* @file     : rtos_port.c
* @brief    : uC/OS-III adapter for the reusable RTOS abstraction.
**********************************************************************************/
#include "rtos_port.h"

#include <stddef.h>

#include "../../USER/rep_config.h"
#include "delay.h"
#include "includes.h"

#define RTOS_PORT_MUTEX_POOL_SIZE  8u
#define RTOS_PORT_TASK_POOL_SIZE   8u

typedef struct stRtosPortMutexSlot {
	stRepRtosMutex *owner;
	OS_SEM sem;
	CPU_BOOLEAN isAllocated;
} stRtosPortMutexSlot;

typedef struct stRtosPortTaskSlot {
	OS_TCB tcb;
	CPU_BOOLEAN isAllocated;
} stRtosPortTaskSlot;

static CPU_SR gRtosPortCriticalState = 0u;
static uint32_t gRtosPortCriticalDepth = 0u;
static stRtosPortMutexSlot gRtosPortMutexPool[RTOS_PORT_MUTEX_POOL_SIZE];
static stRtosPortTaskSlot gRtosPortTaskPool[RTOS_PORT_TASK_POOL_SIZE];

static stRtosPortMutexSlot *rtosPortFindMutexSlot(const stRepRtosMutex *mutex);
static stRtosPortMutexSlot *rtosPortAllocateMutexSlot(stRepRtosMutex *mutex);
static OS_TCB *rtosPortAllocateTaskTcb(void);

static uint32_t rtosPortDelayMsToTicks(uint32_t delayMs)
{
	if (OSCfg_TickRate_Hz == 0u) {
		return 0u;
	}

	if (delayMs == 0u) {
		return 1u;
	}

	return (uint32_t)((((uint64_t)delayMs * (uint64_t)OSCfg_TickRate_Hz) + 999ULL) / 1000ULL);
}

static stRtosPortMutexSlot *rtosPortFindMutexSlot(const stRepRtosMutex *mutex)
{
	uint32_t index;

	if (mutex == NULL) {
		return NULL;
	}

	for (index = 0u; index < RTOS_PORT_MUTEX_POOL_SIZE; index++) {
		if ((gRtosPortMutexPool[index].isAllocated != 0u) &&
			(gRtosPortMutexPool[index].owner == mutex)) {
			return &gRtosPortMutexPool[index];
		}
	}

	return NULL;
}

static stRtosPortMutexSlot *rtosPortAllocateMutexSlot(stRepRtosMutex *mutex)
{
	uint32_t index;
	stRtosPortMutexSlot *slot;

	slot = rtosPortFindMutexSlot(mutex);
	if (slot != NULL) {
		return slot;
	}

	for (index = 0u; index < RTOS_PORT_MUTEX_POOL_SIZE; index++) {
		if (gRtosPortMutexPool[index].isAllocated == 0u) {
			gRtosPortMutexPool[index].owner = mutex;
			gRtosPortMutexPool[index].isAllocated = DEF_TRUE;
			return &gRtosPortMutexPool[index];
		}
	}

	return NULL;
}

static OS_TCB *rtosPortAllocateTaskTcb(void)
{
	uint32_t index;

	for (index = 0u; index < RTOS_PORT_TASK_POOL_SIZE; index++) {
		if (gRtosPortTaskPool[index].isAllocated == 0u) {
			gRtosPortTaskPool[index].isAllocated = DEF_TRUE;
			return &gRtosPortTaskPool[index].tcb;
		}
	}

	return NULL;
}

static void rtosPortEnterBareMetalCritical(void)
{
	CPU_SR cpu_sr = CPU_SR_Save();

	if (gRtosPortCriticalDepth == 0u) {
		gRtosPortCriticalState = cpu_sr;
	}
	gRtosPortCriticalDepth++;
}

static eRepRtosSchedulerState rtosPortGetSchedulerStateImpl(void)
{
	return (OSRunning == OS_STATE_OS_RUNNING) ? REP_RTOS_SCHEDULER_RUNNING : REP_RTOS_SCHEDULER_STOPPED;
}

static eRepRtosStatus rtosPortDelayMsImpl(uint32_t delayMs)
{
	OS_ERR err = OS_ERR_NONE;

	if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
		OSTimeDly((OS_TICK)rtosPortDelayMsToTicks(delayMs),
			 OS_OPT_TIME_DLY,
			 &err);
		if (err != OS_ERR_NONE) {
			return REP_RTOS_STATUS_ERROR;
		}
	} else {
		delay_ms((u16)delayMs);
	}

	return REP_RTOS_STATUS_OK;
}

static uint32_t rtosPortGetTickMsImpl(void)
{
	OS_ERR err = OS_ERR_NONE;
	OS_TICK ticks;

	if (OSCfg_TickRate_Hz == 0u) {
		return 0u;
	}

	ticks = OSTimeGet(&err);
	if (err != OS_ERR_NONE) {
		return 0u;
	}

	return (uint32_t)(((uint64_t)ticks * 1000ULL) / (uint64_t)OSCfg_TickRate_Hz);
}

static void rtosPortYieldImpl(void)
{
	OS_ERR err = OS_ERR_NONE;

	if (rtosPortGetSchedulerStateImpl() == REP_RTOS_SCHEDULER_RUNNING) {
		OSTimeDly(1u, OS_OPT_TIME_DLY, &err);
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
		CPU_SR_Restore(gRtosPortCriticalState);
	}
}

static eRepRtosStatus rtosPortMutexCreateImpl(stRepRtosMutex *mutex)
{
	OS_ERR err = OS_ERR_NONE;
	stRtosPortMutexSlot *slot;

	if (mutex == NULL) {
		return REP_RTOS_STATUS_INVALID_PARAM;
	}

	if (mutex->isCreated) {
		return REP_RTOS_STATUS_OK;
	}

	slot = rtosPortAllocateMutexSlot(mutex);
	if (slot == NULL) {
		return REP_RTOS_STATUS_ERROR;
	}

	OSSemCreate(&slot->sem, (CPU_CHAR *)"repMutex", 1u, &err);
	if (err != OS_ERR_NONE) {
		slot->owner = NULL;
		slot->isAllocated = DEF_FALSE;
		return REP_RTOS_STATUS_ERROR;
	}

	mutex->nativeHandle = (void *)&slot->sem;

	mutex->isLocked = false;
	mutex->isCreated = true;
	return REP_RTOS_STATUS_OK;
}

static eRepRtosStatus rtosPortMutexTakeImpl(stRepRtosMutex *mutex, uint32_t timeoutMs)
{
	OS_ERR err = OS_ERR_NONE;
	OS_TICK ticks = 0u;
	OS_OPT opt = OS_OPT_PEND_BLOCKING;

	if ((mutex == NULL) || !mutex->isCreated || (mutex->nativeHandle == NULL)) {
		return REP_RTOS_STATUS_INVALID_PARAM;
	}

	if (timeoutMs == REP_RTOS_WAIT_FOREVER) {
		ticks = 0u;
	} else if (timeoutMs == 0u) {
		ticks = 0u;
		opt = OS_OPT_PEND_NON_BLOCKING;
	} else {
		ticks = (OS_TICK)rtosPortDelayMsToTicks(timeoutMs);
	}

	(void)OSSemPend((OS_SEM *)mutex->nativeHandle, ticks, opt, NULL, &err);
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
	OS_ERR err = OS_ERR_NONE;

	if ((mutex == NULL) || !mutex->isCreated || (mutex->nativeHandle == NULL)) {
		return REP_RTOS_STATUS_INVALID_PARAM;
	}

	mutex->isLocked = false;
	(void)OSSemPost((OS_SEM *)mutex->nativeHandle, OS_OPT_POST_1, &err);
	return (err == OS_ERR_NONE) ? REP_RTOS_STATUS_OK : REP_RTOS_STATUS_ERROR;
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
	CPU_STK *stackBase;
	OS_TCB *taskTcb;
	OS_ERR err = OS_ERR_NONE;
	OS_OPT taskOpt = OS_OPT_TASK_NONE;

	if ((config == NULL) || (config->entry == NULL) || (config->stackBuffer == NULL) ||
		(config->stackSize == 0u) || (config->priority > 255u)) {
		return REP_RTOS_STATUS_INVALID_PARAM;
	}

	stackBase = (CPU_STK *)config->stackBuffer;
	taskTcb = rtosPortAllocateTaskTcb();
	if (taskTcb == NULL) {
		return REP_RTOS_STATUS_ERROR;
	}
	#ifdef APP_TASK_OPT
	taskOpt = APP_TASK_OPT;
	#endif
	OSTaskCreate(taskTcb,
		     (CPU_CHAR *)config->name,
		     (OS_TASK_PTR)config->entry,
		     config->argument,
		     (OS_PRIO)config->priority,
		     stackBase,
		     (CPU_STK_SIZE)(config->stackSize / 10u),
		     (CPU_STK_SIZE)config->stackSize,
		     (OS_MSG_QTY)0u,
		     (OS_TICK)0u,
		     (void *)0,
		     taskOpt,
		     &err);
	if (err != OS_ERR_NONE) {
		return REP_RTOS_STATUS_ERROR;
	}

	if (config->handle != NULL) {
		*config->handle = (repRtosTaskHandle)taskTcb;
	}

	return REP_RTOS_STATUS_OK;
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

static eRepRtosStatus rtosPortStatsInitImpl(void)
{
	#if OS_CFG_STAT_TASK_EN > 0u
	OS_ERR err = OS_ERR_NONE;

	OSStatTaskCPUUsageInit(&err);
	if (err != OS_ERR_NONE) {
		return REP_RTOS_STATUS_ERROR;
	}
	#endif

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
	rtosPortTaskDelayUntilMsImpl,
	rtosPortStatsInitImpl
};

const stRepRtosOps *rtosPortGetOps(void)
{
	return &gRepRtosOps;
}

const char *rtosPortGetName(void)
{
	return "ucosiii";
}

uint32_t rtosPortGetSystem(void)
{
	return REP_RTOS_SYSTEM;
}

/**************************End of file********************************/
