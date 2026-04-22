/***********************************************************************************
* @file     : drvanlogiic_port.c
* @brief    : Software IIC port-layer BSP binding implementation.
* @details  : This file keeps the project-level logical bus table and binds each
*             enabled bus to its BSP implementation.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvanlogiic.h"

#include "drvanlogiic_port.h"

#include "../bsp/bspanlogiic.h"

stDrvAnlogIicBspInterface gDrvAnlogIicBspInterface[DRVANLOGIIC_MAX] = {
    [DRVANLOGIIC_PCA] = {
        .init = bspAnlogIicInit,
        .setScl = bspAnlogIicSetScl,
        .setSda = bspAnlogIicSetSda,
        .readScl = bspAnlogIicReadScl,
        .readSda = bspAnlogIicReadSda,
        .delayUs = bspAnlogIicDelayUs,
        .halfPeriodUs = DRVANLOGIIC_DEFAULT_HALF_PERIOD_US,
        .recoveryClockCount = DRVANLOGIIC_DEFAULT_RECOVERY_CLOCKS,
    },
    [DRVANLOGIIC_TM] = {
        .init = bspAnlogIicInit,
        .setScl = bspAnlogIicSetScl,
        .setSda = bspAnlogIicSetSda,
        .readScl = bspAnlogIicReadScl,
        .readSda = bspAnlogIicReadSda,
        .delayUs = bspAnlogIicDelayUs,
        .halfPeriodUs = DRVANLOGIIC_DEFAULT_HALF_PERIOD_US,
        .recoveryClockCount = DRVANLOGIIC_DEFAULT_RECOVERY_CLOCKS,
    },
};

/**************************End of file********************************/
