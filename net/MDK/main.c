#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "includes.h"
#include "system/systask.h"
#include "system/system_rtt.h"

static OS_TCB gSystemTaskTcb;
static CPU_STK gSystemTaskStk[SYSTEM_TASK_STK_SIZE];

//主函数
int main(void)
{
	OS_ERR err;

	systick_init();  													//时钟初始化
	LED_Init();
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);						//中断分组配置

	OSInit(&err);
	if (err != OS_ERR_NONE) {
		while (1) {
		}
	}

	OSTaskCreate(	(OS_TCB *)&gSystemTaskTcb,
					(CPU_CHAR *)"SystemTask",
					(OS_TASK_PTR)system_task,
					(void *)0,
					(OS_PRIO)SYSTEM_TASK_PRIO,
					(CPU_STK *)&gSystemTaskStk[0],
					(CPU_STK_SIZE)SYSTEM_TASK_STK_SIZE / 10,
					(CPU_STK_SIZE)SYSTEM_TASK_STK_SIZE,
					(OS_MSG_QTY)0,
					(OS_TICK)0,
					(void *)0,
					(OS_OPT)OS_OPT_TASK_NONE,
					&err);
	if (err != OS_ERR_NONE) {
		while (1) {
		}
	}

	//启动OS，进行任务调度
	OSStart(&err);
	while (1) {
	}
}







