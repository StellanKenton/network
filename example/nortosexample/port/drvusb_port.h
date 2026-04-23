/************************************************************************************
* @file     : drvusb_port.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_DRVUSB_PORT_H
#define REBUILDCPR_DRVUSB_PORT_H

#include "../bsp/bspusb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvUsbPortMap {
    DRVUSB_DEV0 = 0,
    DRVUSB_DEV_MAX
} eDrvUsbPortMap;

#ifndef DRVUSB_PORT_RX_STORAGE_FS
#define DRVUSB_PORT_RX_STORAGE_FS            BSPUSB_CDC_RX_BUFFER_SIZE
#endif

#define DRVUSB_PORT_CDC_DATA_IN_EP           BSPUSB_CDC_DATA_IN_EP
#define DRVUSB_PORT_CDC_DATA_OUT_EP          BSPUSB_CDC_DATA_OUT_EP
#define DRVUSB_PORT_CDC_CMD_EP               BSPUSB_CDC_CMD_EP

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
