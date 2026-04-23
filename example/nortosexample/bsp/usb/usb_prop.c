/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
* File Name          : usb_prop.c
* Author             : MCD Application Team
* Version            : V3.2.1
* Date               : 07/05/2010
* Description        : CDC ACM request processing
*******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
#include "usb_lib.h"
#include "usb_conf.h"
#include "usb_prop.h"
#include "usb_desc.h"
#include "usb_pwr.h"
#include "hw_config.h"
#include "bspusb.h"

/* Private variables ---------------------------------------------------------*/
static uint8_t gCdcRequest = 0U;
static LINE_CODING gCdcLineCoding = {
    115200U,
    0x00U,
    0x00U,
    0x08U,
};
static uint16_t gCdcAbstractState = 0U;
static uint16_t gCdcCountrySetting = 0U;

static uint8_t *usbGetCommFeatureStorage(void)
{
    switch (pInformation->USBwValue) {
        case CDC_FEATURE_ABSTRACT_STATE:
            return (uint8_t *)&gCdcAbstractState;
        case CDC_FEATURE_COUNTRY_SETTING:
            return (uint8_t *)&gCdcCountrySetting;
        default:
            return NULL;
    }
}

DEVICE Device_Table = {
    EP_NUM,
    1
};

DEVICE_PROP Device_Property = {
    Cdc_Init,
    Cdc_Reset,
    Cdc_Status_In,
    Cdc_Status_Out,
    Cdc_Data_Setup,
    Cdc_NoData_Setup,
    Cdc_Get_Interface_Setting,
    Cdc_GetDeviceDescriptor,
    Cdc_GetConfigDescriptor,
    Cdc_GetStringDescriptor,
    0,
    0x40
};

USER_STANDARD_REQUESTS User_Standard_Requests = {
    Cdc_GetConfiguration,
    Cdc_SetConfiguration,
    Cdc_GetInterface,
    Cdc_SetInterface,
    Cdc_GetStatus,
    Cdc_ClearFeature,
    Cdc_SetEndPointFeature,
    Cdc_SetDeviceFeature,
    Cdc_SetDeviceAddress
};

ONE_DESCRIPTOR Device_Descriptor = {
    (uint8_t *)Cdc_DeviceDescriptor,
    CDC_SIZ_DEVICE_DESC
};

ONE_DESCRIPTOR Config_Descriptor = {
    (uint8_t *)Cdc_ConfigDescriptor,
    CDC_SIZ_CONFIG_DESC
};

ONE_DESCRIPTOR String_Descriptor[4] = {
    {(uint8_t *)Cdc_StringLangID, CDC_SIZ_STRING_LANGID},
    {(uint8_t *)Cdc_StringVendor, CDC_SIZ_STRING_VENDOR},
    {(uint8_t *)Cdc_StringProduct, CDC_SIZ_STRING_PRODUCT},
    {(uint8_t *)Cdc_StringSerial, CDC_SIZ_STRING_SERIAL},
};

void Cdc_Init(void)
{
    Get_SerialNum();

    pInformation->Current_Configuration = 0;
    gCdcRequest = 0U;
    gCdcAbstractState = 0U;
    gCdcCountrySetting = 0U;
    (void)PowerOn();
    USB_SIL_Init();
    bDeviceState = UNCONNECTED;
}

void Cdc_Reset(void)
{
    pInformation->Current_Configuration = 0;
    pInformation->Current_Interface = 0;
    pInformation->Current_Feature = Cdc_ConfigDescriptor[7];
    gCdcRequest = 0U;
    gCdcAbstractState = 0U;
    gCdcCountrySetting = 0U;
    PowerResetResumeState();

    SetBTABLE(BTABLE_ADDRESS);

    SetEPType(ENDP0, EP_CONTROL);
    SetEPTxStatus(ENDP0, EP_TX_STALL);
    SetEPRxAddr(ENDP0, ENDP0_RXADDR);
    SetEPTxAddr(ENDP0, ENDP0_TXADDR);
    Clear_Status_Out(ENDP0);
    SetEPRxCount(ENDP0, Device_Property.MaxPacketSize);
    SetEPRxValid(ENDP0);

    SetEPType(ENDP1, EP_BULK);
    SetEPTxAddr(ENDP1, ENDP1_TXADDR);
    SetEPTxStatus(ENDP1, EP_TX_NAK);
    SetEPRxStatus(ENDP1, EP_RX_DIS);

    SetEPType(ENDP2, EP_INTERRUPT);
    SetEPTxAddr(ENDP2, ENDP2_TXADDR);
    SetEPRxStatus(ENDP2, EP_RX_DIS);
    SetEPTxStatus(ENDP2, EP_TX_NAK);

    SetEPType(ENDP3, EP_BULK);
    SetEPRxAddr(ENDP3, ENDP3_RXADDR);
    SetEPRxCount(ENDP3, BSPUSB_CDC_DATA_MAX_PACKET_SIZE);
    SetEPRxStatus(ENDP3, EP_RX_VALID);
    SetEPTxStatus(ENDP3, EP_TX_DIS);

    SetDeviceAddress(0);
    bspUsbResetTransferState();
    bDeviceState = ATTACHED;
}

