/************************************************************************************
* @file     : usbhost_ec800.h
* @brief    : EC800M USB host bridge for drvusb BSP hooks.
***********************************************************************************/
#ifndef NETWORK_USBHOST_EC800_H
#define NETWORK_USBHOST_EC800_H

#include <stdbool.h>
#include <stdint.h>

#include "../../../rep/driver/drvusb/drvusb.h"

#ifdef __cplusplus
extern "C" {
#endif

eDrvStatus usbHostEc800Init(void);
eDrvStatus usbHostEc800Start(void);
eDrvStatus usbHostEc800Stop(void);
void usbHostEc800Process(void);
void usbHostEc800HandleIrq(void);
eDrvStatus usbHostEc800Transmit(uint8_t endpointAddress, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus usbHostEc800Receive(uint8_t endpointAddress, uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs);
bool usbHostEc800IsConnected(void);
bool usbHostEc800IsConfigured(void);
eDrvUsbSpeed usbHostEc800GetSpeed(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
