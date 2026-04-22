#include "systask.h"

#include "includes.h"

#include "../manager/cellular/cellular.h"
#include "../manager/ethernet/ethernet.h"
#include "../manager/iotmanager/iotmanager.h"
#include "../manager/wireless/wireless.h"
#include "sysmgr.h"

#define IOT_MANAGER_TASK_PRIO        9u
#define IOT_MANAGER_TASK_STK_SIZE    256u
#define IOT_MANAGER_TASK_INTERVAL_MS 20u

#define WIRELESS_TASK_PRIO           10u
#define WIRELESS_TASK_STK_SIZE       256u
#define WIRELESS_TASK_INTERVAL_MS    20u

#define CELLULAR_TASK_PRIO           11u
#define CELLULAR_TASK_STK_SIZE       256u
#define CELLULAR_TASK_INTERVAL_MS    20u

#define ETHERNET_TASK_PRIO           12u
#define ETHERNET_TASK_STK_SIZE       256u
#define ETHERNET_TASK_INTERVAL_MS    20u

static OS_TCB gIotManagerTaskTcb;
static CPU_STK gIotManagerTaskStk[IOT_MANAGER_TASK_STK_SIZE];
static OS_TCB gWirelessTaskTcb;
static CPU_STK gWirelessTaskStk[WIRELESS_TASK_STK_SIZE];
static OS_TCB gCellularTaskTcb;
static CPU_STK gCellularTaskStk[CELLULAR_TASK_STK_SIZE];
static OS_TCB gEthernetTaskTcb;
static CPU_STK gEthernetTaskStk[ETHERNET_TASK_STK_SIZE];

static CPU_BOOLEAN gWorkerTasksCreated = DEF_FALSE;

static void systaskCreateTask(OS_TCB *taskTcb,
				  CPU_CHAR *name,
				  OS_TASK_PTR task,
				  void *argument,
				  OS_PRIO prio,
				  CPU_STK *stackBase,
				  CPU_STK_SIZE stackSize)
{
	OS_ERR err;
	CPU_STK *stackTop;

	(void)taskTcb;
	stackTop = &stackBase[stackSize - 1u];

	err = OSTaskCreateExt(task,
					  argument,
					  stackTop,
					  prio,
					  prio,
					  stackBase,
					  stackSize,
					  NULL,
					  APP_TASK_OPT);
	#if OS_TASK_NAME_EN > 0u
	if (err == OS_ERR_NONE) {
		OSTaskNameSet(prio, (INT8U *)name, &err);
	}
	#endif
	APP_RTOS_ASSERT(err);
}

static void iotManagerTask(void *pdata)
{
	OS_ERR err;

	(void)pdata;
	for (;;) {
		iotManagerProcess();
		err = OSTimeDlyHMSM(0, 0, 0, IOT_MANAGER_TASK_INTERVAL_MS);
		APP_RTOS_ASSERT(err);
	}
}

static void wirelessTask(void *pdata)
{
	OS_ERR err;

	(void)pdata;
	for (;;) {
		wirelessProcess();
		err = OSTimeDlyHMSM(0, 0, 0, WIRELESS_TASK_INTERVAL_MS);
		APP_RTOS_ASSERT(err);
	}
}

static void cellularTask(void *pdata)
{
	OS_ERR err;

	(void)pdata;
	for (;;) {
		cellularProcess();
		err = OSTimeDlyHMSM(0, 0, 0, CELLULAR_TASK_INTERVAL_MS);
		APP_RTOS_ASSERT(err);
	}
}

static void ethernetTask(void *pdata)
{
	OS_ERR err;

	(void)pdata;
	for (;;) {
		ethernetProcess();
		err = OSTimeDlyHMSM(0, 0, 0, ETHERNET_TASK_INTERVAL_MS);
		APP_RTOS_ASSERT(err);
	}
}

static void systaskCreateWorkerTasks(void)
{
	if (gWorkerTasksCreated != DEF_FALSE) {
		return;
	}

	systaskCreateTask(&gIotManagerTaskTcb,
				  (CPU_CHAR *)"IotManager",
				  iotManagerTask,
				  NULL,
				  IOT_MANAGER_TASK_PRIO,
				  &gIotManagerTaskStk[0],
				  IOT_MANAGER_TASK_STK_SIZE);
	systaskCreateTask(&gWirelessTaskTcb,
				  (CPU_CHAR *)"Wireless",
				  wirelessTask,
				  NULL,
				  WIRELESS_TASK_PRIO,
				  &gWirelessTaskStk[0],
				  WIRELESS_TASK_STK_SIZE);
	systaskCreateTask(&gCellularTaskTcb,
				  (CPU_CHAR *)"Cellular",
				  cellularTask,
				  NULL,
				  CELLULAR_TASK_PRIO,
				  &gCellularTaskStk[0],
				  CELLULAR_TASK_STK_SIZE);
	systaskCreateTask(&gEthernetTaskTcb,
				  (CPU_CHAR *)"Ethernet",
				  ethernetTask,
				  NULL,
				  ETHERNET_TASK_PRIO,
				  &gEthernetTaskStk[0],
				  ETHERNET_TASK_STK_SIZE);

	gWorkerTasksCreated = DEF_TRUE;
}

void system_task(void *pdata)
{
	OS_ERR err;

	(void)pdata;
	systaskCreateWorkerTasks();
	for (;;) {
		systemManagerRun();
		err = OSTimeDlyHMSM(0, 0, 0, 10);
		APP_RTOS_ASSERT(err);
	}
}

