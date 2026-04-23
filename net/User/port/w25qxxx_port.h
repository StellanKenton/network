/************************************************************************************
* @file     : w25qxxx_port.h
* @brief    : W25Qxxx project port-layer declarations.
***********************************************************************************/
#ifndef NETWORK_APP_PORT_W25QXXX_PORT_H
#define NETWORK_APP_PORT_W25QXXX_PORT_H

#include <stdbool.h>

#include "../../rep/module/w25qxxx/w25qxxx.h"

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
