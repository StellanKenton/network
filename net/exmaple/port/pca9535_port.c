/***********************************************************************************
* @file     : pca9535_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "pca9535_port.h"

#include "main.h"
#include "rtos.h"

#include "drvgpio.h"
#include "drvgpio_port.h"

typedef enum ePca9535LocalBus {
    PCA9535_LOCAL_BUS0 = 0,
} ePca9535LocalBus;

static bool gPca9535PortReady = false;
static uint16_t gPca9535PortShowMask = 0U;
static bool gPca9535PortCycleCntReady = false;

static void pca9535PortEnableCycleCnt(void)
{
    if (gPca9535PortCycleCntReady) {
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    gPca9535PortCycleCntReady = true;
}

static void pca9535PortDelayUs(uint16_t delayUs)
{
    uint32_t cyclesPerUs;
    uint32_t waitCycles;
    uint32_t startCycles;

    if (delayUs == 0U) {
        return;
    }

    pca9535PortEnableCycleCnt();
    cyclesPerUs = SystemCoreClock / 1000000U;
    if (cyclesPerUs == 0U) {
        cyclesPerUs = 1U;
    }

    waitCycles = cyclesPerUs * delayUs;
    startCycles = DWT->CYCCNT;
    while ((DWT->CYCCNT - startCycles) < waitCycles) {
    }
}

static void pca9535PortDriveScl(bool releaseHigh)
{
    drvGpioWrite(DRVGPIO_PCA9535_SCL, releaseHigh ? DRVGPIO_PIN_SET : DRVGPIO_PIN_RESET);
}

static void pca9535PortDriveSda(bool releaseHigh)
{
    drvGpioWrite(DRVGPIO_PCA9535_SDA, releaseHigh ? DRVGPIO_PIN_SET : DRVGPIO_PIN_RESET);
}

static bool pca9535PortReadSda(void)
{
    return drvGpioRead(DRVGPIO_PCA9535_SDA) == DRVGPIO_PIN_SET;
}

static void pca9535PortSendStart(void)
{
    pca9535PortDriveSda(true);
    pca9535PortDriveScl(true);
    pca9535PortDelayUs(5U);
    pca9535PortDriveSda(false);
    pca9535PortDelayUs(5U);
    pca9535PortDriveScl(false);
    pca9535PortDelayUs(5U);
}

static void pca9535PortSendStop(void)
{
    pca9535PortDriveSda(false);
    pca9535PortDelayUs(5U);
    pca9535PortDriveScl(true);
    pca9535PortDelayUs(5U);
    pca9535PortDriveSda(true);
    pca9535PortDelayUs(5U);
}

static eDrvStatus pca9535PortWriteByte(uint8_t value)
{
    uint8_t bitIndex;

    for (bitIndex = 0U; bitIndex < 8U; ++bitIndex) {
        pca9535PortDriveSda((value & 0x80U) != 0U);
        pca9535PortDelayUs(5U);
        pca9535PortDriveScl(true);
        pca9535PortDelayUs(5U);
        pca9535PortDriveScl(false);
        value <<= 1U;
    }

    pca9535PortDriveSda(true);
    pca9535PortDelayUs(5U);
    pca9535PortDriveScl(true);
    pca9535PortDelayUs(5U);
    if (pca9535PortReadSda()) {
        pca9535PortDriveScl(false);
        return DRV_STATUS_NACK;
    }

    pca9535PortDriveScl(false);
    pca9535PortDelayUs(5U);
    return DRV_STATUS_OK;
}

static uint8_t pca9535PortReadByte(bool sendAck)
{
    uint8_t bitIndex;
    uint8_t value = 0U;

    pca9535PortDriveSda(true);
    for (bitIndex = 0U; bitIndex < 8U; ++bitIndex) {
        value <<= 1U;
        pca9535PortDriveScl(true);
        pca9535PortDelayUs(5U);
        if (pca9535PortReadSda()) {
            value |= 0x01U;
        }
        pca9535PortDriveScl(false);
        pca9535PortDelayUs(5U);
    }

    pca9535PortDriveSda(!sendAck);
    pca9535PortDelayUs(5U);
    pca9535PortDriveScl(true);
    pca9535PortDelayUs(5U);
    pca9535PortDriveScl(false);
    pca9535PortDriveSda(true);
    return value;
}

static eDrvStatus pca9535PortBusInit(uint8_t bus)
{
    if (bus != PCA9535_LOCAL_BUS0) {
        return DRV_STATUS_INVALID_PARAM;
    }

    pca9535PortDriveSda(true);
    pca9535PortDriveScl(true);
    return DRV_STATUS_OK;
}

static eDrvStatus pca9535PortWriteReg(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length)
{
    uint16_t index;
    eDrvStatus status;

    if ((bus != PCA9535_LOCAL_BUS0) || (regBuf == NULL) || (regLen == 0U) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    pca9535PortSendStart();
    status = pca9535PortWriteByte((uint8_t)(address << 1U));
    if (status != DRV_STATUS_OK) {
        pca9535PortSendStop();
        return status;
    }

    for (index = 0U; index < regLen; ++index) {
        status = pca9535PortWriteByte(regBuf[index]);
        if (status != DRV_STATUS_OK) {
            pca9535PortSendStop();
            return status;
        }
    }

    for (index = 0U; index < length; ++index) {
        status = pca9535PortWriteByte(buffer[index]);
        if (status != DRV_STATUS_OK) {
            pca9535PortSendStop();
            return status;
        }
    }

    pca9535PortSendStop();
    return DRV_STATUS_OK;
}

static eDrvStatus pca9535PortReadReg(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length)
{
    uint16_t index;
    eDrvStatus status;

    if ((bus != PCA9535_LOCAL_BUS0) || (regBuf == NULL) || (regLen == 0U) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    pca9535PortSendStart();
    status = pca9535PortWriteByte((uint8_t)(address << 1U));
    if (status != DRV_STATUS_OK) {
        pca9535PortSendStop();
        return status;
    }

    for (index = 0U; index < regLen; ++index) {
        status = pca9535PortWriteByte(regBuf[index]);
        if (status != DRV_STATUS_OK) {
            pca9535PortSendStop();
            return status;
        }
    }

    pca9535PortSendStart();
    status = pca9535PortWriteByte((uint8_t)((address << 1U) | 0x01U));
    if (status != DRV_STATUS_OK) {
        pca9535PortSendStop();
        return status;
    }

    for (index = 0U; index < length; ++index) {
        buffer[index] = pca9535PortReadByte(index < (uint16_t)(length - 1U));
    }

    pca9535PortSendStop();
    return DRV_STATUS_OK;
}

static const stPca9535PortIicInterface gPca9535IicInterface = {
    .init = pca9535PortBusInit,
    .writeReg = pca9535PortWriteReg,
    .readReg = pca9535PortReadReg,
};

void pca9535LoadPlatformDefaultCfg(ePca9535MapType device, stPca9535Cfg *cfg)
{
    if ((cfg == NULL) || ((uint32_t)device >= (uint32_t)PCA9535_DEV_MAX)) {
        return;
    }

    cfg->address = PCA9535_IIC_ADDRESS_HLL;
    cfg->outputValue = 0xFFFFU;
    cfg->polarityMask = 0x0000U;
    cfg->directionMask = 0xFFFFU;
    cfg->resetBeforeInit = true;
}

const stPca9535IicInterface *pca9535GetPlatformIicInterface(ePca9535MapType device)
{
    return pca9535PlatformIsValidAssemble(device) ? &gPca9535IicInterface : NULL;
}

bool pca9535PlatformIsValidAssemble(ePca9535MapType device)
{
    return ((uint32_t)device < (uint32_t)PCA9535_DEV_MAX);
}

uint8_t pca9535PlatformGetLinkId(ePca9535MapType device)
{
    (void)device;
    return PCA9535_LOCAL_BUS0;
}

void pca9535PlatformResetInit(void)
{
    drvGpioWrite(DRVGPIO_PCA9535_RESET, DRVGPIO_PIN_SET);
}

void pca9535PlatformResetWrite(bool assertReset)
{
    drvGpioWrite(DRVGPIO_PCA9535_RESET, assertReset ? DRVGPIO_PIN_RESET : DRVGPIO_PIN_SET);
}

uint32_t pca9535PlatformGetResetAssertDelayMs(void)
{
    return PCA9535_PORT_RESET_ASSERT_MS;
}

uint32_t pca9535PlatformGetResetReleaseDelayMs(void)
{
    return PCA9535_PORT_RESET_RELEASE_MS;
}

void pca9535PlatformDelayMs(uint32_t delayMs)
{
    (void)repRtosDelayMs(delayMs);
}

static eDrvStatus pca9535PortApplyShowMask(uint16_t mask)
{
    uint16_t rawPortMask = (uint16_t)(~mask);
    eDrvStatus status;

    status = pca9535SetOutputPort(PCA9535_DEV0, rawPortMask);
    if (status != DRV_STATUS_OK) {
        return status;
    }

    status = pca9535SetDirectionPort(PCA9535_DEV0, rawPortMask);
    if (status != DRV_STATUS_OK) {
        return status;
    }

    return pca9535SetPolarityPort(PCA9535_DEV0, 0U);
}

static eDrvStatus pca9535PortEnsureReady(void)
{
    if (pca9535PortIsReady()) {
        return DRV_STATUS_OK;
    }

    return pca9535PortInit();
}

eDrvStatus pca9535PortInit(void)
{
    eDrvStatus status;

    if (gPca9535PortReady && pca9535IsReady(PCA9535_DEV0)) {
        return DRV_STATUS_OK;
    }

    status = pca9535Init(PCA9535_DEV0);
    if (status != DRV_STATUS_OK) {
        gPca9535PortReady = false;
        return status;
    }

    gPca9535PortReady = true;
    gPca9535PortShowMask = 0U;
    return pca9535PortApplyShowMask(gPca9535PortShowMask);
}

bool pca9535PortIsReady(void)
{
    return gPca9535PortReady && pca9535IsReady(PCA9535_DEV0);
}

eDrvStatus pca9535PortLedOff(void)
{
    return pca9535PortSetShowMask(0U);
}

eDrvStatus pca9535PortLedLightNum(uint8_t num)
{
    static const uint16_t numMap[PCA9535_PORT_LED_MAX + 1U] = {
        0x0000U,
        PCA9535_PORT_LED_NUM_1,
        (uint16_t)(PCA9535_PORT_LED_NUM_1 | PCA9535_PORT_LED_NUM_8),
        (uint16_t)(PCA9535_PORT_LED_NUM_1 | PCA9535_PORT_LED_NUM_8 | PCA9535_PORT_LED_NUM_7),
        (uint16_t)(PCA9535_PORT_LED_NUM_1 | PCA9535_PORT_LED_NUM_8 | PCA9535_PORT_LED_NUM_7 | PCA9535_PORT_LED_NUM_6),
        (uint16_t)(PCA9535_PORT_LED_NUM_1 | PCA9535_PORT_LED_NUM_8 | PCA9535_PORT_LED_NUM_7 | PCA9535_PORT_LED_NUM_6 | PCA9535_PORT_LED_NUM_5),
        (uint16_t)(PCA9535_PORT_LED_NUM_1 | PCA9535_PORT_LED_NUM_8 | PCA9535_PORT_LED_NUM_7 | PCA9535_PORT_LED_NUM_6 | PCA9535_PORT_LED_NUM_5 | PCA9535_PORT_LED_NUM_4),
        (uint16_t)(PCA9535_PORT_LED_NUM_1 | PCA9535_PORT_LED_NUM_8 | PCA9535_PORT_LED_NUM_7 | PCA9535_PORT_LED_NUM_6 | PCA9535_PORT_LED_NUM_5 | PCA9535_PORT_LED_NUM_4 | PCA9535_PORT_LED_NUM_3),
        (uint16_t)(PCA9535_PORT_LED_NUM_1 | PCA9535_PORT_LED_NUM_8 | PCA9535_PORT_LED_NUM_7 | PCA9535_PORT_LED_NUM_6 | PCA9535_PORT_LED_NUM_5 | PCA9535_PORT_LED_NUM_4 | PCA9535_PORT_LED_NUM_3 | PCA9535_PORT_LED_NUM_2),
    };
    uint16_t mask;

    if (num > PCA9535_PORT_LED_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    mask = gPca9535PortShowMask;
    mask &= (uint16_t)~(PCA9535_PORT_LED_NUM_1 |
                        PCA9535_PORT_LED_NUM_2 |
                        PCA9535_PORT_LED_NUM_3 |
                        PCA9535_PORT_LED_NUM_4 |
                        PCA9535_PORT_LED_NUM_5 |
                        PCA9535_PORT_LED_NUM_6 |
                        PCA9535_PORT_LED_NUM_7 |
                        PCA9535_PORT_LED_NUM_8);
    mask |= numMap[num];
    return pca9535PortSetShowMask(mask);
}

eDrvStatus pca9535PortLedPowerShow(bool isRedOn, bool isGreenOn, bool isBlueOn)
{
    uint16_t mask = gPca9535PortShowMask;

    mask &= (uint16_t)~(PCA9535_PORT_LED_POWER_RED |
                        PCA9535_PORT_LED_POWER_GREEN |
                        PCA9535_PORT_LED_POWER_BLUE);
    if (isRedOn) {
        mask |= PCA9535_PORT_LED_POWER_RED;
    }
    if (isGreenOn) {
        mask |= PCA9535_PORT_LED_POWER_GREEN;
    }
    if (isBlueOn) {
        mask |= PCA9535_PORT_LED_POWER_BLUE;
    }

    return pca9535PortSetShowMask(mask);
}

eDrvStatus pca9535PortLedPressShow(bool isRedOn, bool isGreenOn, bool isBlueOn)
{
    uint16_t mask = gPca9535PortShowMask;

    mask &= (uint16_t)~(PCA9535_PORT_LED_PRESS_RED |
                        PCA9535_PORT_LED_PRESS_GREEN |
                        PCA9535_PORT_LED_PRESS_BLUE);
    if (isRedOn) {
        mask |= PCA9535_PORT_LED_PRESS_RED;
    }
    if (isGreenOn) {
        mask |= PCA9535_PORT_LED_PRESS_GREEN;
    }
    if (isBlueOn) {
        mask |= PCA9535_PORT_LED_PRESS_BLUE;
    }

    return pca9535PortSetShowMask(mask);
}

eDrvStatus pca9535PortSetShowMask(uint16_t mask)
{
    eDrvStatus status = pca9535PortEnsureReady();

    if (status != DRV_STATUS_OK) {
        return status;
    }

    status = pca9535PortApplyShowMask(mask);
    if (status == DRV_STATUS_OK) {
        gPca9535PortShowMask = mask;
    }
    return status;
}

eDrvStatus pca9535PortGetShowMask(uint16_t *mask)
{
    if (mask == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    *mask = gPca9535PortShowMask;
    return DRV_STATUS_OK;
}

eDrvStatus pca9535PortReadInputPort(uint16_t *value)
{
    eDrvStatus status;

    if (value == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    status = pca9535PortEnsureReady();
    if (status != DRV_STATUS_OK) {
        return status;
    }

    return pca9535ReadInputPort(PCA9535_DEV0, value);
}

void pca9535PortResetInit(void)
{
    pca9535PlatformResetInit();
}

void pca9535PortResetWrite(bool assertReset)
{
    pca9535PlatformResetWrite(assertReset);
}

void pca9535PortDelayMs(uint32_t delayMs)
{
    pca9535PlatformDelayMs(delayMs);
}

/**************************End of file********************************/
