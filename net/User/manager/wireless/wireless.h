/************************************************************************************
* @file     : wireless.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_MANAGER_WIRELESS_WIRELESS_H
#define NETWORK_APP_MANAGER_WIRELESS_WIRELESS_H

#ifdef __cplusplus
extern "C" {
#endif

enum wirlessMode {
    WIRELESS_MODE_NONE = 0,
    WIRELESS_MODE_WIFI,
    WIRELESS_MODE_BLE,
} eWirelessMode;

enum bleState {
    BLE_STATE_IDLE = 0,
    BLE_STATE_INITIALIZING,
    BLE_STATE_READY,
    BLE_WAITING_CONNECTION,
    BLE_STATE_DISCONNECTED,
    BLE_STATE_CONNECTED,
    BLE_STATE_ERROR,
    BLE_STATE_MAX,
} eBleState;

enum wifiState {
    WIFI_STATE_IDLE = 0,
    WIFI_STATE_INITIALIZING,
    WIFI_STATE_READY,
    WIFI_STATE_SCANNING,
    WIFI_STATE_WAITING_CONNECTION,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_ERROR,
    WIFI_STATE_MAX,
} eWifiState;

void wirelessProcess(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
