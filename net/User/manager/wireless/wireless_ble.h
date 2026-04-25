/************************************************************************************
* @file     : wireless_ble.h
* @brief    : Project wireless BLE public interface.
* @details  : Provides the BLE-facing subset of the wireless manager API.
* @author   : GitHub Copilot
* @date     : 2026-04-25
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_MANAGER_WIRELESS_WIRELESS_BLE_H
#define NETWORK_APP_MANAGER_WIRELESS_WIRELESS_BLE_H

#include "wireless.h"

#ifdef __cplusplus
extern "C" {
#endif

enum bleState wirelessGetBleState(void);
bool wirelessGetBleEnabled(void);
bool wirelessSetBleEnabled(bool enabled);
bool wirelessSendBleData(const uint8_t *buffer, uint16_t length);
bool wirelessGetMacAddress(char *buffer, uint16_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif  // NETWORK_APP_MANAGER_WIRELESS_WIRELESS_BLE_H
/**************************End of file********************************/
