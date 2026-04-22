/************************************************************************************
* @file     : bspuart.h
* @brief    : Board-level UART BSP declarations.
* @details  : Provides STM32 HAL-backed UART hooks and raw RX storage for drvuart.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_BSPUART_H
#define REBUILDCPR_BSPUART_H

#include "../port/drvuart_port.h"
#include "drvuart.h"

#ifdef __cplusplus
extern "C" {
#endif

eDrvStatus bspUartInit(uint8_t uart);
eDrvStatus bspUartTransmit(uint8_t uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus bspUartTransmitIt(uint8_t uart, const uint8_t *buffer, uint16_t length);
eDrvStatus bspUartTransmitDma(uint8_t uart, const uint8_t *buffer, uint16_t length);
uint16_t bspUartGetDataLen(uint8_t uart);
eDrvStatus bspUartReceive(uint8_t uart, uint8_t *buffer, uint16_t length);
void bspUartHandleIrq(uint8_t uart);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
