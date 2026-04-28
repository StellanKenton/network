/************************************************************************************
* @file     : bspiic.h
* @brief    : Board software IIC BSP interface.
***********************************************************************************/
#ifndef NETWORK_APP_BSP_BSPIIC_H
#define NETWORK_APP_BSP_BSPIIC_H

#include "../../rep/driver/drviic/drviic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSPIIC_TCA9535_DEFAULT_TIMEOUT_MS    20U

eDrvStatus bspIicInit(uint8_t iic);
eDrvStatus bspIicTransfer(uint8_t iic, const stDrvIicTransfer *transfer, uint32_t timeoutMs);
eDrvStatus bspIicRecoverBus(uint8_t iic);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
