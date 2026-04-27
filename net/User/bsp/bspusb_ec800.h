/************************************************************************************
* @file     : bspusb_ec800.h
* @brief    : EC800M USB transport helper declarations.
* @details  : Bridges the EC800M module port to the project USB driver mapping.
* @author   : GitHub Copilot
* @date     : 2026-04-27
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_BSP_USB_EC800_H
#define NETWORK_APP_BSP_USB_EC800_H

#include <stdbool.h>
#include <stdint.h>

#include "../../rep/driver/drvusb/drvusb.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BSPUSB_EC800_RX_STORAGE_SIZE
#define BSPUSB_EC800_RX_STORAGE_SIZE     768U
#endif

#ifndef BSPUSB_EC800_RX_POLL_CHUNK_SIZE
#define BSPUSB_EC800_RX_POLL_CHUNK_SIZE  64U
#endif

eDrvStatus bspUsbEc800Init(uint8_t linkId);
eDrvStatus bspUsbEc800Write(uint8_t linkId, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
uint16_t bspUsbEc800GetRxLen(uint8_t linkId);
eDrvStatus bspUsbEc800Read(uint8_t linkId, uint8_t *buffer, uint16_t length);
bool bspUsbEc800IsLinkReady(uint8_t linkId);

#ifdef __cplusplus
}
#endif

#endif  // NETWORK_APP_BSP_USB_EC800_H
/**************************End of file********************************/
