/************************************************************************************
* @file     : gd25qxxx_port.h
* @brief    : GD25Qxxx project port-layer declarations.
* @details  : This file keeps project-level SPI device mapping and timing hooks
*             separate from the reusable GD25Qxxx core implementation.
***********************************************************************************/
#ifndef GD25QXXX_PORT_H
#define GD25QXXX_PORT_H

#include <stdbool.h>

#include "../../rep/module/gd25qxxx/gd25qxxx.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GD25QXXX_PORT_READ_FILL_DATA
#define GD25QXXX_PORT_READ_FILL_DATA          0xFFU
#endif

void gd25qxxxLoadPlatformDefaultCfg(eGd25qxxxMapType device, stGd25qxxxCfg *cfg);
const stGd25qxxxSpiInterface *gd25qxxxGetPlatformSpiInterface(const stGd25qxxxCfg *cfg);
bool gd25qxxxPlatformIsValidCfg(const stGd25qxxxCfg *cfg);
void gd25qxxxPlatformDelayMs(uint32_t delayMs);

#ifdef __cplusplus
}
#endif

#endif  // GD25QXXX_PORT_H
/**************************End of file********************************/
