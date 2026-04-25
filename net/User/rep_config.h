/************************************************************************************
* @file     : rep_config.h
* @brief    : Project-level configuration overrides for reusable modules.
***********************************************************************************/
#ifndef NETWORK_APP_REP_CONFIG_H
#define NETWORK_APP_REP_CONFIG_H

#include "../rep/rep.h"

#define DRVGPIO_MAX                     9U
#define DRVUART_MAX                     2U
#define CONSOLE_MAX_LINE_LENGTH         160U

#ifndef REP_RTOS_SYSTEM
#define REP_RTOS_SYSTEM                 REP_RTOS_UCOSII
#endif

#ifndef REP_MCU_PLATFORM
#define REP_MCU_PLATFORM                REP_MCU_PLATFORM_GD32
#endif

#ifndef REP_LOG_LEVEL
#define REP_LOG_LEVEL                   REP_LOG_LEVEL_INFO
#endif

#ifndef REP_LOG_OUTPUT_PORT
#define REP_LOG_OUTPUT_PORT             1U
#endif

#endif
/**************************End of file********************************/
