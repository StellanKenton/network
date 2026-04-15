#ifndef __LOCAL_UCOSIII_OS_H__
#define __LOCAL_UCOSIII_OS_H__

#include "cpu.h"
#include "cpu_core.h"
#include "lib_def.h"
#include "os_cfg_app.h"
#include "../CORE/ucos_ii.h"

typedef INT8U OS_ERR;
typedef OS_STK CPU_STK;
typedef INT32U CPU_STK_SIZE;
typedef void (*OS_TASK_PTR)(void *p_arg);
typedef INT32U OS_TICK;

#define OSCfg_TickRate_Hz    OS_CFG_TICK_RATE_HZ
#define OS_STATE_OS_RUNNING  1u
#define OS_ERR_Q_MAX         OS_ERR_Q_FULL

#endif
