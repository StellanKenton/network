/***********************************************************************************
* @file     : drvusb_port.c
* @brief    : Project-side USB driver binding.
**********************************************************************************/
#include "drvusb_port.h"

#include "../../rep/driver/drvusb/drvusb.h"

#include "../bsp/bspusb.h"

static const stDrvUsbBspInterface gDrvUsbBspInterfaces[DRVUSB_MAX] = {
	{
		bspUsbInit,
		bspUsbStart,
		bspUsbStop,
		bspUsbSetConnect,
		bspUsbOpenEndpoint,
		bspUsbCloseEndpoint,
		bspUsbFlushEndpoint,
		bspUsbTransmit,
		bspUsbReceive,
		bspUsbIsConnected,
		bspUsbIsConfigured,
		bspUsbGetSpeed,
		DRVUSB_DEFAULT_TIMEOUT_MS,
		DRVUSB_ROLE_DEVICE,
	},
};

const stDrvUsbBspInterface *drvUsbGetPlatformBspInterfaces(void)
{
	return gDrvUsbBspInterfaces;
}

/**************************End of file********************************/