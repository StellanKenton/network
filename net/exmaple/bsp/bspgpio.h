/************************************************************************************
* @file     : bspgpio.h
* @brief    : Board-level GPIO BSP declarations.
* @details  : Provides the concrete GPIO hooks used by the reusable drvgpio layer.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_BSPGPIO_H
#define REBUILDCPR_BSPGPIO_H

#include "../rep_config.h"
#include "drvgpio.h"

#ifdef __cplusplus
extern "C" {
#endif

void bspGpioInit(void);
void bspGpioWrite(uint8_t pin, eDrvGpioPinState state);
eDrvGpioPinState bspGpioRead(uint8_t pin);
void bspGpioToggle(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
