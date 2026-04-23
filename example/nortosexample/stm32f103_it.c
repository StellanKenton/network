/************************************************************************************
 * @file     : stm32f103_it.c
 * @brief    : M600-D interrupt handlers - ported from M600
 * @details  : Cortex fault + DMA1 Ch4/Ch5 (USART1 TX/RX) + DMA1 Ch6/Ch7 (USART2 RX/TX) + USART1/USART2 (IDLE). Std lib.
 ***********************************************************************************/
#include "stm32f103_it.h"
#include <stddef.h>
#include <string.h>
#include "stm32f10x_conf.h"
#include "system/system.h"
#include "bsp/bspuart.h"
#include "bsp/bsprtt.h"
#include "bsp/usb/usb_istr.h"
#include "bsp/usb/usb_pwr.h"

#include "stm32f10x_exti.h"

static void systemFaultTrace(const char *text)
{
    if (text == NULL) {
        return;
    }

    (void)bspRttLogWrite((const uint8_t *)text, (uint16_t)strlen(text));
}
/* -----------------------------------------------------------------------------
 * Cortex-M3 exception handlers
 * ----------------------------------------------------------------------------- */

void NMI_Handler(void)
{
    systemFaultTrace("fault: nmi\r\n");
    while (1) { }
}

void HardFault_Handler(void)
{
   systemFaultTrace("fault: hardfault\r\n");
   while (1) { }
}

void MemManage_Handler(void)
{
    systemFaultTrace("fault: memmanage\r\n");
    while (1) { }
}

void BusFault_Handler(void)
{
    systemFaultTrace("fault: busfault\r\n");
    while (1) { }
}

void UsageFault_Handler(void)
{
    systemFaultTrace("fault: usagefault\r\n");
    while (1) { }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    systemTickHandler();
}

/* -----------------------------------------------------------------------------
 * DMA1 Channel1 (ADC1) - double buffering on TC
 * ----------------------------------------------------------------------------- */
void DMA1_Channel1_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC1) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC1);
        /* Copy DMA working buffer to read buffer (double buffering) */
    }
    if (DMA_GetITStatus(DMA1_IT_TE1) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TE1);
        /* Error handling - optional */
    }
}

/* -----------------------------------------------------------------------------
 * DMA1 Channel4 (USART1 TX) - clear flags on TC
 * ----------------------------------------------------------------------------- */
void DMA1_Channel4_IRQHandler(void)
{
    bspUartHandleTxDmaIrq(DRVUART_UART1);
}

/* -----------------------------------------------------------------------------
 * DMA1 Channel5 (USART1 RX) - clear flags on TC / HT (circular)
 * ----------------------------------------------------------------------------- */
void DMA1_Channel5_IRQHandler(void)
{
    bspUartHandleRxDmaIrq(DRVUART_UART1);
}

void UART4_IRQHandler(void)
{
    bspUartHandleIrq(DRVUART_WIFI);
}

/* -----------------------------------------------------------------------------
 * DMA1 Channel6 (USART2 RX) - clear flags on TC / HT (circular)
 * ----------------------------------------------------------------------------- */
void DMA1_Channel6_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC6) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC6);
        /* Optional: process full buffer */
    }
    if (DMA_GetITStatus(DMA1_IT_HT6) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_HT6);
        /* Optional: process half buffer */
    }
    if (DMA_GetITStatus(DMA1_IT_TE6) != RESET)
        DMA_ClearITPendingBit(DMA1_IT_TE6);
}

/* -----------------------------------------------------------------------------
 * DMA1 Channel7 (USART2 TX) - clear flags on TC
 * ----------------------------------------------------------------------------- */
void DMA1_Channel7_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC7) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC7);
        /* Optional: user callback for TX complete */
    }
    if (DMA_GetITStatus(DMA1_IT_TE7) != RESET)
        DMA_ClearITPendingBit(DMA1_IT_TE7);
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
    USB_Istr();
}

void USBWakeUp_IRQHandler(void)
{
    EXTI_ClearITPendingBit(EXTI_Line18);
    Resume(RESUME_EXTERNAL);
}
