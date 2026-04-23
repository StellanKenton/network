/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
* File Name          : usb_desc.h
* Author             : MCD Application Team
* Version            : V3.2.1
* Date               : 07/05/2010
* Description        : Descriptors for CDC ACM device
*******************************************************************************/
#ifndef __USB_DESC_H
#define __USB_DESC_H

#include <stdint.h>

#define USB_DEVICE_DESCRIPTOR_TYPE              0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE       0x02
#define USB_STRING_DESCRIPTOR_TYPE              0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE           0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE            0x05

#define CDC_SIZ_DEVICE_DESC                     18
#define CDC_SIZ_CONFIG_DESC                     67
#define CDC_SIZ_STRING_LANGID                   4
#define CDC_SIZ_STRING_VENDOR                   22
#define CDC_SIZ_STRING_PRODUCT                  36
#define CDC_SIZ_STRING_SERIAL                   26

#define STANDARD_ENDPOINT_DESC_SIZE             0x09

extern const uint8_t Cdc_DeviceDescriptor[CDC_SIZ_DEVICE_DESC];
extern const uint8_t Cdc_ConfigDescriptor[CDC_SIZ_CONFIG_DESC];
extern const uint8_t Cdc_StringLangID[CDC_SIZ_STRING_LANGID];
extern const uint8_t Cdc_StringVendor[CDC_SIZ_STRING_VENDOR];
extern const uint8_t Cdc_StringProduct[CDC_SIZ_STRING_PRODUCT];
extern uint8_t Cdc_StringSerial[CDC_SIZ_STRING_SERIAL];

#endif /* __USB_DESC_H */
