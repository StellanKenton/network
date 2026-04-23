/***********************************************************************************
* @file     : lsm6_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "lsm6_port.h"

#include "drviic_port.h"
#include "rtos.h"

static eDrvStatus lsm6PortInit(uint8_t bus)
{
    return drvIicInit(bus);
}

static eDrvStatus lsm6PortWriteReg(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length)
{
    return drvIicWriteRegister(bus, address, regBuf, regLen, buffer, length);
}

static eDrvStatus lsm6PortReadReg(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length)
{
    return drvIicReadRegister(bus, address, regBuf, regLen, buffer, length);
}

static const stLsm6IicInterface gLsm6IicInterface = {
    .init = lsm6PortInit,
    .writeReg = lsm6PortWriteReg,
    .readReg = lsm6PortReadReg,
};

static const uint8_t gLsm6LinkMap[LSM6_DEV_MAX] = {
    [LSM6_DEV0] = DRVIIC_BUS1,
};

void lsm6LoadPlatformDefaultCfg(eLsm6MapType device, stLsm6Cfg *cfg)
{
    if ((cfg == NULL) || ((uint32_t)device >= (uint32_t)LSM6_DEV_MAX)) {
        return;
    }

    cfg->address = LSM6_IIC_ADDRESS_LOW;
    cfg->accelDataRate = LSM6_ACCEL_ODR_104HZ;
    cfg->gyroDataRate = LSM6_GYRO_ODR_104HZ;
    cfg->accelRange = LSM6_ACCEL_RANGE_4G;
    cfg->gyroRange = LSM6_GYRO_RANGE_2000DPS;
    cfg->blockDataUpdate = true;
    cfg->autoIncrement = true;
}

const stLsm6IicInterface *lsm6GetPlatformIicInterface(eLsm6MapType device)
{
    if (!lsm6PlatformIsValidAssemble(device)) {
        return NULL;
    }

    return &gLsm6IicInterface;
}

bool lsm6PlatformIsValidAssemble(eLsm6MapType device)
{
    return ((uint32_t)device < (uint32_t)LSM6_DEV_MAX) && (gLsm6LinkMap[device] < DRVIIC_MAX);
}

uint8_t lsm6PlatformGetLinkId(eLsm6MapType device)
{
    if ((uint32_t)device >= (uint32_t)LSM6_DEV_MAX) {
        return 0U;
    }

    return gLsm6LinkMap[device];
}

uint32_t lsm6PlatformGetResetDelayMs(void)
{
    return LSM6_PORT_RESET_DELAY_MS;
}

uint32_t lsm6PlatformGetResetPollDelayMs(void)
{
    return LSM6_PORT_RESET_POLL_DELAY_MS;
}

void lsm6PlatformDelayMs(uint32_t delayMs)
{
    (void)repRtosDelayMs(delayMs);
}

/**************************End of file********************************/
