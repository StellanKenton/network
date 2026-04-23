/************************************************************************************
* @file     : bspusb.h
* @brief    : Board-level USB BSP declarations.
* @details  : Provides STM32 USB device hooks and CDC receive binding for drvusb.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_BSPUSB_H
#define REBUILDCPR_BSPUSB_H

#include "drvusb.h"

#ifdef __cplusplus
extern "C" {
#endif

eDrvStatus bspUsbInit(uint8_t usb);
eDrvStatus bspUsbStart(uint8_t usb);
eDrvStatus bspUsbStop(uint8_t usb);
eDrvStatus bspUsbSetConnect(uint8_t usb, bool isConnect);
eDrvStatus bspUsbOpenEndpoint(uint8_t usb, const stDrvUsbEndpointConfig *config);
eDrvStatus bspUsbCloseEndpoint(uint8_t usb, uint8_t endpointAddress);
eDrvStatus bspUsbFlushEndpoint(uint8_t usb, uint8_t endpointAddress);
eDrvStatus bspUsbTransmit(uint8_t usb, uint8_t endpointAddress, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus bspUsbReceive(uint8_t usb, uint8_t endpointAddress, uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs);
bool bspUsbIsConnected(uint8_t usb);
bool bspUsbIsConfigured(uint8_t usb);
eDrvUsbSpeed bspUsbGetSpeed(uint8_t usb);

void usbdCdcIfUserReceiveCallback(const uint8_t *buffer, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
