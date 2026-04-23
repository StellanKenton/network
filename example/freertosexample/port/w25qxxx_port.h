/************************************************************************************
* @file     : w25qxxx_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_W25QXXX_PORT_H
#define REBUILDCPR_W25QXXX_PORT_H

#include "w25qxxx.h"

#ifdef __cplusplus
extern "C" {
#endif

void w25qxxxLoadPlatformDefaultCfg(eW25qxxxMapType device, stW25qxxxCfg *cfg);
const stW25qxxxSpiInterface *w25qxxxGetPlatformSpiInterface(const stW25qxxxCfg *cfg);
bool w25qxxxPlatformIsValidCfg(const stW25qxxxCfg *cfg);
void w25qxxxPlatformDelayMs(uint32_t delayMs);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
