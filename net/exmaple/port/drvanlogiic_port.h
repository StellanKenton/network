/************************************************************************************
* @file     : drvanlogiic_port.h
* @brief    : Shared software IIC logical bus mapping definitions.
* @details  : This file keeps the project-level software IIC identifiers and
*             default timing options independent from the driver interface.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVANLOGIIC_PORT_H
#define DRVANLOGIIC_PORT_H

#include "../../rep/driver/drvanlogiic/drvanlogiic.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRVANLOGIIC_LOG_SUPPORT
#define DRVANLOGIIC_LOG_SUPPORT                 1
#endif

#ifndef DRVANLOGIIC_CONSOLE_SUPPORT
#define DRVANLOGIIC_CONSOLE_SUPPORT             1
#endif

#define DRVANLOGIIC_LOCK_WAIT_MS                5U
#define DRVANLOGIIC_DEFAULT_HALF_PERIOD_US      10U
#define DRVANLOGIIC_DEFAULT_RECOVERY_CLOCKS     9U

typedef enum eDrvAnlogIicPortMap {
    DRVANLOGIIC_PCA = 0,
    DRVANLOGIIC_TM,
    DRVANLOGIIC_PORT_MAX = DRVANLOGIIC_MAX,
} eDrvAnlogIicPortMap;

#ifdef __cplusplus
}
#endif

#endif  // DRVANLOGIIC_PORT_H
/**************************End of file********************************/
