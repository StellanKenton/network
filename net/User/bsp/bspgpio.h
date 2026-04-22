/************************************************************************************
* @file     : bspgpio.h
* @brief    : Board GPIO BSP declarations.
***********************************************************************************/
#ifndef NETWORK_APP_BSP_GPIO_H
#define NETWORK_APP_BSP_GPIO_H

#include "../../rep/driver/drvgpio/drvgpio.h"

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