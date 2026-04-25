/************************************************************************************
* @file     : wireless_wifi.h
* @brief    : Project wireless WiFi public interface.
* @details  : Provides the WiFi, HTTP auth, and MQTT subset of the wireless manager API.
* @author   : GitHub Copilot
* @date     : 2026-04-25
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_MANAGER_WIRELESS_WIRELESS_WIFI_H
#define NETWORK_APP_MANAGER_WIRELESS_WIRELESS_WIFI_H

#include "wireless.h"

#ifdef __cplusplus
extern "C" {
#endif

enum wifiState wirelessGetWifiState(void);
eWirelessIotState wirelessGetIotState(void);
bool wirelessGetWifiEnabled(void);
bool wirelessSetWifiEnabled(bool enabled);
bool wirelessConnectWifi(const uint8_t *ssid, uint8_t ssidLen, const uint8_t *password, uint8_t passwordLen);
bool wirelessGetMqttEnabled(void);
bool wirelessSetMqttEnabled(bool enabled);
bool wirelessSendWifiData(const uint8_t *buffer, uint16_t length);
bool wirelessSetWifiCredentials(const uint8_t *ssid, uint8_t ssidLen, const uint8_t *password, uint8_t passwordLen);

#ifdef __cplusplus
}
#endif

#endif  // NETWORK_APP_MANAGER_WIRELESS_WIRELESS_WIFI_H
/**************************End of file********************************/
