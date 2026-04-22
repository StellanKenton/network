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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvUartPortMapTable {
    DRVUART_DEBUG = 0,
    DRVUART_AUDIO,
} eDrvUartPortMap;

#define DRVUART_RECVLEN_DEBUGUART 256U
#define DRVUART_RECVLEN_AUDIO 256U

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
