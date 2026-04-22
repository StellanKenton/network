/************************************************************************************
* @file     : bspanlogiic.h
* @brief    : Board-level software IIC BSP declarations.
* @details  : Provides GPIO and delay hooks for the reusable drvanlogiic driver.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_BSPANLOGIIC_H
#define REBUILDCPR_BSPANLOGIIC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void bspAnlogIicInit(uint8_t iic);
void bspAnlogIicSetScl(uint8_t iic, bool releaseHigh);
void bspAnlogIicSetSda(uint8_t iic, bool releaseHigh);
bool bspAnlogIicReadScl(uint8_t iic);
bool bspAnlogIicReadSda(uint8_t iic);
void bspAnlogIicDelayUs(uint16_t delayUs);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
