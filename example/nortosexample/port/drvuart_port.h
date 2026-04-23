/************************************************************************************
* @file     : drvuart_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_DRVUART_PORT_H
#define REBUILDCPR_DRVUART_PORT_H

#include "../rep_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvUartPortMapTable {
    DRVUART_UART1 = 0,
    DRVUART_WIFI,
    DRVUART_PORT_MAX = DRVUART_MAX,
} eDrvUartPortMap;

#define DRVUART_RECVLEN_UART1 256U
#define DRVUART_RECVLEN_WIFI 256U

#ifdef __cplusplus
}
#endif


#endif
/**************************End of file********************************/
