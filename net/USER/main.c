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

#include "cJSON.h"
#include "mqtt_app.h"

#include "text.h"	

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


//��LCD����ʾ��ַ��Ϣ����
//�������ȼ�
#define DISPLAY_TASK_PRIO	8
//�����ջ��С
#define DISPLAY_STK_SIZE	128
//�����ջ
static OS_TCB display_task_tcb;
static CPU_STK DISPLAY_TASK_STK[DISPLAY_STK_SIZE];
//������
void display_task(void *pdata);


//LED����
//�������ȼ�
#define LED_TASK_PRIO		9
//�����ջ��С
#define LED_STK_SIZE		64
//�����ջ
static OS_TCB led_task_tcb;
static CPU_STK LED_TASK_STK[LED_STK_SIZE];
//������
void led_task(void *pdata);  


//START����
//�������ȼ�
#define START_TASK_PRIO		10
//�����ջ��С
#define START_STK_SIZE		128
//�����ջ
static OS_TCB start_task_tcb;
static CPU_STK START_TASK_STK[START_STK_SIZE];
//������
void start_task(void *pdata); 

#define mqtt_task_prio	11
#define mqtt_stk_size   1024
static OS_TCB mqtt_task_tcb;
static CPU_STK mqtt_task_stk[mqtt_stk_size];
void mqtt_task(void *pdata);

#define dht11_task_prio	12
#define dht11_stk_size   128
static OS_TCB dht11_task_tcb;
static CPU_STK dht11_task_stk[dht11_stk_size];
void dht11_task(void *pdata);

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


//��LCD����ʾ��ַ��Ϣ
//mode:1 ��ʾDHCP��ȡ���ĵ�ַ
//	  ���� ��ʾ��̬��ַ
void show_address(u8 mode)
{
	u8 buf[30];
	if(mode==2)
	{
		sprintf((char*)buf,"DHCP IP :%d.%d.%d.%d",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);						//��ӡ��̬IP��ַ
		LCD_ShowString(30,130,210,16,16,buf); 
		sprintf((char*)buf,"DHCP GW :%d.%d.%d.%d",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);	//��ӡ���ص�ַ
		LCD_ShowString(30,150,210,16,16,buf); 
		sprintf((char*)buf,"NET MASK:%d.%d.%d.%d",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);	//��ӡ���������ַ
		LCD_ShowString(30,170,210,16,16,buf); 
	}
	else 
	{
		sprintf((char*)buf,"Static IP:%d.%d.%d.%d",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);						//��ӡ��̬IP��ַ
		LCD_ShowString(30,130,210,16,16,buf); 
		sprintf((char*)buf,"Static GW:%d.%d.%d.%d",lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);	//��ӡ���ص�ַ
		LCD_ShowString(30,150,210,16,16,buf); 
		sprintf((char*)buf,"NET MASK :%d.%d.%d.%d",lwipdev.netmask[0],lwipdev.netmask[1],lwipdev.netmask[2],lwipdev.netmask[3]);	//��ӡ���������ַ
		LCD_ShowString(30,170,210,16,16,buf); 
	}	
}

int main(void)
{
	delay_init(168);       	//��ʱ��ʼ��
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);	//�жϷ�������
	uart_init(115200);    	//���ڲ���������
	LED_Init();  			//LED��ʼ��
	KEY_Init();  			//������ʼ��
	LCD_Init();  			//LCD��ʼ��
	DS18B20_Init();			//�¶ȴ�������ʼ��
	W25QXX_Init();				//��ʼ��W25Q128
	FSMC_SRAM_Init();		//SRAM��ʼ��
	
	mymem_init(SRAMIN);  	//��ʼ���ڲ��ڴ��
	mymem_init(SRAMEX);  	//��ʼ���ⲿ�ڴ��
	mymem_init(SRAMCCM); 	//��ʼ��CCM�ڴ��
	
	exfuns_init();				//Ϊfatfs��ر��������ڴ�  
  	f_mount(fs[0],"0:",1); 		//����SD�� 
 	f_mount(fs[1],"1:",1); 		//����FLASH.
	font_init();
	
	POINT_COLOR = RED; 		//��ɫ����
	LCD_ShowString(30,30,200,20,16,"Explorer STM32F4");
	LCD_ShowString(30,50,200,20,16,"LWIP+UCOS Test");
	LCD_ShowString(30,70,200,20,16,"ATOM@ALIENTEK");
	LCD_ShowString(30,90,200,20,16,"2014/9/1");
