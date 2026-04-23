/************************************************************************************
* @file     : bspiic.h
* @brief    : Board-level hardware IIC BSP declarations.
* @details  : Provides STM32 HAL-backed hooks for the reusable drviic layer.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_BSPIIC_H
#define REBUILDCPR_BSPIIC_H

#include "drviic.h"

#ifdef __cplusplus
extern "C" {
#endif

eDrvStatus bspIicInit(uint8_t iic);
eDrvStatus bspIicRecoverBus(uint8_t iic);
eDrvStatus bspIicTransfer(uint8_t iic, const stDrvIicTransfer *transfer, uint32_t timeoutMs);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
