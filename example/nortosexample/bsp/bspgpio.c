/***********************************************************************************
* @file     : bspgpio.c
* @brief    : Board-specific GPIO binding for drvgpio.
* @details  : Provides logical GPIO to STM32F103 pin mapping and polarity handling.
* @author   : GitHub Copilot
* @date     : 2026-04-16
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bspgpio.h"

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

#include <stdbool.h>
#include <stddef.h>

static const stBspGpioPinMap gBspGpioPinMap[DRVGPIO_MAX] = {
    [DRVGPIO_BUZZER_PWM] = {
        .gpioClock = RCC_APB2Periph_GPIOB,
        .gpioPort = GPIOB,
        .gpioPin = GPIO_Pin_6,
        .direction = BSPGPIO_DIR_OUTPUT,
        .mode = BSPGPIO_MODE_PUSH_PULL,
        .isActiveHigh = true,
        .defaultState = DRVGPIO_PIN_RESET,
    },
    [DRVGPIO_RESET_WIFI] = {
        .gpioClock = RCC_APB2Periph_GPIOB,
        .gpioPort = GPIOB,
        .gpioPin = GPIO_Pin_15,
        .direction = BSPGPIO_DIR_OUTPUT,
        .mode = BSPGPIO_MODE_PUSH_PULL,
        .isActiveHigh = false,
        .defaultState = DRVGPIO_PIN_RESET,
    },
    [DRVGPIO_USB_SELECT] = {
        .gpioClock = RCC_APB2Periph_GPIOA,
        .gpioPort = GPIOA,
        .gpioPin = GPIO_Pin_8,
        .direction = BSPGPIO_DIR_OUTPUT,
        .mode = BSPGPIO_MODE_PUSH_PULL,
        .isActiveHigh = true,
        .defaultState = DRVGPIO_PIN_RESET,
    },
    [DRVGPIO_POWER_ON_CTRL] = {
        .gpioClock = RCC_APB2Periph_GPIOC,
        .gpioPort = GPIOC,
        .gpioPin = GPIO_Pin_3,
        .direction = BSPGPIO_DIR_OUTPUT,
        .mode = BSPGPIO_MODE_PUSH_PULL,
        .isActiveHigh = true,
        .defaultState = DRVGPIO_PIN_RESET,
    },
    [DRVGPIO_TM1651_CLK] = {
        .gpioClock = RCC_APB2Periph_GPIOD,
        .gpioPort = GPIOD,
        .gpioPin = GPIO_Pin_2,
        .direction = BSPGPIO_DIR_OUTPUT,
        .mode = BSPGPIO_MODE_OPEN_DRAIN,
        .isActiveHigh = true,
        .defaultState = DRVGPIO_PIN_SET,
    },
    [DRVGPIO_TM1651_SDA] = {
        .gpioClock = RCC_APB2Periph_GPIOC,
        .gpioPort = GPIOC,
        .gpioPin = GPIO_Pin_12,
        .direction = BSPGPIO_DIR_OUTPUT,
        .mode = BSPGPIO_MODE_OPEN_DRAIN,
        .isActiveHigh = true,
        .defaultState = DRVGPIO_PIN_SET,
    },
    [DRVGPIO_SPI_CS] = {
        .gpioClock = RCC_APB2Periph_GPIOA,
        .gpioPort = GPIOA,
        .gpioPin = GPIO_Pin_4,
        .direction = BSPGPIO_DIR_OUTPUT,
        .mode = BSPGPIO_MODE_PUSH_PULL,
        .isActiveHigh = false,
        .defaultState = DRVGPIO_PIN_RESET,
    },
};

static const stBspGpioPinMap *bspGpioGetPinMap(uint8_t pin)
{
    if (pin >= (uint8_t)DRVGPIO_MAX) {
        return NULL;
    }

    if ((gBspGpioPinMap[pin].gpioPort == NULL) || (gBspGpioPinMap[pin].gpioPin == 0U)) {
        return NULL;
    }

    return &gBspGpioPinMap[pin];
}

static BitAction bspGpioToPhysicalState(const stBspGpioPinMap *pinMap, eDrvGpioPinState state)
{
    bool driveHigh;

    driveHigh = (state == DRVGPIO_PIN_SET) ? pinMap->isActiveHigh : !pinMap->isActiveHigh;
    return driveHigh ? Bit_SET : Bit_RESET;
}

static eDrvGpioPinState bspGpioFromPhysicalState(const stBspGpioPinMap *pinMap, BitAction state)
{
    bool isHigh = (state == Bit_SET);

    return (isHigh == pinMap->isActiveHigh) ? DRVGPIO_PIN_SET : DRVGPIO_PIN_RESET;
}

static void bspGpioInitOne(const stBspGpioPinMap *pinMap)
{
    GPIO_InitTypeDef lGpioInit;

    if (pinMap == NULL) {
        return;
    }

    GPIO_StructInit(&lGpioInit);
    lGpioInit.GPIO_Pin = pinMap->gpioPin;
    lGpioInit.GPIO_Speed = GPIO_Speed_50MHz;
    if (pinMap->direction == BSPGPIO_DIR_OUTPUT) {
        lGpioInit.GPIO_Mode = (pinMap->mode == BSPGPIO_MODE_OPEN_DRAIN) ?
                              GPIO_Mode_Out_OD :
                              GPIO_Mode_Out_PP;
    }
    else {
        lGpioInit.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    }
    GPIO_Init(pinMap->gpioPort, &lGpioInit);

    if (pinMap->direction == BSPGPIO_DIR_OUTPUT) {
        GPIO_WriteBit(pinMap->gpioPort,
                      pinMap->gpioPin,
                      bspGpioToPhysicalState(pinMap, pinMap->defaultState));
    }
}

void bspGpioInit(void)
{
    uint8_t lIndex;
    uint32_t lClockMask = 0U;

    for (lIndex = 0U; lIndex < (uint8_t)DRVGPIO_MAX; ++lIndex) {
        const stBspGpioPinMap *lPinMap = bspGpioGetPinMap(lIndex);

        if (lPinMap != NULL) {
            lClockMask |= lPinMap->gpioClock;
        }
    }

    if (lClockMask != 0U) {
        RCC_APB2PeriphClockCmd(lClockMask, ENABLE);
    }

    for (lIndex = 0U; lIndex < (uint8_t)DRVGPIO_MAX; ++lIndex) {
        bspGpioInitOne(bspGpioGetPinMap(lIndex));
    }
}

void bspGpioWrite(uint8_t pin, eDrvGpioPinState state)
{
    const stBspGpioPinMap *lPinMap = bspGpioGetPinMap(pin);

    if ((lPinMap == NULL) || (lPinMap->direction != BSPGPIO_DIR_OUTPUT)) {
        return;
    }

    GPIO_WriteBit(lPinMap->gpioPort, lPinMap->gpioPin, bspGpioToPhysicalState(lPinMap, state));
}

eDrvGpioPinState bspGpioRead(uint8_t pin)
{
    const stBspGpioPinMap *lPinMap = bspGpioGetPinMap(pin);

    if (lPinMap == NULL) {
        return DRVGPIO_PIN_STATE_INVALID;
    }

    return bspGpioFromPhysicalState(lPinMap,
                                    (BitAction)GPIO_ReadInputDataBit(lPinMap->gpioPort, lPinMap->gpioPin));
}

void bspGpioToggle(uint8_t pin)
{
    eDrvGpioPinState lState;
    const stBspGpioPinMap *lPinMap = bspGpioGetPinMap(pin);

    if ((lPinMap == NULL) || (lPinMap->direction != BSPGPIO_DIR_OUTPUT)) {
        return;
    }

    lState = bspGpioRead(pin);
    if (lState == DRVGPIO_PIN_STATE_INVALID) {
        return;
    }

    bspGpioWrite(pin, (lState == DRVGPIO_PIN_SET) ? DRVGPIO_PIN_RESET : DRVGPIO_PIN_SET);
}

/**************************End of file********************************/
