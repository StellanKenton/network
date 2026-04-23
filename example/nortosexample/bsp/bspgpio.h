/************************************************************************************
* @file     : bspgpio.h
* @brief    : Board-specific GPIO binding for drvgpio.
* @details  : Maps project logical GPIO IDs to STM32F103 physical pins.
* @author   : GitHub Copilot
* @date     : 2026-04-16
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef BSPGPIO_H
#define BSPGPIO_H

#include <stdint.h>

#include "../port/drvgpio_port.h"
#include "../../rep/driver/drvgpio/drvgpio.h"
#include "stm32f10x_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eBspGpioDirection {
    BSPGPIO_DIR_OUTPUT = 0,
    BSPGPIO_DIR_INPUT,
} eBspGpioDirection;

typedef enum eBspGpioMode {
    BSPGPIO_MODE_PUSH_PULL = 0,
    BSPGPIO_MODE_OPEN_DRAIN,
} eBspGpioMode;

typedef struct stBspGpioPinMap {
    uint32_t gpioClock;
    GPIO_TypeDef *gpioPort;
    uint16_t gpioPin;
    eBspGpioDirection direction;
    eBspGpioMode mode;
    bool isActiveHigh;
    eDrvGpioPinState defaultState;
} stBspGpioPinMap;

void bspGpioInit(void);
void bspGpioWrite(uint8_t pin, eDrvGpioPinState state);
eDrvGpioPinState bspGpioRead(uint8_t pin);
void bspGpioToggle(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif  // BSPGPIO_H
/**************************End of file********************************/
