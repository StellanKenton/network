#include "systask.h"

#include "includes.h"
#include "../../rep/service/rtos/rtos.h"

#include "../manager/cellular/cellular.h"
#include "../manager/ethernet/ethernet.h"
#include "../manager/iotmanager/iotmanager.h"
#include "../manager/wireless/wireless.h"
#include "sysmgr.h"



static repRtosStackType gIotManagerTaskStk[IOT_MANAGER_TASK_STK_SIZE];
static repRtosStackType gWirelessTaskStk[WIRELESS_TASK_STK_SIZE];
static repRtosStackType gCellularTaskStk[CELLULAR_TASK_STK_SIZE];
static repRtosStackType gEthernetTaskStk[ETHERNET_TASK_STK_SIZE];

static CPU_BOOLEAN gWorkerTasksCreated = DEF_FALSE;

static void systaskAssertRtosStatus(eRepRtosStatus status)
{
	if (status == REP_RTOS_STATUS_OK) {
		return;
	}

	for (;;) {
	}
}

static void systaskCreateTask(const char *name,
				  repRtosTaskEntry task,
				  void *argument,
				  uint32_t prio,
				  repRtosStackType *stackBase,
				  uint32_t stackSize)
{
	stRepRtosTaskConfig config;

	config.name = name;
	config.entry = task;
	config.argument = argument;
	config.stackBuffer = stackBase;
	config.stackSize = stackSize;
	config.priority = prio;
	config.handle = NULL;

	systaskAssertRtosStatus(repRtosTaskCreate(&config));
}

static void iotManagerTask(void *pdata)
{
	(void)pdata;
	for (;;) {
		iotManagerProcess();
		systaskAssertRtosStatus(repRtosDelayMs(IOT_MANAGER_TASK_INTERVAL_MS));
	}
}

static void wirelessTask(void *pdata)
{
	(void)pdata;
	for (;;) {
		wirelessProcess();
		systaskAssertRtosStatus(repRtosDelayMs(WIRELESS_TASK_INTERVAL_MS));
	}
}

static void cellularTask(void *pdata)
{
	(void)pdata;
	for (;;) {
		cellularProcess();
		systaskAssertRtosStatus(repRtosDelayMs(CELLULAR_TASK_INTERVAL_MS));
	}
}

static void ethernetTask(void *pdata)
{
	(void)pdata;
	for (;;) {
		ethernetProcess();
		systaskAssertRtosStatus(repRtosDelayMs(ETHERNET_TASK_INTERVAL_MS));
	}
}

static void systaskCreateWorkerTasks(void)
{
	if (gWorkerTasksCreated != DEF_FALSE) {
		return;
	}

	systaskCreateTask("IotManager",
				  iotManagerTask,
				  NULL,
				  IOT_MANAGER_TASK_PRIO,
				  &gIotManagerTaskStk[0],
				  IOT_MANAGER_TASK_STK_SIZE);
	systaskCreateTask("Wireless",
				  wirelessTask,
				  NULL,
				  WIRELESS_TASK_PRIO,
				  &gWirelessTaskStk[0],
				  WIRELESS_TASK_STK_SIZE);
	systaskCreateTask("Cellular",
				  cellularTask,
				  NULL,
				  CELLULAR_TASK_PRIO,
				  &gCellularTaskStk[0],
				  CELLULAR_TASK_STK_SIZE);
	systaskCreateTask("Ethernet",
				  ethernetTask,
				  NULL,
				  ETHERNET_TASK_PRIO,
				  &gEthernetTaskStk[0],
				  ETHERNET_TASK_STK_SIZE);

	gWorkerTasksCreated = DEF_TRUE;
}

void system_task(void *pdata)
{
	(void)pdata;
	systaskCreateWorkerTasks();
	for (;;) {
		systemManagerRun();
		systaskAssertRtosStatus(repRtosDelayMs(SYSTEM_TASK_INTERVAL_MS));
	}
}

