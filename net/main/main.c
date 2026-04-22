#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "key.h"
#include "lwip_comm.h"
#include "LAN8720.h"
#include "timer.h"
#include "lcd.h"
#include "sram.h"
#include "malloc.h"
#include "lwip_comm.h"
#include "includes.h"
#include "lwipopts.h"


#include "../app/system/systask.h"

#include "ds18b20.h"
#include "sdio_sdcard.h"    
#include "w25qxx.h"    
#include "ff.h"  
#include "exfuns.h"    
#include "fontupd.h"

//ALIENTEK ̽����STM32F407������
//LWIP LWIP+UCOS����ϵͳ��ֲʵ��
//����֧�֣�www.openedv.com
//�������������ӿƼ����޹�˾


static OS_TCB system_task_tcb;
static CPU_STK system_task_stk[SYSTEM_TASK_STK_SIZE];

static void app_task_create(OS_TCB *task_tcb,
							CPU_CHAR *name,
							OS_TASK_PTR task,
							void *arg,
							OS_PRIO prio,
							CPU_STK *stack_base,
							CPU_STK_SIZE stack_size)
{
	OS_ERR err;
	CPU_STK *stack_top;

	(void)task_tcb;
	stack_top = &stack_base[stack_size - 1u];

	err = OSTaskCreateExt(task,
					  arg,
					  stack_top,
					  prio,
					  prio,
					  stack_base,
					  stack_size,
					  NULL,
					  APP_TASK_OPT);
	#if OS_TASK_NAME_EN > 0u
	if (err == OS_ERR_NONE) {
		OSTaskNameSet(prio, (INT8U *)name, &err);
	}
	#endif
	APP_RTOS_ASSERT(err);
}
int main(void)
{
	delay_init(168);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	uart_init(115200);
	mymem_init(SRAMIN);
	CPU_Init();
	OSInit();
	// while (lwip_comm_init() != 0u) {
	// 	delay_ms(100);
	// }
	app_task_create(&system_task_tcb,
					"system_task",
					system_task,
					(void *)0,
					SYSTEM_TASK_PRIO,
					&system_task_stk[0],
					SYSTEM_TASK_STK_SIZE);
	OSStart();

	return 0;
}











