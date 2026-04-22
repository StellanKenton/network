/************************************************************************************
* @file     : lsm6_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_LSM6_PORT_H
#define REBUILDCPR_LSM6_PORT_H

#include "lsm6_assembly.h"

#define LSM6_PORT_IIC_DELAY_US            5U
#define LSM6_PORT_RESET_DELAY_MS          20U
#define LSM6_PORT_RESET_POLL_DELAY_MS     2U

#define LSM6_PORT_SDA_GPIO_Port           GPIOB
#define LSM6_PORT_SDA_Pin                 GPIO_PIN_0
#define LSM6_PORT_SCL_GPIO_Port           GPIOB
#define LSM6_PORT_SCL_Pin                 GPIO_PIN_1

#ifdef __cplusplus
extern "C" {
#endif

void lsm6LoadPlatformDefaultCfg(eLsm6MapType device, stLsm6Cfg *cfg);
const stLsm6IicInterface *lsm6GetPlatformIicInterface(eLsm6MapType device);
bool lsm6PlatformIsValidAssemble(eLsm6MapType device);
uint8_t lsm6PlatformGetLinkId(eLsm6MapType device);
uint32_t lsm6PlatformGetResetDelayMs(void);
uint32_t lsm6PlatformGetResetPollDelayMs(void);
void lsm6PlatformDelayMs(uint32_t delayMs);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
