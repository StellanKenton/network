/***********************************************************************************
* @file     : log_port.c
* @brief    : Project-side log transport binding.
* @details  : Binds the reusable log layer to the board RTT transport.
**********************************************************************************/
#include "../../rep/service/log/log.h"
#include "../../rep/tools/trace/trace.h"

#include "../bsp/bsp_rtt.h"

static const stLogInterface gLogInterfaces[] = {
	{
		LOG_TRANSPORT_RTT,
		bspRttLogInit,
		bspRttLogWrite,
		bspRttLogGetInputBuffer,
		(BSP_RTT_LOG_OUTPUT_ENABLE != 0),
		(BSP_RTT_LOG_INPUT_ENABLE != 0)
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

void traceFaultPlatformTransportInit(void)
{
	bspRttLogInit();
}

int32_t traceFaultPlatformTransportWrite(const uint8_t *buffer, uint16_t length)
{
	return bspRttLogWrite(buffer, length);
}

/**************************End of file********************************/
