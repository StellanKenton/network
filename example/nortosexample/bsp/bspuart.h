/***********************************************************************************
* @file     : bspuart.h
* @brief    : Board-specific UART binding for drvuart.
* @details  : Provides USART1 DMA transport and UART4 WiFi transport services.
* @author   : GitHub Copilot
* @date     : 2026-04-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef BSPUART_H
#define BSPUART_H

#include <stdint.h>

#include "../port/drvuart_port.h"
#include "../../rep/driver/drvuart/drvuart.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSPUART_UART1_BAUDRATE              115200U
#define BSPUART_WIFI_BAUDRATE               115200U
#define BSPUART_TX_DMA_BUFFER_SIZE          256U

extern uint8_t gBspUartRxStorageUart1[DRVUART_RECVLEN_UART1];
extern uint8_t gBspUartRxStorageWifi[DRVUART_RECVLEN_WIFI];

eDrvStatus bspUartInit(uint8_t uart);
eDrvStatus bspUartTransmit(uint8_t uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus bspUartTransmitIt(uint8_t uart, const uint8_t *buffer, uint16_t length);
eDrvStatus bspUartTransmitDma(uint8_t uart, const uint8_t *buffer, uint16_t length);
uint16_t bspUartGetDataLen(uint8_t uart);
eDrvStatus bspUartReceive(uint8_t uart, uint8_t *buffer, uint16_t length);
void bspUartHandleTxDmaIrq(uint8_t uart);
void bspUartHandleRxDmaIrq(uint8_t uart);
void bspUartHandleIrq(uint8_t uart);

#ifdef __cplusplus
}
#endif

#endif  // BSPUART_H

 /**************************End of file********************************/
