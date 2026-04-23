/************************************************************************************
* @file     : rep_config.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef PROJECT_REBUILDCPR_REP_CONFIG_H
#define PROJECT_REBUILDCPR_REP_CONFIG_H

#include "../rep/rep.h"


#ifndef NULL
#define NULL ((void *)0)
#endif

#undef REP_MCU_PLATFORM
#define REP_MCU_PLATFORM REP_MCU_PLATFORM_STM32

#undef REP_RTOS_SYSTEM
#define REP_RTOS_SYSTEM REP_RTOS_FREERTOS

#ifndef REP_RTOS_CMSIS_FREERTOS
#define REP_RTOS_CMSIS_FREERTOS REP_RTOS_CUBEMX_FREERTOS
#endif

#ifndef REP_LOG_LEVEL
#define REP_LOG_LEVEL REP_LOG_LEVEL_INFO
#endif

#ifndef REP_LOG_OUTPUT_PORT
#define REP_LOG_OUTPUT_PORT 1U
#endif

#ifndef CONSOLE_MAX_COMMANDS
#define CONSOLE_MAX_COMMANDS 24U
#endif

#ifndef DRVIIC_MAX
#define DRVIIC_MAX 2U
#endif

#ifndef DRVSPI_MAX
#define DRVSPI_MAX 1U
#endif

#ifndef DRVUART_MAX
#define DRVUART_MAX 2U
#endif

#ifndef DRVADC_MAX
#define DRVADC_MAX 5U
#endif

#ifndef DRVGPIO_MAX
#define DRVGPIO_MAX 11U
#endif

#ifndef DRVUSB_MAX
#define DRVUSB_MAX 1U
#endif

#ifndef DRVIIC_LOG_SUPPORT
#define DRVIIC_LOG_SUPPORT 0
#endif

#ifndef DRVSPI_LOG_SUPPORT
#define DRVSPI_LOG_SUPPORT 0
#endif

#ifndef DRVUART_LOG_SUPPORT
#define DRVUART_LOG_SUPPORT 0
#endif

#ifndef DRVADC_LOG_SUPPORT
#define DRVADC_LOG_SUPPORT 0
#endif

#ifndef DRVUSB_LOG_SUPPORT
#define DRVUSB_LOG_SUPPORT 0
#endif

#ifndef DRVIIC_CONSOLE_SUPPORT
#define DRVIIC_CONSOLE_SUPPORT 1
#endif

#ifndef DRVGPIO_CONSOLE_SUPPORT
#define DRVGPIO_CONSOLE_SUPPORT 1
#endif

#ifndef DRVSPI_CONSOLE_SUPPORT
#define DRVSPI_CONSOLE_SUPPORT 1
#endif

#ifndef DRVUART_CONSOLE_SUPPORT
#define DRVUART_CONSOLE_SUPPORT 1
#endif

#ifndef DRVADC_CONSOLE_SUPPORT
#define DRVADC_CONSOLE_SUPPORT 1
#endif

#ifndef DRVUSB_CONSOLE_SUPPORT
#define DRVUSB_CONSOLE_SUPPORT 0
#endif

#ifndef GD25QXXX_CONSOLE_SUPPORT
#define GD25QXXX_CONSOLE_SUPPORT 0
#endif

#ifndef PCA9535_CONSOLE_SUPPORT
#define PCA9535_CONSOLE_SUPPORT 0
#endif

#ifndef TM1651_CONSOLE_SUPPORT
#define TM1651_CONSOLE_SUPPORT 0
#endif

#endif
/**************************End of file********************************/
