/************************************************************************************
* @file     : drvusb_port.h
* @brief    : Project USB logical mapping and endpoint defaults.
***********************************************************************************/
#ifndef NETWORK_APP_PORT_DRVUSB_PORT_H
#define NETWORK_APP_PORT_DRVUSB_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvUsbPortMap {
	DRVUSB_CELLULAR = 0,
	DRVUSB_DEV_MAX,
} eDrvUsbPortMap;

#ifndef DRVUSB_PORT_RX_STORAGE_FS
#define DRVUSB_PORT_RX_STORAGE_FS     256U
#endif

#define DRVUSB_PORT_CDC_DATA_IN_EP    0x81U
#define DRVUSB_PORT_CDC_DATA_OUT_EP   0x01U
#define DRVUSB_PORT_CDC_CMD_EP        0x82U

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/