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

#include "usbd_cdc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvUsbPortMap {
    DRVUSB_DEV0 = 0,
    DRVUSB_DEV_MAX
} eDrvUsbPortMap;

#ifndef DRVUSB_PORT_RX_STORAGE_FS
#define DRVUSB_PORT_RX_STORAGE_FS            APP_RX_DATA_SIZE
#endif

#define DRVUSB_PORT_CDC_DATA_IN_EP           CDC_IN_EP
#define DRVUSB_PORT_CDC_DATA_OUT_EP          CDC_OUT_EP
#define DRVUSB_PORT_CDC_CMD_EP               CDC_CMD_EP

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
