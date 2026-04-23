/************************************************************************************
* @file     : drvgpio_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_DRVGPIO_PORT_H
#define REBUILDCPR_DRVGPIO_PORT_H

#ifndef DRVGPIO_MAX
#define DRVGPIO_MAX                     7U
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvGpioPinMap {
    DRVGPIO_BUZZER_PWM = 0,
    DRVGPIO_RESET_WIFI,
    DRVGPIO_USB_SELECT,
    DRVGPIO_POWER_ON_CTRL,
    DRVGPIO_TM1651_CLK,
    DRVGPIO_TM1651_SDA,
    DRVGPIO_SPI_CS,
} eDrvGpioPinMap;

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
