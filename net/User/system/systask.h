#ifndef NETWORK_APP_SYSTEM_SYSTASK_H
#define NETWORK_APP_SYSTEM_SYSTASK_H

#define SYSTEM_TASK_PRIO		     8
#define SYSTEM_TASK_STK_SIZE	     1024
#define SYSTEM_TASK_INTERVAL_MS		 10

#define IOT_MANAGER_TASK_PRIO        9u
#define IOT_MANAGER_TASK_STK_SIZE    1024
#define IOT_MANAGER_TASK_INTERVAL_MS 10u

#define WIRELESS_TASK_PRIO           10u
#define WIRELESS_TASK_STK_SIZE       1024
#define WIRELESS_TASK_INTERVAL_MS    10u

#define CELLULAR_TASK_PRIO           11u
#define CELLULAR_TASK_STK_SIZE       1024
#define CELLULAR_TASK_INTERVAL_MS    10u

#define ETHERNET_TASK_PRIO           12u
#define ETHERNET_TASK_STK_SIZE       1024
#define ETHERNET_TASK_INTERVAL_MS    10u

#define VFS_TASK_PRIO                13u
#define VFS_TASK_STK_SIZE            1024
#define VFS_TASK_INTERVAL_MS         10u

void systaskCreateWorkerTasks(void);
void system_task(void *pdata);

#endif

