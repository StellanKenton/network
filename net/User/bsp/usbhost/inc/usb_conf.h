/************************************************************************************
* @file     : usb_conf.h
* @brief    : STM32 USB OTG host stack configuration for this project.
***********************************************************************************/
#ifndef NETWORK_USB_CONF_H
#define NETWORK_USB_CONF_H

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#define USE_USB_OTG_FS
#define USB_OTG_FS_CORE
#define USE_HOST_MODE

#define RX_FIFO_FS_SIZE                          128
#define TXH_NP_FS_FIFOSIZ                         96
#define TXH_P_FS_FIFOSIZ                          96

#define __ALIGN_BEGIN
#define __ALIGN_END

#if defined(__CC_ARM)
#define __packed                                 __packed
#elif defined(__ICCARM__)
#define __packed                                 __packed
#elif defined(__GNUC__)
#define __packed                                 __attribute__((__packed__))
#elif defined(__TASKING__)
#define __packed                                 __unaligned
#endif

#endif
/**************************End of file********************************/
