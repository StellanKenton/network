/************************************************************************************
* @file     : drvspi_port.h
* @brief    : Project SPI logical bus mapping.
***********************************************************************************/
#ifndef NETWORK_APP_PORT_DRVSPI_PORT_H
#define NETWORK_APP_PORT_DRVSPI_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvSpiPortMap {
	DRVSPI_FLASH = 0,
	DRVSPI_MAX,
} eDrvSpiPortMap;

#define DRVSPI_DEFAULT_TIMEOUT_MS  100U

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/