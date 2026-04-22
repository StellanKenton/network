/***********************************************************************************
* @file     : bspgpio.c
* @brief    : Board GPIO BSP implementation.
**********************************************************************************/
#include "bspgpio.h"

#include "../../SYSTEM/sys/sys.h"

#include "../port/drvgpio_port.h"

typedef struct stBspGpioMap {
	GPIO_TypeDef *gpioPort;
	uint16_t gpioPin;
	uint8_t isOutput;
	uint32_t pull;
	eDrvGpioPinState defaultState;
} stBspGpioMap;

static const stBspGpioMap gBspGpioMap[DRVGPIO_MAX] = {
	[DRVGPIO_WIFI_EN] = {GPIOD, GPIO_Pin_8, 1U, GPIO_PuPd_UP, DRVGPIO_PIN_RESET},
	[DRVGPIO_CELLULAR_PWRKEY] = {GPIOD, GPIO_Pin_10, 1U, GPIO_PuPd_UP, DRVGPIO_PIN_SET},
	[DRVGPIO_CELLULAR_RESET] = {GPIOD, GPIO_Pin_12, 1U, GPIO_PuPd_UP, DRVGPIO_PIN_SET},
	[DRVGPIO_FLASH_CS] = {GPIOE, GPIO_Pin_15, 1U, GPIO_PuPd_UP, DRVGPIO_PIN_SET},
	[DRVGPIO_LED_RED] = {GPIOC, GPIO_Pin_0, 1U, GPIO_PuPd_NOPULL, DRVGPIO_PIN_RESET},
	[DRVGPIO_LED_GREEN] = {GPIOC, GPIO_Pin_1, 1U, GPIO_PuPd_NOPULL, DRVGPIO_PIN_RESET},
	[DRVGPIO_LED_BLUE] = {GPIOC, GPIO_Pin_2, 1U, GPIO_PuPd_NOPULL, DRVGPIO_PIN_RESET},
	[DRVGPIO_KEY] = {GPIOC, GPIO_Pin_13, 0U, GPIO_PuPd_UP, DRVGPIO_PIN_RESET},
	[DRVGPIO_SDIO_CD] = {GPIOD, GPIO_Pin_15, 0U, GPIO_PuPd_UP, DRVGPIO_PIN_RESET},
};

static void bspGpioEnablePortClock(GPIO_TypeDef *gpioPort)
{
	if (gpioPort == GPIOA) {
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	} else if (gpioPort == GPIOB) {
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	} else if (gpioPort == GPIOC) {
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	} else if (gpioPort == GPIOD) {
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	} else if (gpioPort == GPIOE) {
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
	}
}

void bspGpioInit(void)
{
	GPIO_InitTypeDef lGpioInit;
	uint8_t lIndex;

	for (lIndex = 0U; lIndex < DRVGPIO_MAX; lIndex++) {
		bspGpioEnablePortClock(gBspGpioMap[lIndex].gpioPort);
		GPIO_StructInit(&lGpioInit);
		lGpioInit.GPIO_Pin = gBspGpioMap[lIndex].gpioPin;
		lGpioInit.GPIO_Mode = (gBspGpioMap[lIndex].isOutput != 0U) ? GPIO_Mode_OUT : GPIO_Mode_IN;
		lGpioInit.GPIO_OType = GPIO_OType_PP;
		lGpioInit.GPIO_Speed = GPIO_Speed_50MHz;
		lGpioInit.GPIO_PuPd = gBspGpioMap[lIndex].pull;
		GPIO_Init(gBspGpioMap[lIndex].gpioPort, &lGpioInit);

		if (gBspGpioMap[lIndex].isOutput != 0U) {
			if (gBspGpioMap[lIndex].defaultState == DRVGPIO_PIN_SET) {
				GPIO_SetBits(gBspGpioMap[lIndex].gpioPort, gBspGpioMap[lIndex].gpioPin);
			} else {
				GPIO_ResetBits(gBspGpioMap[lIndex].gpioPort, gBspGpioMap[lIndex].gpioPin);
			}
		}
	}
}

void bspGpioWrite(uint8_t pin, eDrvGpioPinState state)
{
	if ((pin >= DRVGPIO_MAX) || (gBspGpioMap[pin].isOutput == 0U)) {
		return;
	}

	if (state == DRVGPIO_PIN_SET) {
		GPIO_SetBits(gBspGpioMap[pin].gpioPort, gBspGpioMap[pin].gpioPin);
	} else {
		GPIO_ResetBits(gBspGpioMap[pin].gpioPort, gBspGpioMap[pin].gpioPin);
	}
}

eDrvGpioPinState bspGpioRead(uint8_t pin)
{
	BitAction lState;

	if (pin >= DRVGPIO_MAX) {
		return DRVGPIO_PIN_STATE_INVALID;
	}

	lState = GPIO_ReadInputDataBit(gBspGpioMap[pin].gpioPort, gBspGpioMap[pin].gpioPin);
	return (lState != Bit_RESET) ? DRVGPIO_PIN_SET : DRVGPIO_PIN_RESET;
}

void bspGpioToggle(uint8_t pin)
{
	if ((pin >= DRVGPIO_MAX) || (gBspGpioMap[pin].isOutput == 0U)) {
		return;
	}

	gBspGpioMap[pin].gpioPort->ODR ^= gBspGpioMap[pin].gpioPin;
}

/**************************End of file********************************/