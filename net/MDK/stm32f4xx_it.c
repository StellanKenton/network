/**
  ******************************************************************************
  * @file    Project/STM32F4xx_StdPeriph_Templates/stm32f4xx_it.c 
  * @author  MCD Application Team
  * @version V1.4.0
  * @date    04-August-2014
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_it.h"
#include "../User/bsp/bspuart.h"
#include "../UCOSIII/uCOS_CONFIG/includes.h"

#include <stdint.h>
 

/** @addtogroup Template_Project
  * @{
  */

/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
//void PendSV_Handler(void)
//{
//}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
//void SysTick_Handler(void)
//{
// 
//}

/******************************************************************************/
/*                 STM32F4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f4xx.s).                                               */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

void USART2_IRQHandler(void)
{
  OSIntEnter();
  bspUartHandleIrq(DRVUART_WIFI);
  OSIntExit();
}

void DMA1_Stream5_IRQHandler(void)
{
  OSIntEnter();
  bspUartHandleDmaRxIrq(DRVUART_WIFI);
  OSIntExit();
}

void DMA1_Stream6_IRQHandler(void)
{
  OSIntEnter();
  bspUartHandleDmaTxIrq(DRVUART_WIFI);
  OSIntExit();
}

void USART3_IRQHandler(void)
{
  OSIntEnter();
  bspUartHandleIrq(DRVUART_CELLULAR);
  OSIntExit();
}

void DMA1_Stream1_IRQHandler(void)
{
  OSIntEnter();
  bspUartHandleDmaRxIrq(DRVUART_CELLULAR);
  OSIntExit();
}

void DMA1_Stream3_IRQHandler(void)
{
  OSIntEnter();
  bspUartHandleDmaTxIrq(DRVUART_CELLULAR);
  OSIntExit();
}

/**
  * @}
  */ 


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
