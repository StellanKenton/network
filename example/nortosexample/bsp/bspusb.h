/************************************************************************************
* @file     : bspusb.h
* @brief    : Board-specific USB CDC device interface.
* @details  : Bridges STM32F103 USB FS peripheral and the reusable drvusb layer.
* @author   : GitHub Copilot
* @date     : 2026-04-17
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef BSPUSB_H
#define BSPUSB_H

#include <stdbool.h>
#include <stdint.h>

#include "../../rep/driver/drvusb/drvusb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSPUSB_DEVICE_ID                    0U

#define BSPUSB_CDC_DATA_IN_EP               0x81U
#define BSPUSB_CDC_CMD_EP                   0x82U
#define BSPUSB_CDC_DATA_OUT_EP              0x03U

#define BSPUSB_CDC_DATA_MAX_PACKET_SIZE     64U
#define BSPUSB_CDC_CMD_PACKET_SIZE          8U
#define BSPUSB_CDC_RX_BUFFER_SIZE           256U

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
bool bspUsbIsSuspended(uint8_t usb);
eDrvUsbSpeed bspUsbGetSpeed(uint8_t usb);
void bspUsbForceDisconnectEarly(void);

uint32_t bspUsbCdcGetRxLength(void);
eDrvStatus bspUsbCdcWrite(const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus bspUsbCdcRead(uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs);

void bspUsbNotifyConfigured(bool isConfigured);
void bspUsbNotifyCableState(bool isConnected);
void bspUsbResetTransferState(void);
void bspUsbHandleDataOut(void);
void bspUsbHandleDataIn(uint8_t endpointAddress);

#ifdef __cplusplus
}
#endif

#endif  // BSPUSB_H
/**************************End of file********************************/
