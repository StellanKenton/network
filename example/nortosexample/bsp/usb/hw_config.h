/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
* File Name          : hw_config.h
* Author             : MCD Application Team
* Version            : V3.2.1
* Date               : 07/05/2010
* Description        : Hardware configuration for CDC device
*******************************************************************************/
#ifndef __HW_CONFIG_H
#define __HW_CONFIG_H

#include "stm32f10x.h"

void Set_USBClock(void);
void Enter_LowPowerMode(void);
void Leave_LowPowerMode(void);
void USB_Interrupts_Config(void);
void USB_Cable_Config(FunctionalState NewState);
void Get_SerialNum(void);

#endif  /* __HW_CONFIG_H */
