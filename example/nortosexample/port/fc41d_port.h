/************************************************************************************
* @file     : fc41d_port.h
* @brief    : Project-side FC41D transport binding.
* @details  : Binds the reusable FC41D core to the current board UART and tick.
* @author   : GitHub Copilot
* @date     : 2026-04-21
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_FC41D_PORT_H
#define CPRSENSORBOOT_FC41D_PORT_H

#include "../../rep/module/fc41d/fc41d_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

void fc41dLoadPlatformDefaultCfg(eFc41dMapType device, stFc41dCfg *cfg);
const stFc41dTransportInterface *fc41dGetPlatformTransportInterface(const stFc41dCfg *cfg);
const stFc41dControlInterface *fc41dGetPlatformControlInterface(eFc41dMapType device);
bool fc41dPlatformIsValidCfg(const stFc41dCfg *cfg);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_FC41D_PORT_H
/**************************End of file********************************/
