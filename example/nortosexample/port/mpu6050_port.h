/************************************************************************************
* @file     : mpu6050_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_MPU6050_PORT_H
#define REBUILDCPR_MPU6050_PORT_H

#include "mpu6050_assembly.h"

#define MPU6050_PORT_RESET_DELAY_MS 100U
#define MPU6050_PORT_WAKE_DELAY_MS 10U

#ifdef __cplusplus
extern "C" {
#endif

void mpu6050LoadPlatformDefaultCfg(eMPU6050MapType device, stMpu6050Cfg *cfg);
const stMpu6050IicInterface *mpu6050GetPlatformIicInterface(eMPU6050MapType device);
bool mpu6050PlatformIsValidAssemble(eMPU6050MapType device);
uint8_t mpu6050PlatformGetLinkId(eMPU6050MapType device);
uint32_t mpu6050PlatformGetResetDelayMs(void);
uint32_t mpu6050PlatformGetWakeDelayMs(void);
void mpu6050PlatformDelayMs(uint32_t delayMs);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
