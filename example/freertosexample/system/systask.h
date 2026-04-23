/************************************************************************************
* @file     : systask.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_SYSTASK_H
#define REBUILDCPR_SYSTASK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define MemoryTaskStackSize         24U
#define MemoryTaskPriority          13U
#define MemoryTaskInterval          100U
static void memoryTaskEntry(void *argument);

#define PowerTaskStackSize          8U
#define PowerTaskPriority           16U
#define PowerTaskInterval           100U
static void powerTaskEntry(void *argument);

#define WirelessTaskStackSize       8U
#define WirelessTaskPriority        23U
#define WirelessTaskInterval        5U
static void wirelessTaskEntry(void *argument);

#define AudioTaskStackSize          4U
#define AudioTaskPriority           8U
#define AudioTaskInterval           20U
static void audioTaskEntry(void *argument);

#define SystemTaskStackSize         40U
#define SystemTaskPriority          5U
#define SystemTaskInterval          10U
void systemTaskEntry(void *argument);


bool systaskCreateWorkerTasks(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
