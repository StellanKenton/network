/************************************************************************************
* @file     : ec800m_port.h
* @brief    : Project-side EC800M transport binding.
* @details  : Binds EC800M core to USB and cellular control GPIOs.
* @author   : GitHub Copilot
* @date     : 2026-04-27
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_PORT_EC800M_PORT_H
#define NETWORK_APP_PORT_EC800M_PORT_H

#include "../../rep/module/ec800m/ec800m_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

void ec800mLoadPlatformDefaultCfg(eEc800mMapType device, stEc800mCfg *cfg);
const stEc800mTransportInterface *ec800mGetPlatformTransportInterface(const stEc800mCfg *cfg);
const stEc800mControlInterface *ec800mGetPlatformControlInterface(eEc800mMapType device);
bool ec800mPlatformIsValidCfg(const stEc800mCfg *cfg);

#ifdef __cplusplus
}
#endif

#endif  // NETWORK_APP_PORT_EC800M_PORT_H
/**************************End of file********************************/
