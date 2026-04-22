/************************************************************************************
* @file     : rep_config.h
* @brief    : Project reusable-layer configuration.
* @details  : Selects STM32 + uC/OS-II runtime options for log and console services.
***********************************************************************************/
#ifndef NETWORK_USER_REP_CONFIG_H
#define NETWORK_USER_REP_CONFIG_H

#include "../rep/rep.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

#undef REP_MCU_PLATFORM
#define REP_MCU_PLATFORM REP_MCU_PLATFORM_STM32

#undef REP_RTOS_SYSTEM
#define REP_RTOS_SYSTEM REP_RTOS_UCOSII

#ifndef REP_LOG_LEVEL
#define REP_LOG_LEVEL REP_LOG_LEVEL_INFO
#endif

#ifndef REP_LOG_OUTPUT_PORT
#define REP_LOG_OUTPUT_PORT 1U
#endif

#endif
/**************************End of file********************************/
