/***********************************************************************************
* @file     : drviic_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drviic_port.h"

#include "drviic.h"

#include "../bsp/bspiic.h"

static const stDrvIicBspInterface gDrvIicBspInterfaces[DRVIIC_MAX] = {
    [DRVIIC_BUS0] = {
        .init = bspIicInit,
        .transfer = bspIicTransfer,
        .recoverBus = bspIicRecoverBus,
        .defaultTimeoutMs = DRVIIC_DEFAULT_TIMEOUT_MS,
    },
    [DRVIIC_BUS1] = {
        .init = bspIicInit,
        .transfer = bspIicTransfer,
        .recoverBus = bspIicRecoverBus,
        .defaultTimeoutMs = DRVIIC_DEFAULT_TIMEOUT_MS,
    },
};

const stDrvIicBspInterface *drvIicGetPlatformBspInterfaces(void)
{
    return gDrvIicBspInterfaces;
}

/**************************End of file********************************/
