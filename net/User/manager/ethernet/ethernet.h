/************************************************************************************
* @file     : ethernet.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_MANAGER_ETHERNET_ETHERNET_H
#define NETWORK_APP_MANAGER_ETHERNET_ETHERNET_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ethernetSendData(const uint8_t *buffer, uint16_t length);
void ethernetProcess(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/