void Cdc_SetConfiguration(void)
{
    if (pInformation->Current_Configuration != 0U) {
        bDeviceState = CONFIGURED;
        bspUsbNotifyConfigured(true);
    }
}

void Cdc_SetDeviceAddress(void)
{
    bspUsbNotifyConfigured(false);
    bDeviceState = ADDRESSED;
}

void Cdc_Status_In(void)
{
    if (gCdcRequest == SET_LINE_CODING) {
        gCdcRequest = 0U;
    }
}

void Cdc_Status_Out(void)
{
}

RESULT Cdc_Data_Setup(uint8_t RequestNo)
{
    uint8_t *(*CopyRoutine)(uint16_t Length) = NULL;

    if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT)) {
        if (RequestNo == GET_LINE_CODING) {
            CopyRoutine = Cdc_GetLineCoding;
        }
        else if ((RequestNo == SET_LINE_CODING) &&
                 (pInformation->USBwLength == CDC_LINE_CODING_SIZE)) {
            CopyRoutine = Cdc_SetLineCoding;
            gCdcRequest = SET_LINE_CODING;
        }
        else if ((RequestNo == GET_COMM_FEATURE) &&
                 (pInformation->USBwLength == sizeof(uint16_t)) &&
                 (usbGetCommFeatureStorage() != NULL)) {
            CopyRoutine = Cdc_GetCommFeature;
        }
        else if ((RequestNo == SET_COMM_FEATURE) &&
                 (pInformation->USBwLength == sizeof(uint16_t)) &&
                 (usbGetCommFeatureStorage() != NULL)) {
            CopyRoutine = Cdc_SetCommFeature;
        }
    }

    if (CopyRoutine == NULL) {
        return USB_UNSUPPORT;
    }

    pInformation->Ctrl_Info.CopyData = CopyRoutine;
    pInformation->Ctrl_Info.Usb_wOffset = 0U;
    (void)(*CopyRoutine)(0U);
    return USB_SUCCESS;
}

RESULT Cdc_NoData_Setup(uint8_t RequestNo)
{
    if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT)) {
        if (RequestNo == SET_CONTROL_LINE_STATE) {
            return USB_SUCCESS;
        }
        if (RequestNo == SEND_BREAK) {
            return USB_SUCCESS;
        }
        if (RequestNo == CLEAR_COMM_FEATURE) {
            uint8_t *lFeature = usbGetCommFeatureStorage();

            if (lFeature == NULL) {
                return USB_UNSUPPORT;
            }

            ((uint16_t *)lFeature)[0] = 0U;
            return USB_SUCCESS;
        }
    }

    return USB_UNSUPPORT;
}

uint8_t *Cdc_GetDeviceDescriptor(uint16_t Length)
{
    return Standard_GetDescriptorData(Length, &Device_Descriptor);
}

uint8_t *Cdc_GetConfigDescriptor(uint16_t Length)
{
    return Standard_GetDescriptorData(Length, &Config_Descriptor);
}

uint8_t *Cdc_GetStringDescriptor(uint16_t Length)
{
    uint8_t wValue0 = pInformation->USBwValue0;

    if (wValue0 > 3U) {
        return NULL;
    }

    return Standard_GetDescriptorData(Length, &String_Descriptor[wValue0]);
}

RESULT Cdc_Get_Interface_Setting(uint8_t Interface, uint8_t AlternateSetting)
{
    if (AlternateSetting > 0U) {
        return USB_UNSUPPORT;
    }

    if (Interface > 1U) {
        return USB_UNSUPPORT;
    }

    return USB_SUCCESS;
}

uint8_t *Cdc_GetCommFeature(uint16_t Length)
{
    uint8_t *lFeature = usbGetCommFeatureStorage();

    if (lFeature == NULL) {
        return NULL;
    }

    if (Length == 0U) {
        pInformation->Ctrl_Info.Usb_wLength = sizeof(uint16_t);
        return NULL;
    }

    return lFeature;
}

uint8_t *Cdc_SetCommFeature(uint16_t Length)
{
    uint8_t *lFeature = usbGetCommFeatureStorage();

    if (lFeature == NULL) {
        return NULL;
    }

    if (Length == 0U) {
        pInformation->Ctrl_Info.Usb_wLength = sizeof(uint16_t);
        return NULL;
    }

    return lFeature;
}

uint8_t *Cdc_GetLineCoding(uint16_t Length)
{
    if (Length == 0U) {
        pInformation->Ctrl_Info.Usb_wLength = CDC_LINE_CODING_SIZE;
        return NULL;
    }

    return (uint8_t *)&gCdcLineCoding;
}



uint8_t *Cdc_SetLineCoding(uint16_t Length)
{
    if (Length == 0U) {
        pInformation->Ctrl_Info.Usb_wLength = CDC_LINE_CODING_SIZE;
        return NULL;
    }

    return (uint8_t *)&gCdcLineCoding;
}