//	Show_Str(30,244,200,16,"��Ӧ����(16*16)Ϊ:",16,0);

	CPU_Init();
	OSInit();
	while(lwip_comm_init()) 	//lwip��ʼ��
	{
		LCD_ShowString(30,110,200,20,16,"Lwip Init failed!"); 	//lwip��ʼ��ʧ��
		delay_ms(500);
		LCD_Fill(30,110,230,150,WHITE);
		delay_ms(500);
	}
	LCD_ShowString(30,110,200,20,16,"Lwip Init Success!"); 		//lwip��ʼ���ɹ�
	app_task_create(&start_task_tcb, "start_task", start_task, (void*)0, START_TASK_PRIO, &START_TASK_STK[0], START_STK_SIZE);
	OSStart();
	while (1) {
	}
}

//start����
void start_task(void *pdata)
{
	OS_ERR err;
	pdata = pdata ;
	
	OSStatInit();
#if LWIP_DHCP
	lwip_comm_dhcp_creat(); //����DHCP����
	err = OSTimeDlyHMSM(0,0,2,500);
	APP_RTOS_ASSERT(err);
#endif
	
	app_task_create(&led_task_tcb, "led_task", led_task, (void*)0, LED_TASK_PRIO, &LED_TASK_STK[0], LED_STK_SIZE);
	app_task_create(&display_task_tcb, "display_task", display_task, (void*)0, DISPLAY_TASK_PRIO, &DISPLAY_TASK_STK[0], DISPLAY_STK_SIZE);
	app_task_create(&mqtt_task_tcb, "mqtt_task", mqtt_task, (void*)0, mqtt_task_prio, &mqtt_task_stk[0], mqtt_stk_size);
	app_task_create(&dht11_task_tcb, "dht11_task", dht11_task, (void*)0, dht11_task_prio, &dht11_task_stk[0], dht11_stk_size);
	
	err = OSTaskSuspend(OS_PRIO_SELF);
	APP_RTOS_ASSERT(err);
}

//��ʾ��ַ����Ϣ
void display_task(void *pdata)
{
	OS_ERR err;
	pdata = pdata ;
	while(1)
	{ 
#if LWIP_DHCP									//������DHCP��ʱ��
		if(lwipdev.dhcpstatus != 0) 			//����DHCP
		{
			show_address(lwipdev.dhcpstatus );	//��ʾ��ַ��Ϣ
			err = OSTaskSuspend(OS_PRIO_SELF);
			APP_RTOS_ASSERT(err);
		}
#else
		show_address(0); 						//��ʾ��̬��ַ
		
		err = OSTaskSuspend(OS_PRIO_SELF);
		APP_RTOS_ASSERT(err);
#endif //LWIP_DHCP
		err = OSTimeDlyHMSM(0,0,0,500);
		APP_RTOS_ASSERT(err);
	}
}

//led����
void led_task(void *pdata)
{
	OS_ERR err;
	pdata = pdata ;
	while(1)
	{
		LED0 = !LED0;
		err = OSTimeDlyHMSM(0,0,0,500);  //��ʱ500ms
		APP_RTOS_ASSERT(err);
 	}
}

void mqtt_task(void *pdata)
{
	pdata = pdata ;
	printf("\r\ncJSON Version: %s\r\n", cJSON_Version());
	mqtt_thread();
//	while(1)
//	{
//		OSTimeDlyHMSM(0,0,0,500);  //
//	}
}

void dht11_task(void *pdata)
{
	OS_ERR err;
	pdata = pdata ;
	while(1)
	{
		err = OSTimeDlyHMSM(0,0,0,500);  //
		APP_RTOS_ASSERT(err);
	}
}











