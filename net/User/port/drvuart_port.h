/************************************************************************************
* @file     : drvuart_port.h
* @brief    : Project UART logical port mapping.
***********************************************************************************/
#ifndef NETWORK_APP_PORT_DRVUART_PORT_H
#define NETWORK_APP_PORT_DRVUART_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvUartPortMap {
	DRVUART_WIFI = 0,
	DRVUART_CELLULAR,
} eDrvUartPortMap;

#define DRVUART_RECVLEN_WIFI         512U
#define DRVUART_RECVLEN_CELLULAR     1024U
#define DRVUART_RECVLEN_AUDIO        DRVUART_RECVLEN_WIFI
#define DRVUART_RECVLEN_DEBUGUART    DRVUART_RECVLEN_CELLULAR

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
