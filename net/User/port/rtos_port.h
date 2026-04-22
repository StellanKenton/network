/************************************************************************************
* @file     : rtos_port.h
* @brief    : Project RTOS adapter binding declarations.
***********************************************************************************/
#ifndef NETWORK_APP_PORT_RTOS_PORT_H
#define NETWORK_APP_PORT_RTOS_PORT_H

#include <stdint.h>

#include "../../rep/service/rtos/rtos.h"

#ifdef __cplusplus
extern "C" {
#endif

const stRepRtosOps *rtosPortGetOps(void);
const char *rtosPortGetName(void);
uint32_t rtosPortGetSystem(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
