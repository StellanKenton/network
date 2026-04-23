/***********************************************************************************
* @file     : log_port.c
* @brief    : Project-side log transport binding.
* @details  : Binds the reusable log layer to the board RTT transport.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "../../rep/service/log/log.h"

#include "../bsp/bsp_rtt.h"

static const stLogInterface gLogInterfaces[] = {
    {
        .transport = LOG_TRANSPORT_RTT,
        .init = bspRttLogInit,
        .write = bspRttLogWrite,
        .getBuffer = bspRttLogGetInputBuffer,
        .isOutputEnabled = (BSP_RTT_LOG_OUTPUT_ENABLE != 0),
        .isInputEnabled = (BSP_RTT_LOG_INPUT_ENABLE != 0),
    },
};

const stLogInterface *logGetPlatformInterfaces(void)
{
    return gLogInterfaces;
}

uint32_t logGetPlatformInterfaceCount(void)
{
    return (uint32_t)(sizeof(gLogInterfaces) / sizeof(gLogInterfaces[0]));
}

void logPlatformConsolePoll(void)
{
    (void)bspRttLogGetInputBuffer();
}

/**************************End of file********************************/
