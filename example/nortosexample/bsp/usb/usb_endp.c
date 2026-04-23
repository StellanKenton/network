/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
* File Name          : usb_endp.c
* Author             : MCD Application Team
* Version            : V3.2.1
* Date               : 07/05/2010
* Description        : CDC endpoint callbacks
*******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "usb_lib.h"
#include "usb_istr.h"
#include "bspusb.h"

void EP1_IN_Callback(void)
{
    bspUsbHandleDataIn(BSPUSB_CDC_DATA_IN_EP);
}

void EP2_IN_Callback(void)
{
    bspUsbHandleDataIn(BSPUSB_CDC_CMD_EP);
}

void EP3_OUT_Callback(void)
{
    bspUsbHandleDataOut();
    SetEPRxStatus(ENDP3, EP_RX_VALID);
}
