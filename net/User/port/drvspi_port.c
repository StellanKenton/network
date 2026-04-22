/***********************************************************************************
* @file     : drvspi_port.c
* @brief    : Project-side SPI driver binding.
**********************************************************************************/
#include "drvspi_port.h"

#include "../../rep/driver/drvspi/drvspi.h"

#include "../bsp/bspspi.h"

static const stDrvSpiBspInterface gDrvSpiBspInterfaces[DRVSPI_MAX] = {
	{
		bspSpiInit,
		bspSpiTransfer,
		DRVSPI_DEFAULT_TIMEOUT_MS,
		{
			bspSpiCsInit,
			bspSpiCsWrite,
			(void *)&gBspSpiFlashCsPin,
		},
	},
};

const stDrvSpiBspInterface *drvSpiGetPlatformBspInterfaces(void)
{
	return gDrvSpiBspInterfaces;
}

/**************************End of file********************************/