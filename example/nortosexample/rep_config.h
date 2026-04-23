#ifndef REP_CONFIG_H
#define REP_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "../rep/rep.h"

/* Compatibility macros for modules that still use these names in #if branches. */

#ifndef REP_MCU_PLATFORM
#define REP_MCU_PLATFORM REP_MCU_PLATFORM_STM32
#endif

#define REP_STM32_MCU_SERIES REP_STM32_F1

#ifndef REP_RTOS_SYSTEM
#define REP_RTOS_SYSTEM REP_RTOS_NONE
#endif

#ifndef REP_LOG_LEVEL
#define REP_LOG_LEVEL 4U
#endif

#ifndef REP_LOG_OUTPUT_PORT
#define REP_LOG_OUTPUT_PORT 1U
#endif

#ifndef DRVADC_MAX
#define DRVADC_MAX 5U
#endif

#ifndef DRVUART_MAX
#define DRVUART_MAX 2U
#endif

#ifndef DRVANLOGIIC_MAX
#define DRVANLOGIIC_MAX 2U
#endif

#ifndef DRVGPIO_MAX
#define DRVGPIO_MAX 7U
#endif

#ifndef TM1651_CONSOLE_SUPPORT
#define TM1651_CONSOLE_SUPPORT 0
#endif

#ifndef PCA9535_CONSOLE_SUPPORT
#define PCA9535_CONSOLE_SUPPORT 0
#endif

#endif
