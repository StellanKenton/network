/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
* File Name          : usb_pwr.h
* Author             : MCD Application Team
* Version            : V3.2.1
* Date               : 07/05/2010
* Description        : Connection/disconnection and power management header
*******************************************************************************/
#ifndef __USB_PWR_H
#define __USB_PWR_H

#include <stdbool.h>

#include "usb_core.h"

typedef enum _RESUME_STATE {
    RESUME_EXTERNAL,
    RESUME_INTERNAL,
    RESUME_LATER,
    RESUME_WAIT,
    RESUME_START,
    RESUME_ON,
    RESUME_OFF,
    RESUME_ESOF
} RESUME_STATE;

typedef enum _DEVICE_STATE {
    UNCONNECTED,
    ATTACHED,
    POWERED,
    SUSPENDED,
    ADDRESSED,
    CONFIGURED
} DEVICE_STATE;

void Suspend(void);
void Resume_Init(void);
void Resume(RESUME_STATE eResumeSetVal);
void PowerResetResumeState(void);
RESULT PowerOn(void);
RESULT PowerOff(void);

extern __IO uint32_t bDeviceState;
extern __IO bool fSuspendEnabled;

#endif  /* __USB_PWR_H */
