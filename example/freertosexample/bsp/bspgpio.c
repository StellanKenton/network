/***********************************************************************************
* @file     : bspgpio.c
* @brief    : Board-level GPIO BSP implementation.
* @details  : Maps project logical GPIO pins to the concrete STM32 GPIO resources.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bspgpio.h"

#include <stdbool.h>

#include "main.h"

#include "../port/drvgpio_port.h"

typedef struct stBspGpioMap {
    GPIO_TypeDef *gpioPort;
    uint16_t gpioPin;
    bool isOutput;
    eDrvGpioPinState defaultState;
} stBspGpioMap;

static const stBspGpioMap gBspGpioMap[DRVGPIO_MAX] = {
    [DRVGPIO_EN_AUDIO] = {
        .gpioPort = EN_AUDIO_GPIO_Port,
        .gpioPin = EN_AUDIO_Pin,
        .isOutput = true,
        .defaultState = DRVGPIO_PIN_RESET,
    },
    [DRVGPIO_RESET_WIFI] = {
        .gpioPort = RESET_WIFI_GPIO_Port,
        .gpioPin = RESET_WIFI_Pin,
        .isOutput = true,
        .defaultState = DRVGPIO_PIN_RESET,
    },
    [DRVGPIO_USB_SELECT] = {
        .gpioPort = USB_Select_GPIO_Port,
        .gpioPin = USB_Select_Pin,
        .isOutput = true,
        .defaultState = DRVGPIO_PIN_RESET,
    },
    [DRVGPIO_POWER_ON_CHECK] = {
        .gpioPort = Power_ON_Check_GPIO_Port,
        .gpioPin = Power_ON_Check_Pin,
        .isOutput = false,
        .defaultState = DRVGPIO_PIN_RESET,
    },
    [DRVGPIO_PCA9535_SCL] = {
        .gpioPort = PCA9535_SCL_GPIO_Port,
        .gpioPin = PCA9535_SCL_Pin,
        .isOutput = true,
        .defaultState = DRVGPIO_PIN_SET,
    },
    [DRVGPIO_PCA9535_SDA] = {
        .gpioPort = PCA9535_SDA_GPIO_Port,
        .gpioPin = PCA9535_SDA_Pin,
        .isOutput = true,
        .defaultState = DRVGPIO_PIN_SET,
    },
    [DRVGPIO_PCA9535_RESET] = {
        .gpioPort = PCA9535_RESET_GPIO_Port,
        .gpioPin = PCA9535_RESET_Pin,
        .isOutput = true,
        .defaultState = DRVGPIO_PIN_SET,
    },
    [DRVGPIO_TM1651_CLK] = {
        .gpioPort = MCU_LED_CLK_GPIO_Port,
        .gpioPin = MCU_LED_CLK_Pin,
        .isOutput = true,
        .defaultState = DRVGPIO_PIN_SET,
    },
    [DRVGPIO_TM1651_SDA] = {
        .gpioPort = MCU_LED_SDA_GPIO_Port,
        .gpioPin = MCU_LED_SDA_Pin,
        .isOutput = true,
        .defaultState = DRVGPIO_PIN_SET,
    },
    [DRVGPIO_SPI_CS] = {
        .gpioPort = SPI_CS_GPIO_Port,
        .gpioPin = SPI_CS_Pin,
        .isOutput = true,
        .defaultState = DRVGPIO_PIN_SET,
    },
    [DRVGPIO_POWER_ON_CTRL] = {
        .gpioPort = Power_ON_Ctrl_GPIO_Port,
        .gpioPin = Power_ON_Ctrl_Pin,
        .isOutput = true,
        .defaultState = DRVGPIO_PIN_RESET,
    },
};

void bspGpioInit(void)
{
    uint8_t lPin;

    for (lPin = 0U; lPin < DRVGPIO_MAX; lPin++) {
        if (!gBspGpioMap[lPin].isOutput) {
            continue;
        }

        HAL_GPIO_WritePin(gBspGpioMap[lPin].gpioPort,
                          gBspGpioMap[lPin].gpioPin,
                          (gBspGpioMap[lPin].defaultState == DRVGPIO_PIN_SET) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

void bspGpioWrite(uint8_t pin, eDrvGpioPinState state)
{
    if ((pin >= DRVGPIO_MAX) || !gBspGpioMap[pin].isOutput) {
        return;
    }

    HAL_GPIO_WritePin(gBspGpioMap[pin].gpioPort,
                      gBspGpioMap[pin].gpioPin,
                      (state == DRVGPIO_PIN_SET) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

eDrvGpioPinState bspGpioRead(uint8_t pin)
{
    if (pin >= DRVGPIO_MAX) {
        return DRVGPIO_PIN_STATE_INVALID;
    }

    return (HAL_GPIO_ReadPin(gBspGpioMap[pin].gpioPort, gBspGpioMap[pin].gpioPin) == GPIO_PIN_SET) ?
        DRVGPIO_PIN_SET : DRVGPIO_PIN_RESET;
}

void bspGpioToggle(uint8_t pin)
{
    if ((pin >= DRVGPIO_MAX) || !gBspGpioMap[pin].isOutput) {
        return;
    }

    HAL_GPIO_TogglePin(gBspGpioMap[pin].gpioPort, gBspGpioMap[pin].gpioPin);
}
/**************************End of file********************************/
