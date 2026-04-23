/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
* File Name          : usb_prop.h
* Author             : MCD Application Team
* Version            : V3.2.1
* Date               : 07/05/2010
* Description        : CDC ACM request processing
*******************************************************************************/
#ifndef __USB_PROP_H
#define __USB_PROP_H

#include "usb_core.h"

typedef struct stLineCoding {
    uint32_t bitrate;
    uint8_t format;
    uint8_t paritytype;
    uint8_t datatype;
} LINE_CODING;

#define CDC_LINE_CODING_SIZE                7U

typedef enum _CDC_REQUESTS {
    SEND_ENCAPSULATED_COMMAND = 0x00,
    GET_ENCAPSULATED_RESPONSE = 0x01,
    SET_COMM_FEATURE = 0x02,
    GET_COMM_FEATURE = 0x03,
    CLEAR_COMM_FEATURE = 0x04,
    SET_LINE_CODING = 0x20,
    GET_LINE_CODING = 0x21,
    SET_CONTROL_LINE_STATE = 0x22,
    SEND_BREAK = 0x23,
} CDC_REQUESTS;

#define CDC_FEATURE_ABSTRACT_STATE          0x0001U
#define CDC_FEATURE_COUNTRY_SETTING         0x0002U

void Cdc_Init(void);
void Cdc_Reset(void);
void Cdc_SetConfiguration(void);
void Cdc_SetDeviceAddress(void);
void Cdc_Status_In(void);
void Cdc_Status_Out(void);
RESULT Cdc_Data_Setup(uint8_t RequestNo);
RESULT Cdc_NoData_Setup(uint8_t RequestNo);
RESULT Cdc_Get_Interface_Setting(uint8_t Interface, uint8_t AlternateSetting);
uint8_t *Cdc_GetDeviceDescriptor(uint16_t Length);
uint8_t *Cdc_GetConfigDescriptor(uint16_t Length);
uint8_t *Cdc_GetStringDescriptor(uint16_t Length);
uint8_t *Cdc_GetCommFeature(uint16_t Length);
uint8_t *Cdc_SetCommFeature(uint16_t Length);
uint8_t *Cdc_GetLineCoding(uint16_t Length);
uint8_t *Cdc_SetLineCoding(uint16_t Length);

#define Cdc_GetConfiguration        NOP_Process
#define Cdc_GetInterface            NOP_Process
#define Cdc_SetInterface            NOP_Process
#define Cdc_GetStatus               NOP_Process
#define Cdc_ClearFeature            NOP_Process
#define Cdc_SetEndPointFeature      NOP_Process
#define Cdc_SetDeviceFeature        NOP_Process

#endif /* __USB_PROP_H */
