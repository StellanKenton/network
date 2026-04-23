/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
* File Name          : usb_istr.c
* Author             : MCD Application Team
* Version            : V3.2.1
* Date               : 07/05/2010
* Description        : USB interrupt service routines
*******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "usb_lib.h"
#include "usb_prop.h"
#include "usb_pwr.h"
#include "usb_istr.h"

__IO uint16_t wIstr;
__IO uint8_t bIntPackSOF;

void usbDbgTraceEp0SetupEntry(uint16_t istr, uint16_t epValue)
{
    (void)istr;
    (void)epValue;
}

void usbDbgTraceSetupPacketFields(uint8_t requestType, uint8_t request, uint16_t value, uint16_t index, uint16_t length)
{
    (void)requestType;
    (void)request;
    (void)value;
    (void)index;
    (void)length;
}

void (*pEpInt_IN[7])(void) = {
    EP1_IN_Callback,
    EP2_IN_Callback,
    EP3_IN_Callback,
    EP4_IN_Callback,
    EP5_IN_Callback,
    EP6_IN_Callback,
    EP7_IN_Callback,
};

void (*pEpInt_OUT[7])(void) = {
    EP1_OUT_Callback,
    EP2_OUT_Callback,
    EP3_OUT_Callback,
    EP4_OUT_Callback,
    EP5_OUT_Callback,
    EP6_OUT_Callback,
    EP7_OUT_Callback,
};

void USB_Istr(void)
{
    wIstr = _GetISTR();

    if ((wIstr & ISTR_CTR & wInterrupt_Mask) != 0U) {
        CTR_LP();
    }

    if ((wIstr & ISTR_RESET & wInterrupt_Mask) != 0U) {
        _SetISTR((uint16_t)CLR_RESET);
        Device_Property.Reset();
    }

    if ((wIstr & ISTR_DOVR & wInterrupt_Mask) != 0U) {
        _SetISTR((uint16_t)CLR_DOVR);
    }

    if ((wIstr & ISTR_ERR & wInterrupt_Mask) != 0U) {
        _SetISTR((uint16_t)CLR_ERR);
    }

    if ((wIstr & ISTR_WKUP & wInterrupt_Mask) != 0U) {
        _SetISTR((uint16_t)CLR_WKUP);
        Resume(RESUME_EXTERNAL);
    }

    if ((wIstr & ISTR_SUSP & wInterrupt_Mask) != 0U) {
        if (fSuspendEnabled) {
            Suspend();
        }
        else {
            Resume(RESUME_LATER);
        }
        _SetISTR((uint16_t)CLR_SUSP);
    }

    if ((wIstr & ISTR_SOF & wInterrupt_Mask) != 0U) {
        _SetISTR((uint16_t)CLR_SOF);
        bIntPackSOF++;
    }

    if ((wIstr & ISTR_ESOF & wInterrupt_Mask) != 0U) {
        _SetISTR((uint16_t)CLR_ESOF);
        Resume(RESUME_ESOF);
    }
}
