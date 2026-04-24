/************************************************************************************
* @file     : cellular.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_MANAGER_CELLULAR_CELLULAR_H
#define NETWORK_APP_MANAGER_CELLULAR_CELLULAR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool cellularSendData(const uint8_t *buffer, uint16_t length);
void cellularProcess(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
