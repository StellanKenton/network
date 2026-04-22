/***********************************************************************************
* @file     : bspanlogiic.c
* @brief    : Board-level software IIC BSP implementation.
* @details  : Maps logical software IIC buses to the PCA9535 and TM1651 GPIOs.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bspanlogiic.h"

#include "main.h"

#include "drvgpio.h"
#include "drvgpio_port.h"

#include "../port/drvanlogiic_port.h"

static bool gBspAnlogIicCycleCntReady = false;

static void bspAnlogIicEnableCycleCnt(void);
static uint8_t bspAnlogIicGetSclPin(uint8_t iic);
static uint8_t bspAnlogIicGetSdaPin(uint8_t iic);

void bspAnlogIicInit(uint8_t iic)
{
    bspAnlogIicSetSda(iic, true);
    bspAnlogIicSetScl(iic, true);
}

void bspAnlogIicSetScl(uint8_t iic, bool releaseHigh)
{
    uint8_t lPin = bspAnlogIicGetSclPin(iic);

    if (lPin >= DRVGPIO_MAX) {
        return;
    }

    drvGpioWrite(lPin, releaseHigh ? DRVGPIO_PIN_SET : DRVGPIO_PIN_RESET);
}

void bspAnlogIicSetSda(uint8_t iic, bool releaseHigh)
{
    uint8_t lPin = bspAnlogIicGetSdaPin(iic);

    if (lPin >= DRVGPIO_MAX) {
        return;
    }

    drvGpioWrite(lPin, releaseHigh ? DRVGPIO_PIN_SET : DRVGPIO_PIN_RESET);
}

bool bspAnlogIicReadScl(uint8_t iic)
{
    uint8_t lPin = bspAnlogIicGetSclPin(iic);

    return (lPin < DRVGPIO_MAX) && (drvGpioRead(lPin) == DRVGPIO_PIN_SET);
}

bool bspAnlogIicReadSda(uint8_t iic)
{
    uint8_t lPin = bspAnlogIicGetSdaPin(iic);

    return (lPin < DRVGPIO_MAX) && (drvGpioRead(lPin) == DRVGPIO_PIN_SET);
}

void bspAnlogIicDelayUs(uint16_t delayUs)
{
    uint32_t lCyclesPerUs;
    uint32_t lWaitCycles;
    uint32_t lStartCycles;

    if (delayUs == 0U) {
        return;
    }

    bspAnlogIicEnableCycleCnt();
    lCyclesPerUs = SystemCoreClock / 1000000U;
    if (lCyclesPerUs == 0U) {
        lCyclesPerUs = 1U;
    }

    lWaitCycles = lCyclesPerUs * delayUs;
    lStartCycles = DWT->CYCCNT;
    while ((DWT->CYCCNT - lStartCycles) < lWaitCycles) {
    }
}

static void bspAnlogIicEnableCycleCnt(void)
{
    if (gBspAnlogIicCycleCntReady) {
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    gBspAnlogIicCycleCntReady = true;
}

static uint8_t bspAnlogIicGetSclPin(uint8_t iic)
{
    switch ((eDrvAnlogIicPortMap)iic) {
        case DRVANLOGIIC_PCA:
            return DRVGPIO_PCA9535_SCL;
        case DRVANLOGIIC_TM:
            return DRVGPIO_TM1651_CLK;
        default:
            return DRVGPIO_MAX;
    }
}

static uint8_t bspAnlogIicGetSdaPin(uint8_t iic)
{
    switch ((eDrvAnlogIicPortMap)iic) {
        case DRVANLOGIIC_PCA:
            return DRVGPIO_PCA9535_SDA;
        case DRVANLOGIIC_TM:
            return DRVGPIO_TM1651_SDA;
        default:
            return DRVGPIO_MAX;
    }
}
/**************************End of file********************************/
