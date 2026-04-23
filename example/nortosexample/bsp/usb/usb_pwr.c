/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
* File Name          : usb_pwr.c
* Author             : MCD Application Team
* Version            : V3.2.1
* Date               : 07/05/2010
* Description        : Connection/disconnection and power management
*******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
#include "usb_lib.h"
#include "usb_conf.h"
#include "usb_pwr.h"
#include "hw_config.h"

__IO uint32_t bDeviceState = UNCONNECTED;
__IO bool fSuspendEnabled = true;

static struct {
    __IO RESUME_STATE eState;
    __IO uint8_t bESOFcnt;
} ResumeS;

RESULT PowerOn(void)
{
#ifndef STM32F10X_CL
    volatile uint32_t lStartupDelay;

    _SetCNTR(CNTR_FRES);

    for (lStartupDelay = 0U; lStartupDelay < 72U; lStartupDelay++) {
    }

    wInterrupt_Mask = 0U;
    _SetCNTR(wInterrupt_Mask);
    _SetISTR(0U);
    wInterrupt_Mask = IMR_MSK;
    _SetCNTR(wInterrupt_Mask);
#endif

    return USB_SUCCESS;
}

RESULT PowerOff(void)
{
#ifndef STM32F10X_CL
    _SetCNTR(CNTR_FRES);
    _SetISTR(0U);
    USB_Cable_Config(DISABLE);
    _SetCNTR(CNTR_FRES | CNTR_PDWN);
#endif

    return USB_SUCCESS;
}

void Suspend(void)
{
#ifndef STM32F10X_CL
    uint16_t wCNTR;

    wCNTR = _GetCNTR();
    wCNTR |= CNTR_FSUSP;
    _SetCNTR(wCNTR);

    wCNTR = _GetCNTR();
    wCNTR |= CNTR_LPMODE;
    _SetCNTR(wCNTR);
#endif

    Enter_LowPowerMode();
}

void Resume_Init(void)
{
#ifndef STM32F10X_CL
    uint16_t wCNTR;

    wCNTR = _GetCNTR();
    wCNTR &= (uint16_t)(~CNTR_LPMODE);
    _SetCNTR(wCNTR);
#endif

    Leave_LowPowerMode();

#ifndef STM32F10X_CL
    _SetCNTR(IMR_MSK);
#endif
}

void Resume(RESUME_STATE eResumeSetVal)
{
#ifndef STM32F10X_CL
    uint16_t wCNTR;
#endif

    if (eResumeSetVal != RESUME_ESOF) {
        ResumeS.eState = eResumeSetVal;
    }

    switch (ResumeS.eState) {
        case RESUME_EXTERNAL:
            Resume_Init();
            ResumeS.eState = RESUME_OFF;
            break;
        case RESUME_INTERNAL:
            Resume_Init();
            ResumeS.eState = RESUME_START;
            break;
        case RESUME_LATER:
            ResumeS.bESOFcnt = 2U;
            ResumeS.eState = RESUME_WAIT;
            break;
        case RESUME_WAIT:
            ResumeS.bESOFcnt--;
            if (ResumeS.bESOFcnt == 0U) {
                ResumeS.eState = RESUME_START;
            }
            break;
        case RESUME_START:
#ifndef STM32F10X_CL
            wCNTR = _GetCNTR();
            wCNTR |= CNTR_RESUME;
            _SetCNTR(wCNTR);
#endif
            ResumeS.eState = RESUME_ON;
            ResumeS.bESOFcnt = 10U;
            break;
        case RESUME_ON:
#ifndef STM32F10X_CL
            ResumeS.bESOFcnt--;
            if (ResumeS.bESOFcnt == 0U) {
                wCNTR = _GetCNTR();
                wCNTR &= (uint16_t)(~CNTR_RESUME);
                _SetCNTR(wCNTR);
                ResumeS.eState = RESUME_OFF;
            }
#endif
            break;
        case RESUME_OFF:
        case RESUME_ESOF:
        default:
            ResumeS.eState = RESUME_OFF;
            break;
    }
}

void PowerResetResumeState(void)
{
#ifndef STM32F10X_CL
    _SetCNTR(IMR_MSK);
#endif

    ResumeS.eState = RESUME_OFF;
    ResumeS.bESOFcnt = 0U;
    Leave_LowPowerMode();
}
