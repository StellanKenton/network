/***********************************************************************************
* @file     : update.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "update.h"

static stUpdateStatus gUpdateStatus = {
    .state = eUPDATE_STATE_UNINIT,
    .isUpdateRequested = false,
};

bool updateInit(void)
{
    if ((gUpdateStatus.state == eUPDATE_STATE_UNINIT) || (gUpdateStatus.state == eUPDATE_STATE_STOPPED)) {
        gUpdateStatus.state = eUPDATE_STATE_IDLE;
        gUpdateStatus.isUpdateRequested = false;
    }

    return true;
}

bool updateStart(void)
{
    if (!updateInit()) {
        return false;
    }

    gUpdateStatus.state = gUpdateStatus.isUpdateRequested ? eUPDATE_STATE_PENDING : eUPDATE_STATE_IDLE;
    return true;
}

void updateStop(void)
{
    if (gUpdateStatus.state == eUPDATE_STATE_UNINIT) {
        return;
    }

    gUpdateStatus.state = eUPDATE_STATE_STOPPED;
    gUpdateStatus.isUpdateRequested = false;
}

void updateProcess(void)
{
    if ((gUpdateStatus.state != eUPDATE_STATE_IDLE) &&
        (gUpdateStatus.state != eUPDATE_STATE_PENDING) &&
        (gUpdateStatus.state != eUPDATE_STATE_ACTIVE)) {
        return;
    }

    gUpdateStatus.state = gUpdateStatus.isUpdateRequested ? eUPDATE_STATE_ACTIVE : eUPDATE_STATE_IDLE;
}

const stUpdateStatus *updateGetStatus(void)
{
    return &gUpdateStatus;
}

/**************************End of file********************************/
