#include "sram.h"
#include "usart.h"
#include "../fmc_fsmc_compat.h"
//////////////////////////////////////////////////////////////////////////////////	 
//������ֻ��ѧϰʹ�ã�δ���������ɣ��������������κ���;
//ALIENTEK STM32F407������
//�ⲿSRAM ��������	   
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//��������:2014/5/14
//�汾��V1.0
//��Ȩ���У�����ؾ���
//Copyright(C) �������������ӿƼ����޹�˾ 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	 


//ʹ��NOR/SRAM�� Bank1.sector3,��ַλHADDR[27,26]=10 
//��IS61LV25616/IS62WV25616,��ַ�߷�ΧΪA0~A17 
//��IS61LV51216/IS62WV51216,��ַ�߷�ΧΪA0~A18

#define Bank1_SRAM3_ADDR		(u32)(0x68000000)

//��ʼ���ⲿSRAM
void FSMC_SRAM_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	FSMC_NORSRAMInitTypeDef FSMC_NORSRAMInitStructure;
	FSMC_NORSRAMTimingInitTypeDef FSMC_ReadWriteTimingStructure;
	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD|RCC_AHB1Periph_GPIOE|RCC_AHB1Periph_GPIOF|RCC_AHB1Periph_GPIOG,ENABLE); //ʹ��PD,PE,PF,PGʱ��
	
	GPIO_InitStructure.GPIO_Pin =( GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_8|GPIO_Pin_9|GPIO_Pin_10 \
																|GPIO_Pin_11|GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15); //PD0,1,4,5,8,9,10,11,12,13,14,15
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF; //����ģʽ
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz; //100Mʱ��
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;  //����ģʽ
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; //����
	GPIO_Init(GPIOD,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = (GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_7|GPIO_Pin_8|GPIO_Pin_9|GPIO_Pin_10|GPIO_Pin_11 \
																|GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15); //PE0,1,7,8,9,10,11,12,13,14,15
	GPIO_Init(GPIOE,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = (GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_12 \
																|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15); //PF0,1,2,3,4,5,12,13,14,15
	GPIO_Init(GPIOF,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_10 ; //PG0,1,2,3,4,5,10
	GPIO_Init(GPIOG,&GPIO_InitStructure);
	
	//GPIOD��������
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource0,GPIO_AF_FSMC);  //PD0
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource1,GPIO_AF_FSMC);  //PD1
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource4,GPIO_AF_FSMC);  //PD4
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource5,GPIO_AF_FSMC);  //PD5
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource8,GPIO_AF_FSMC);  //PD8
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource9,GPIO_AF_FSMC);  //PD9
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource10,GPIO_AF_FSMC); //PD10
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource11,GPIO_AF_FSMC); //PD11
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource12,GPIO_AF_FSMC); //PD12
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource13,GPIO_AF_FSMC); //PD13
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource14,GPIO_AF_FSMC); //PD14
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource15,GPIO_AF_FSMC); //PD15
	
	//GPIOE��������
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource0,GPIO_AF_FSMC);  //PE0
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource1,GPIO_AF_FSMC);  //PE1
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource7,GPIO_AF_FSMC);  //PE7
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource8,GPIO_AF_FSMC);  //PE8
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource9,GPIO_AF_FSMC);  //PE9
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource10,GPIO_AF_FSMC); //PE10
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource11,GPIO_AF_FSMC); //PE11
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource12,GPIO_AF_FSMC); //PE12
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource13,GPIO_AF_FSMC); //PE13
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource14,GPIO_AF_FSMC); //PE14
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource15,GPIO_AF_FSMC); //PE15
	
	//GPIOF��������
	GPIO_PinAFConfig(GPIOF,GPIO_PinSource0,GPIO_AF_FSMC);  //PF0
	GPIO_PinAFConfig(GPIOF,GPIO_PinSource1,GPIO_AF_FSMC);  //PF1
	GPIO_PinAFConfig(GPIOF,GPIO_PinSource2,GPIO_AF_FSMC);  //PF2
	GPIO_PinAFConfig(GPIOF,GPIO_PinSource3,GPIO_AF_FSMC);  //PF3
	GPIO_PinAFConfig(GPIOF,GPIO_PinSource4,GPIO_AF_FSMC);  //PF4
	GPIO_PinAFConfig(GPIOF,GPIO_PinSource5,GPIO_AF_FSMC);  //PF5
	GPIO_PinAFConfig(GPIOF,GPIO_PinSource12,GPIO_AF_FSMC); //PF12
	GPIO_PinAFConfig(GPIOF,GPIO_PinSource13,GPIO_AF_FSMC); //PF13
	GPIO_PinAFConfig(GPIOF,GPIO_PinSource14,GPIO_AF_FSMC); //PF14
	GPIO_PinAFConfig(GPIOF,GPIO_PinSource15,GPIO_AF_FSMC); //PF15
	
	//GPIOG��������
	GPIO_PinAFConfig(GPIOG,GPIO_PinSource0,GPIO_AF_FSMC);  //PG0
	GPIO_PinAFConfig(GPIOG,GPIO_PinSource1,GPIO_AF_FSMC);  //PG1
	GPIO_PinAFConfig(GPIOG,GPIO_PinSource2,GPIO_AF_FSMC);  //PG2
	GPIO_PinAFConfig(GPIOG,GPIO_PinSource3,GPIO_AF_FSMC);  //PG3
	GPIO_PinAFConfig(GPIOG,GPIO_PinSource4,GPIO_AF_FSMC);  //PG4
	GPIO_PinAFConfig(GPIOG,GPIO_PinSource5,GPIO_AF_FSMC);  //PG5
	GPIO_PinAFConfig(GPIOG,GPIO_PinSource10,GPIO_AF_FSMC); //PG10
	
	FSMC_ReadWriteTimingStructure.FSMC_AddressSetupTime = 0X00; //��ַ����ʱ��0��HCLK 0ns
	FSMC_ReadWriteTimingStructure.FSMC_AddressHoldTime = 0x00;  //��ַ����ʱ��,ģʽAδ�õ�
	FSMC_ReadWriteTimingStructure.FSMC_DataSetupTime = 0x09;    //���ݱ���ʱ��,9��HCLK��6*9=54ns
	FSMC_ReadWriteTimingStructure.FSMC_BusTurnAroundDuration = 0x00;
	FSMC_ReadWriteTimingStructure.FSMC_CLKDivision = 0x00;
	FSMC_ReadWriteTimingStructure.FSMC_DataLatency = 0x00;
	FSMC_ReadWriteTimingStructure.FSMC_AccessMode = FSMC_AccessMode_A; //ģʽA
	
	FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM3;  //NOR/SRAM��Bank3
	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable; //���ݺ͵�ַ�߲�����
	FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_SRAM; //SRAM�洢ģʽ
	FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b; //16λ���ݿ���
	FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable; //FLASHʹ�õ�,SRAMδʹ��
	FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable; //�Ƿ�ʹ��ͬ������ģʽ�µĵȴ��ź�,�˴�δ�õ�
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low; //�ȴ��źŵļ���,����ͻ��ģʽ����������
	FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;  //�Ƿ�ʹ�ܻ�·ͻ��ģʽ,�˴�δ�õ�
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState; //�洢�����ڵȴ�����֮ǰ��һ��ʱ�����ڻ��ǵȴ������ڼ�ʹ��NWAIT
	FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable; //�洢��дʹ��
	FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;   //�ȴ�ʹ��λ,�˴�δ�õ�
	FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable; //��дʹ����ͬ��ʱ��
	FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;  //�첽�����ڼ�ĵȴ��ź�
	FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &FSMC_ReadWriteTimingStructure;
	FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &FSMC_ReadWriteTimingStructure;
	FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);  //FSMC_SRAM��ʼ��
	
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM3,ENABLE);  //ʹ��NOR/SRAM����
}

