/************************************************************************************
* @file     : pca9535_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_PCA9535_PORT_H
#define REBUILDCPR_PCA9535_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "pca9535.h"
#include "pca9535_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PCA9535_PORT_RESET_ASSERT_MS         1U
#define PCA9535_PORT_RESET_RELEASE_MS        1U

#define PCA9535_PORT_LED_MAX                 8U

#define PCA9535_PORT_LED_PRESS_RED           0x0001U
#define PCA9535_PORT_LED_NUM_2               0x0002U
#define PCA9535_PORT_LED_NUM_3               0x0004U
#define PCA9535_PORT_LED_NUM_4               0x0008U
#define PCA9535_PORT_LED_NUM_5               0x0010U
#define PCA9535_PORT_LED_NUM_6               0x0020U
#define PCA9535_PORT_LED_NUM_7               0x0040U
#define PCA9535_PORT_LED_NUM_8               0x0080U
#define PCA9535_PORT_LED_NUM_1               0x0100U
#define PCA9535_PORT_LED_POWER_BLUE          0x0800U
#define PCA9535_PORT_LED_POWER_GREEN         0x1000U
#define PCA9535_PORT_LED_POWER_RED           0x2000U
#define PCA9535_PORT_LED_PRESS_BLUE          0x4000U
#define PCA9535_PORT_LED_PRESS_GREEN         0x8000U

typedef stPca9535IicInterface stPca9535PortIicInterface;
typedef stPca9535AssembleCfg stPca9535PortAssembleCfg;

eDrvStatus pca9535PortInit(void);
bool pca9535PortIsReady(void);
eDrvStatus pca9535PortLedOff(void);
eDrvStatus pca9535PortLedLightNum(uint8_t num);
eDrvStatus pca9535PortLedPowerShow(bool isRedOn, bool isGreenOn, bool isBlueOn);
eDrvStatus pca9535PortLedPressShow(bool isRedOn, bool isGreenOn, bool isBlueOn);
eDrvStatus pca9535PortSetShowMask(uint16_t mask);
eDrvStatus pca9535PortGetShowMask(uint16_t *mask);
eDrvStatus pca9535PortReadInputPort(uint16_t *value);
void pca9535PortResetInit(void);
void pca9535PortResetWrite(bool assertReset);
void pca9535PortDelayMs(uint32_t delayMs);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
