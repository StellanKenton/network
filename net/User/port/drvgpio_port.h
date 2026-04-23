/************************************************************************************
* @file     : drvgpio_port.h
* @brief    : Project GPIO logical pin mapping.
***********************************************************************************/
#ifndef NETWORK_APP_PORT_DRVGPIO_PORT_H
#define NETWORK_APP_PORT_DRVGPIO_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvGpioPinMap {
	DRVGPIO_WIFI_EN = 0,
	DRVGPIO_CELLULAR_PWRKEY,
	DRVGPIO_CELLULAR_RESET,
	DRVGPIO_FLASH_CS,
	DRVGPIO_LED_RED,
	DRVGPIO_LED_GREEN,
	DRVGPIO_LED_BLUE,
	DRVGPIO_KEY,
	DRVGPIO_SDIO_CD,
} eDrvGpioPinMap;

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
