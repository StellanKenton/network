/************************************************************************************
* @file     : bspuart.h
* @brief    : Board UART BSP declarations.
***********************************************************************************/
#ifndef NETWORK_APP_BSP_UART_H
#define NETWORK_APP_BSP_UART_H

#include "../port/drvuart_port.h"
#include "../../rep/driver/drvuart/drvuart.h"

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
void bspUartHandleDmaRxIrq(uint8_t uart);
void bspUartHandleDmaTxIrq(uint8_t uart);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
