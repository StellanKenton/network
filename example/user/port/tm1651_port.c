/***********************************************************************************
* @file     : tm1651_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "tm1651_port.h"

#include "main.h"

#include "drvgpio.h"
#include "drvgpio_port.h"

typedef enum eTm1651LocalBus {
    TM1651_LOCAL_BUS0 = 0,
} eTm1651LocalBus;

static bool gTm1651PortReady = false;
static bool gTm1651PortCycleCntReady = false;

static void tm1651PortEnableCycleCnt(void)
{
    if (gTm1651PortCycleCntReady) {
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    gTm1651PortCycleCntReady = true;
}

static void tm1651PortDelayUs(uint16_t delayUs)
{
    uint32_t cyclesPerUs;
    uint32_t waitCycles;
    uint32_t startCycles;

    if (delayUs == 0U) {
        return;
    }

    tm1651PortEnableCycleCnt();
    cyclesPerUs = SystemCoreClock / 1000000U;
    if (cyclesPerUs == 0U) {
        cyclesPerUs = 1U;
    }

    waitCycles = cyclesPerUs * delayUs;
    startCycles = DWT->CYCCNT;
    while ((DWT->CYCCNT - startCycles) < waitCycles) {
    }
}

static void tm1651PortDriveScl(bool releaseHigh)
{
    drvGpioWrite(DRVGPIO_TM1651_CLK, releaseHigh ? DRVGPIO_PIN_SET : DRVGPIO_PIN_RESET);
}

static void tm1651PortDriveSda(bool releaseHigh)
{
    drvGpioWrite(DRVGPIO_TM1651_SDA, releaseHigh ? DRVGPIO_PIN_SET : DRVGPIO_PIN_RESET);
}

static bool tm1651PortReadSda(void)
{
    return drvGpioRead(DRVGPIO_TM1651_SDA) == DRVGPIO_PIN_SET;
}

static void tm1651PortSendStart(void)
{
    tm1651PortDriveSda(true);
    tm1651PortDriveScl(true);
    tm1651PortDelayUs(5U);
    tm1651PortDriveSda(false);
    tm1651PortDelayUs(5U);
    tm1651PortDriveScl(false);
    tm1651PortDelayUs(5U);
}

static void tm1651PortSendStop(void)
{
    tm1651PortDriveSda(false);
    tm1651PortDelayUs(5U);
    tm1651PortDriveScl(true);
    tm1651PortDelayUs(5U);
    tm1651PortDriveSda(true);
    tm1651PortDelayUs(5U);
}

static eDrvStatus tm1651PortWriteByteLsbFirst(uint8_t value)
{
    uint8_t bitIndex;

    for (bitIndex = 0U; bitIndex < 8U; ++bitIndex) {
        tm1651PortDriveSda((value & 0x01U) != 0U);
        tm1651PortDelayUs(5U);
        tm1651PortDriveScl(true);
        tm1651PortDelayUs(5U);
        tm1651PortDriveScl(false);
        tm1651PortDelayUs(5U);
        value >>= 1U;
    }

    tm1651PortDriveSda(true);
    tm1651PortDelayUs(5U);
    tm1651PortDriveScl(true);
    tm1651PortDelayUs(5U);
    if (tm1651PortReadSda()) {
        tm1651PortDriveScl(false);
        return DRV_STATUS_NACK;
    }

    tm1651PortDriveScl(false);
    tm1651PortDelayUs(5U);
    return DRV_STATUS_OK;
}

static eDrvStatus tm1651PortBusInit(uint8_t bus)
{
    if (bus != TM1651_LOCAL_BUS0) {
        return DRV_STATUS_INVALID_PARAM;
    }

    tm1651PortDriveSda(true);
    tm1651PortDriveScl(true);
    return DRV_STATUS_OK;
}

static eDrvStatus tm1651PortWriteFrameRaw(uint8_t bus, const uint8_t *buffer, uint8_t length)
{
    uint8_t index;
    eDrvStatus status;

    if ((bus != TM1651_LOCAL_BUS0) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    tm1651PortSendStart();
    for (index = 0U; index < length; ++index) {
        status = tm1651PortWriteByteLsbFirst(buffer[index]);
        if (status != DRV_STATUS_OK) {
            tm1651PortSendStop();
            return status;
        }
    }
    tm1651PortSendStop();
    return DRV_STATUS_OK;
}

static const stTm1651PortIicInterface gTm1651IicInterface = {
    .init = tm1651PortBusInit,
    .writeFrame = tm1651PortWriteFrameRaw,
};

void tm1651LoadPlatformDefaultCfg(eTm1651MapType device, stTm1651Cfg *cfg)
{
    if ((cfg == NULL) || ((uint32_t)device >= (uint32_t)TM1651_DEV_MAX)) {
        return;
    }

    cfg->brightness = 7U;
    cfg->digitCount = TM1651_DEFAULT_DIGIT_COUNT;
    cfg->isDisplayOn = true;
}

const stTm1651IicInterface *tm1651GetPlatformIicInterface(eTm1651MapType device)
{
    return tm1651PlatformIsValidAssemble(device) ? &gTm1651IicInterface : NULL;
}

bool tm1651PlatformIsValidAssemble(eTm1651MapType device)
{
    return ((uint32_t)device < (uint32_t)TM1651_DEV_MAX);
}

uint8_t tm1651PlatformGetLinkId(eTm1651MapType device)
{
    (void)device;
    return TM1651_LOCAL_BUS0;
}

static eDrvStatus tm1651PortEnsureReady(void)
{
    if (tm1651PortIsReady()) {
        return DRV_STATUS_OK;
    }

    return tm1651PortInit();
}

eDrvStatus tm1651PortInit(void)
{
    eDrvStatus status;

    if (gTm1651PortReady && tm1651IsReady(TM1651_DEV0)) {
        return DRV_STATUS_OK;
    }

    status = tm1651Init(TM1651_DEV0);
    if (status != DRV_STATUS_OK) {
        gTm1651PortReady = false;
        return status;
    }

    gTm1651PortReady = true;
    return DRV_STATUS_OK;
}

bool tm1651PortIsReady(void)
{
    return gTm1651PortReady && tm1651IsReady(TM1651_DEV0);
}

eDrvStatus tm1651PortSetBrightness(uint8_t brightness)
{
    eDrvStatus status = tm1651PortEnsureReady();

    if (status != DRV_STATUS_OK) {
        return status;
    }

    return tm1651SetBrightness(TM1651_DEV0, brightness);
}

eDrvStatus tm1651PortSetDisplayOn(bool isDisplayOn)
{
    eDrvStatus status = tm1651PortEnsureReady();

    if (status != DRV_STATUS_OK) {
        return status;
    }

    return tm1651SetDisplayOn(TM1651_DEV0, isDisplayOn);
}

eDrvStatus tm1651PortDisplayDigits(uint8_t dig1, uint8_t dig2, uint8_t dig3, uint8_t dig4)
{
    eDrvStatus status = tm1651PortEnsureReady();

    if (status != DRV_STATUS_OK) {
        return status;
    }

    return tm1651DisplayDigits(TM1651_DEV0, dig1, dig2, dig3, dig4);
}

eDrvStatus tm1651PortClearDisplay(void)
{
    eDrvStatus status = tm1651PortEnsureReady();

    if (status != DRV_STATUS_OK) {
        return status;
    }

    return tm1651ClearDisplay(TM1651_DEV0);
}

eDrvStatus tm1651PortShowNone(void)
{
    eDrvStatus status = tm1651PortEnsureReady();

    if (status != DRV_STATUS_OK) {
        return status;
    }

    return tm1651ShowNone(TM1651_DEV0);
}

eDrvStatus tm1651PortShowNumber3(uint16_t value)
{
    eDrvStatus status = tm1651PortEnsureReady();

    if (status != DRV_STATUS_OK) {
        return status;
    }

    return tm1651ShowNumber3(TM1651_DEV0, value);
}

eDrvStatus tm1651PortShowError(uint16_t value)
{
    eDrvStatus status = tm1651PortEnsureReady();

    if (status != DRV_STATUS_OK) {
        return status;
    }

    return tm1651ShowError(TM1651_DEV0, value);
}

/**************************End of file********************************/
