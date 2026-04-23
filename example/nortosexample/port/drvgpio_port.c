/***********************************************************************************
* @file     : drvgpio_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvgpio_port.h"

#include "drvgpio.h"

#include "../bsp/bspgpio.h"

static const stDrvGpioBspInterface gDrvGpioBspInterface = {
    .init = bspGpioInit,
    .write = bspGpioWrite,
    .read = bspGpioRead,
    .toggle = bspGpioToggle,
};

const stDrvGpioBspInterface *drvGpioGetPlatformBspInterface(void)
{
    return &gDrvGpioBspInterface;
}

/**************************End of file********************************/

