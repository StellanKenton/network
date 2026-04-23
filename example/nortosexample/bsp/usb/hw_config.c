/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
* File Name          : hw_config.c
* Author             : MCD Application Team
* Version            : V3.2.1
* Date               : 07/05/2010
* Description        : Hardware configuration for CDC device
*******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
#include "hw_config.h"
#include "bspusb.h"
#include "usb_lib.h"
#include "usb_desc.h"
#include "usb_pwr.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "../../rep/service/log/log.h"

#define USB_HW_LOG_TAG    "UsbHw"

/* Private function prototypes -----------------------------------------------*/
static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len);

void Set_USBClock(void)
{
    RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_1Div5);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);
}

void Enter_LowPowerMode(void)
{
    bDeviceState = SUSPENDED;
}

void Leave_LowPowerMode(void)
{
    DEVICE_INFO *pInfo = &Device_Info;

    if (pInfo->Current_Configuration != 0U) {
        bDeviceState = CONFIGURED;
    }
    else {
        bDeviceState = ATTACHED;
    }
}

void USB_Interrupts_Config(void)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;

    EXTI_ClearITPendingBit(EXTI_Line18);
    EXTI_InitStructure.EXTI_Line = EXTI_Line18;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2U;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0U;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = USBWakeUp_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1U;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0U;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void USB_Cable_Config(FunctionalState NewState)
{
    if (NewState != DISABLE) {
        LOG_I(USB_HW_LOG_TAG, "cable enable");
        GPIO_SetBits(GPIOA, GPIO_Pin_8);
        bspUsbNotifyCableState(true);
    }
    else {
        LOG_I(USB_HW_LOG_TAG, "cable disable");
        GPIO_ResetBits(GPIOA, GPIO_Pin_8);
        bspUsbNotifyCableState(false);
    }
}

void Get_SerialNum(void)
{
    uint32_t Device_Serial0;
    uint32_t Device_Serial1;
    uint32_t Device_Serial2;

    Device_Serial0 = *(__IO uint32_t *)(0x1FFFF7E8);
    Device_Serial1 = *(__IO uint32_t *)(0x1FFFF7EC);
    Device_Serial2 = *(__IO uint32_t *)(0x1FFFF7F0);

    Device_Serial0 += Device_Serial2;

    if (Device_Serial0 != 0U) {
        IntToUnicode(Device_Serial0, &Cdc_StringSerial[2], 8U);
        IntToUnicode(Device_Serial1, &Cdc_StringSerial[18], 4U);
    }
}

static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{
    uint8_t idx;

    for (idx = 0U; idx < len; idx++) {
        pbuf[2U * idx] = ((value >> 28) < 0xAU) ?
                          (uint8_t)((value >> 28) + '0') :
                          (uint8_t)((value >> 28) + 'A' - 10U);
        value = value << 4;
        pbuf[(2U * idx) + 1U] = 0U;
    }
}
