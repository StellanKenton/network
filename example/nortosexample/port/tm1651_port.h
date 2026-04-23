/************************************************************************************
* @file     : tm1651_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_TM1651_PORT_H
#define REBUILDCPR_TM1651_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "tm1651.h"
#include "tm1651_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef stTm1651IicInterface stTm1651PortIicInterface;
typedef stTm1651AssembleCfg stTm1651PortAssembleCfg;

eDrvStatus tm1651PortGetAssembleCfg(eTm1651MapType device, stTm1651PortAssembleCfg *cfg);
eDrvStatus tm1651PortInit(void);
bool tm1651PortIsReady(void);
eDrvStatus tm1651PortSetBrightness(uint8_t brightness);
eDrvStatus tm1651PortSetDisplayOn(bool isDisplayOn);
eDrvStatus tm1651PortDisplayDigits(uint8_t dig1, uint8_t dig2, uint8_t dig3, uint8_t dig4);
eDrvStatus tm1651PortClearDisplay(void);
eDrvStatus tm1651PortShowNone(void);
eDrvStatus tm1651PortShowBoo(void);
eDrvStatus tm1651PortShowNumber3(uint16_t value);
eDrvStatus tm1651PortShowError(uint16_t value);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