//��ָ����ַ(WriteAddr+Bank1_SRAM3_ADDR)��ʼ,����д��n���ֽ�.
//pBuffer:�ֽ�ָ��
//WriteAddr:Ҫд��ĵ�ַ
//n:Ҫд����ֽ���
void FSMC_SRAM_WriteBuffer(u8 *pBuffer,u32 WriteAddr,u32 n)
{
	for(;n!=0;n--)
	{
		*(vu8*)(Bank1_SRAM3_ADDR + WriteAddr) = *pBuffer; 
		WriteAddr++;
		pBuffer++;
	}
}

//��ָ����ַ((WriteAddr+Bank1_SRAM3_ADDR))��ʼ,��������n���ֽ�.
//pBuffer:�ֽ�ָ��
//ReadAddr:Ҫ��������ʼ��ַ
//n:Ҫд����ֽ���
void FSMC_SRAM_ReadBuffer(u8 *pBuffer,u32 ReadAddr,u32 n)
{
	for(;n!=0;n--)
	{
		*pBuffer ++=*(vu8*)(Bank1_SRAM3_ADDR + ReadAddr);
		ReadAddr++;
	}
}

////////////////////////////////////////////////////////////////////////////////////////
//���Ժ���
//��ָ����ַд��1���ֽ�
//addr:��ַ
//data:Ҫд�������
void fsmc_sram_test_write(u32 addr,u8 data)
{
	FSMC_SRAM_WriteBuffer(&data,addr,1);//д��һ���ֽ�
}

//��ȡ1���ֽ�
//addr:Ҫ��ȡ�ĵ�ַ
//����ֵ:��ȡ��������
u8 fsmc_sram_test_read(u32 addr)
{
	u8 data;
	FSMC_SRAM_ReadBuffer(&data,addr,1);
	return data;
}

