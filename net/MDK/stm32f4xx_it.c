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
#include "../User/bsp/bsp_rtt.h"
#include "../UCOSIII/uCOS_CONFIG/includes.h"

#include <stdint.h>
 

/** @addtogroup Template_Project
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
typedef struct stFaultStackFrame {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t psr;
} stFaultStackFrame;

/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void faultWriteText(const char *text);
static void faultWriteHex32(uint32_t value);
static void faultWriteLabelHex32(const char *label, uint32_t value);
void hardFaultHandlerC(const stFaultStackFrame *frame, uint32_t excReturn);
/* Private functions ---------------------------------------------------------*/

static void faultWriteText(const char *text)
{
  const char *lpText;
  uint16_t lLength;

  if (text == 0) {
    return;
  }

  lpText = text;
  lLength = 0u;
  while (*lpText != '\0') {
    lpText++;
    lLength++;
  }

  if (lLength == 0u) {
    return;
  }

  (void)bspRttLogWrite((const uint8_t *)text, lLength);
}

static void faultWriteHex32(uint32_t value)
{
  static const char lHexChars[] = "0123456789ABCDEF";
  char lBuffer[10];
  uint32_t lIndex;

  lBuffer[0] = '0';
  lBuffer[1] = 'x';
  for (lIndex = 0u; lIndex < 8u; lIndex++) {
    lBuffer[2u + lIndex] = lHexChars[(value >> (28u - (lIndex * 4u))) & 0x0Fu];
  }

  (void)bspRttLogWrite((const uint8_t *)lBuffer, sizeof(lBuffer));
}

static void faultWriteLabelHex32(const char *label, uint32_t value)
{
  faultWriteText(label);
  faultWriteHex32(value);
  faultWriteText("\r\n");
}

void hardFaultHandlerC(const stFaultStackFrame *frame, uint32_t excReturn)
{
  __disable_irq();
  bspRttLogInit();
  faultWriteText("\r\n[HardFault]\r\n");
  faultWriteLabelHex32("EXC_RETURN=", excReturn);
  faultWriteLabelHex32("PC=", (frame != 0) ? frame->pc : 0u);
  faultWriteLabelHex32("LR=", (frame != 0) ? frame->lr : 0u);
  faultWriteLabelHex32("CFSR=", SCB->CFSR);
  faultWriteLabelHex32("HFSR=", SCB->HFSR);
  faultWriteLabelHex32("BFAR=", SCB->BFAR);
  faultWriteLabelHex32("MMFAR=", SCB->MMFAR);
  faultWriteLabelHex32("MSP=", __get_MSP());
  faultWriteLabelHex32("PSP=", __get_PSP());
  faultWriteLabelHex32("PSR=", (frame != 0) ? frame->psr : 0u);

  while (1) {
  }
}

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
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
__asm void HardFault_Handler(void)
{
  IMPORT  hardFaultHandlerC
  TST     LR, #4
  ITE     EQ
  MRSEQ   R0, MSP
  MRSNE   R0, PSP
  MOV     R1, LR
  B       hardFaultHandlerC
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
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
