/************************************************************************************
* @file     : esp32c5_port.h
* @brief    : Project-side ESP32-C5 transport binding.
* @details  : Binds the reusable ESP32-C5 BLE core to the current board UART,
*             RTOS tick, and WIFI_EN control pin.
* @author   : GitHub Copilot
* @date     : 2026-04-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_PORT_ESP32C5_PORT_H
#define NETWORK_APP_PORT_ESP32C5_PORT_H

#include "../../rep/module/esp32c5/esp32c5_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

void esp32c5LoadPlatformDefaultCfg(eEsp32c5MapType device, stEsp32c5Cfg *cfg);
const stEsp32c5TransportInterface *esp32c5GetPlatformTransportInterface(const stEsp32c5Cfg *cfg);
const stEsp32c5ControlInterface *esp32c5GetPlatformControlInterface(eEsp32c5MapType device);
bool esp32c5PlatformIsValidCfg(const stEsp32c5Cfg *cfg);

#ifdef __cplusplus
}
#endif

#endif  // NETWORK_APP_PORT_ESP32C5_PORT_H
/**************************End of file********************************/
