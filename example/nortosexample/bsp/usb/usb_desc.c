/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
* File Name          : usb_desc.c
* Author             : MCD Application Team
* Version            : V3.2.1
* Date               : 07/05/2010
* Description        : CDC ACM descriptors
*******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "usb_desc.h"

const uint8_t Cdc_DeviceDescriptor[CDC_SIZ_DEVICE_DESC] = {
    CDC_SIZ_DEVICE_DESC,
    USB_DEVICE_DESCRIPTOR_TYPE,
    0x00, 0x02,
    0x02,
    0x02,
    0x02,
    0x40,
    0x83, 0x04,
    0x40, 0x57,
    0x00, 0x02,
    0x01,
    0x02,
    0x03,
    0x01
};

const uint8_t Cdc_ConfigDescriptor[CDC_SIZ_CONFIG_DESC] = {
    0x09,
    USB_CONFIGURATION_DESCRIPTOR_TYPE,
    CDC_SIZ_CONFIG_DESC,
    0x00,
    0x02,
    0x01,
    0x00,
    0xC0,
    0x32,

    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x00,
    0x00,
    0x01,
    0x02,
    0x02,
    0x01,
    0x00,

    0x05,
    0x24,
    0x00,
    0x10, 0x01,

    0x05,
    0x24,
    0x01,
    0x00,
    0x01,

    0x04,
    0x24,
    0x02,
    0x02,

    0x05,
    0x24,
    0x06,
    0x00,
    0x01,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x82,
    0x03,
    0x08, 0x00,
    0xFF,

    0x09,
    USB_INTERFACE_DESCRIPTOR_TYPE,
    0x01,
    0x00,
    0x02,
    0x0A,
    0x00,
    0x00,
    0x00,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x03,
    0x02,
    0x40, 0x00,
    0x00,

    0x07,
    USB_ENDPOINT_DESCRIPTOR_TYPE,
    0x81,
    0x02,
    0x40, 0x00,
    0x00
};

const uint8_t Cdc_StringLangID[CDC_SIZ_STRING_LANGID] = {
    CDC_SIZ_STRING_LANGID,
    USB_STRING_DESCRIPTOR_TYPE,
    0x09, 0x04
};

const uint8_t Cdc_StringVendor[CDC_SIZ_STRING_VENDOR] = {
    CDC_SIZ_STRING_VENDOR,
    USB_STRING_DESCRIPTOR_TYPE,
    'C', 0x00,
    'P', 0x00,
    'R', 0x00,
    ' ', 0x00,
    'S', 0x00,
    'e', 0x00,
    'n', 0x00,
    's', 0x00,
    'o', 0x00,
    'r', 0x00
};

const uint8_t Cdc_StringProduct[CDC_SIZ_STRING_PRODUCT] = {
    CDC_SIZ_STRING_PRODUCT,
    USB_STRING_DESCRIPTOR_TYPE,
    'C', 0x00,
    'P', 0x00,
    'R', 0x00,
    'S', 0x00,
    'e', 0x00,
    'n', 0x00,
    's', 0x00,
    'o', 0x00,
    'r', 0x00,
    'B', 0x00,
    'o', 0x00,
    'o', 0x00,
    't', 0x00,
    ' ', 0x00,
    'C', 0x00,
    'D', 0x00,
    'C', 0x00
};

uint8_t Cdc_StringSerial[CDC_SIZ_STRING_SERIAL] = {
    CDC_SIZ_STRING_SERIAL,
    USB_STRING_DESCRIPTOR_TYPE,
    '0', 0x00,
    '0', 0x00,
    '0', 0x00,
    '0', 0x00,
    '0', 0x00,
    '0', 0x00,
    '0', 0x00,
    '0', 0x00,
    '0', 0x00,
    '0', 0x00,
    '0', 0x00,
    '0', 0x00
};
