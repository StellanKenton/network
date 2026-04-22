/***********************************************************************************
* @file     : power.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "power.h"

static stPowerStatus gPowerStatus = {
    .state = ePOWER_STATE_UNINIT,
    .isLowPowerRequested = false,
};

bool powerInit(void)
{
    if ((gPowerStatus.state == ePOWER_STATE_UNINIT) || (gPowerStatus.state == ePOWER_STATE_FAULT)) {
        gPowerStatus.state = ePOWER_STATE_READY;
        gPowerStatus.isLowPowerRequested = false;
    }

    return true;
}

void powerProcess(void)
{
    if ((gPowerStatus.state != ePOWER_STATE_ACTIVE) &&
        (gPowerStatus.state != ePOWER_STATE_LOW_POWER)) {
        return;
    }

    gPowerStatus.state = gPowerStatus.isLowPowerRequested ? ePOWER_STATE_LOW_POWER : ePOWER_STATE_ACTIVE;
}

bool powerRequestLowPower(bool isEnabled)
{
    if ((gPowerStatus.state == ePOWER_STATE_UNINIT) || (gPowerStatus.state == ePOWER_STATE_FAULT)) {
        return false;
    }

    gPowerStatus.isLowPowerRequested = isEnabled;
    return true;
}

const stPowerStatus *powerGetStatus(void)
{
    return &gPowerStatus;
}

/**************************End of file********************************/
