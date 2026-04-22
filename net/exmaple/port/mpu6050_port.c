/***********************************************************************************
* @file     : mpu6050_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "mpu6050_port.h"

#include "main.h"
#include "drviic_port.h"
#include "rtos.h"

static eDrvStatus mpu6050PortInit(uint8_t bus)
{
    return drvIicInit(bus);
}

static eDrvStatus mpu6050PortWriteReg(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length)
{
    return drvIicWriteRegister(bus, address, regBuf, regLen, buffer, length);
}

static eDrvStatus mpu6050PortReadReg(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length)
{
    return drvIicReadRegister(bus, address, regBuf, regLen, buffer, length);
}

static const stMpu6050IicInterface gMpu6050IicInterface = {
    .init = mpu6050PortInit,
    .writeReg = mpu6050PortWriteReg,
    .readReg = mpu6050PortReadReg,
};

static const uint8_t gMpu6050LinkMap[MPU6050_DEV_MAX] = {
    [MPU6050_DEV0] = DRVIIC_BUS0,
    [MPU6050_DEV1] = DRVIIC_BUS0,
};

void mpu6050LoadPlatformDefaultCfg(eMPU6050MapType device, stMpu6050Cfg *cfg)
{
    if ((cfg == NULL) || ((uint32_t)device >= (uint32_t)MPU6050_DEV_MAX)) {
        return;
    }

    cfg->address = (device == MPU6050_DEV0) ? MPU6050_IIC_ADDRESS_LOW : MPU6050_IIC_ADDRESS_HIGH;
    cfg->sampleRateDiv = 0U;
    cfg->dlpfCfg = 3U;
    cfg->accelRange = MPU6050_ACCEL_RANGE_4G;
    cfg->gyroRange = MPU6050_GYRO_RANGE_2000DPS;
}

const stMpu6050IicInterface *mpu6050GetPlatformIicInterface(eMPU6050MapType device)
{
    if (!mpu6050PlatformIsValidAssemble(device)) {
        return NULL;
    }

    return &gMpu6050IicInterface;
}

bool mpu6050PlatformIsValidAssemble(eMPU6050MapType device)
{
    return ((uint32_t)device < (uint32_t)MPU6050_DEV_MAX) && (gMpu6050LinkMap[device] < DRVIIC_MAX);
}

uint8_t mpu6050PlatformGetLinkId(eMPU6050MapType device)
{
    if ((uint32_t)device >= (uint32_t)MPU6050_DEV_MAX) {
        return 0U;
    }

    return gMpu6050LinkMap[device];
}

uint32_t mpu6050PlatformGetResetDelayMs(void)
{
    return MPU6050_PORT_RESET_DELAY_MS;
}

uint32_t mpu6050PlatformGetWakeDelayMs(void)
{
    return MPU6050_PORT_WAKE_DELAY_MS;
}

void mpu6050PlatformDelayMs(uint32_t delayMs)
{
    (void)repRtosDelayMs(delayMs);
}

/**************************End of file********************************/
