/***********************************************************************************
* @file     : drviic_port.c
* @brief    : Project IIC platform binding.
**********************************************************************************/
#include "drviic_port.h"

#include "../../rep/driver/drviic/drviic.h"
#include "../bsp/bspiic.h"

static const stDrvIicBspInterface gDrvIicBspInterfaces[DRVIIC_MAX] = {
    [DRVIIC_TCA9535] = {
        .init = bspIicInit,
        .transfer = bspIicTransfer,
        .recoverBus = bspIicRecoverBus,
        .defaultTimeoutMs = BSPIIC_TCA9535_DEFAULT_TIMEOUT_MS,
    },
};

const stDrvIicBspInterface *drvIicGetPlatformBspInterfaces(void)
{
    return &gDrvIicBspInterfaces[0];
}

/**************************End of file********************************/
