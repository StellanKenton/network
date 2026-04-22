/***********************************************************************************
* @file     : drvgpio_port.c
* @brief    : Project-side GPIO driver binding.
**********************************************************************************/
#include "drvgpio_port.h"

#include "../../rep/driver/drvgpio/drvgpio.h"

#include "../bsp/bspgpio.h"

static const stDrvGpioBspInterface gDrvGpioBspInterface = {
	bspGpioInit,
	bspGpioWrite,
	bspGpioRead,
	bspGpioToggle,
};

const stDrvGpioBspInterface *drvGpioGetPlatformBspInterface(void)
{
	return &gDrvGpioBspInterface;
}

/**************************End of file********************************/