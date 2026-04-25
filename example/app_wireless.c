/** 
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_Wireless.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#include "app_Wireless.h"
#include "main.h"
#include <string.h>
#include "app_memory.h"
#include "app_flash.h"
#include "encryption.h"
#include "lib_rtc.h"
#include "stm32f1xx_hal_uart.h"
#include "MD5.h"
#include <string.h>
#include <stdio.h>
#include "semphr.h"
#include "app_system.h"
#include "app_i2c.h"
#include "SEGGER_RTT.h"

#define RING_BLE_BUFFSIZE 1024
#define RX_BUFFER_SIZE 512

static LogUpdate_Typedef Log_Status;
static SelfCheck_Typedef Self_Check_Data;
UART_HandleTypeDef *WirelessComm_Uart;
Comm_Priority_EnumDef Comm_Priority = BLE_PRIORITY;
uint8_t g_WirelessModlue_Self_Check = 0;
uint8_t CPR_Metronome_Freq_Recv;
volatile char UTC_Offset_Time[6];
uint64_t UTC_Offset_Time_Trans = 0;
extern Memory_SendState_TypeDef Memory_SendState;

extern osMutexId_t Time_SyncHandle;
extern Factory_TypeDef Factory_Info;
extern Audio_Language_EnumDef Audio_Language_Rev;
extern Power_Voltage_TypeDef g_PowerVoltage;
extern uint8_t global_HardVer_Flag;
extern uint8_t Audio_Volume_Rev;
extern osMutexId_t Power_MutexHandle;
extern osMutexId_t Time_SyncHandle;
extern osMessageQueueId_t CPR_Data_SendHandle;
extern osMessageQueueId_t CPR_Time_SendHandle;
extern osMessageQueueId_t History_StatusHandle;
extern osMessageQueueId_t Boot_Time_SendHandleHandle;
extern AESInfo_t aesInfo;
extern osSemaphoreId_t SaveSecretSemHandle;
extern PowerDown_State_StructDef Power_State;
extern osMessageQueueId_t Boot_Time_SendHandleHandle;
extern osMessageQueueId_t Log_SaveHandle;
extern osMessageQueueId_t Log_SendHandle;
extern osMessageQueueId_t Self_Check_SaveHandle;
extern osMessageQueueId_t Self_Check_SendHandle;
extern uint8_t Audio_Change_Language_Flag;

Wifi_Module_Typedef Wifi_Module = {
    .SSID = "default",
    .PWD  = "default",
    .IP   = "",
    .MAC  = {0},
    .RSSI = 0,
    .Port = "",
};

CBuff Ring_WireLessComm;
static uint8_t Ring_WireLessCommBuff[RING_BLE_BUFFSIZE];
static uint8_t Ring_WireLessCommRecv[256];

static uint8_t BleDMARx_Buffer[RX_BUFFER_SIZE];

CBuff Ring_WireLessSendBuffer;
static uint8_t Ring_WireLessSendBuff[RING_BLE_BUFFSIZE];
static uint8_t Ring_WireLessSendRecv[256];
static uint8_t Send_Rev_Ok_Flag;

static uint8_t MAC_ADRESS[16];
static uint8_t BLE_VER[33];
static uint8_t Encrypt_Data[128];
static uint8_t Decrypt_Data[128];
static volatile uint8_t MD5_MAC_KEY[17] = {0};
uint32_t Recv_World_Time_Tick;

static WireLess_Recv_EnumDef s_Wireless_Data_Recv_Flag;
static BLE_FSM_EnumDef s_BLE_State = BLE_INIT_STATE;
static WIFI_FSM_EnumDef s_Wifi_State = WIFI_INIT_STATE;
static Ble_Send_ByteUnion  Wireless_SendFlag;
static Ble_State_ByteUnion BLE_StateFlag;
static WIRELESS_WORK_EnumDef WireLess_Work_State = WIFI_WORK_STATE;

static uint8_t sSendBuffer[WIRELESS_MAX_LEN] = {0};
static uint8_t su8_DataBuffer[49] = {0};
static uint8_t su8_AT_Command[WIRELESS_MAX_LEN]= {0};
static CPRFeedbck_Data_Typedef CPR_Data;
static CPRFeedbck_time_Typedef CPR_Time;

NetCfg_t g_NetCfg = {
    .serverName = "iot-test2.yuwell.com",
	//.serverName = "101.37.175.138",
    .serverPort = 8800,
    .mqttServer = "iot-test2.yuwell.com",
	//.mqttServer = "101.37.26.133",
    .mqttPort = 1883,
    .secret = "YuWell@CPR",
};
static char sMqttUserName[100] = {0};
static char sMqttPassword[100] = {0};
char Device_Secret[18] = {0};
//char Device_Secret_static[17] = "SQAQSVCPE4XTCWZ3";
char productId[18] = {0};

static void Wireless_Send_Frame(uint8_t cmd, const uint8_t *payload, uint16_t payload_len);
/*!
* \brief 串口接收DMA中断处理
* \param   none
* \return none
*/
void WirelessComm_DMA_Recive(void)
{
    if(__HAL_UART_GET_FLAG(WirelessComm_Uart, UART_FLAG_IDLE)) 
	{
		__HAL_UART_CLEAR_IDLEFLAG(WirelessComm_Uart); // 清除IDLE标志
		HAL_UART_DMAStop(WirelessComm_Uart);          // 停止DMA（防止数据被覆盖）
		uint16_t rxLen = RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(WirelessComm_Uart->hdmarx);
        CBuff_Write(&Ring_WireLessComm,BleDMARx_Buffer,rxLen);
		HAL_UART_Receive_DMA(WirelessComm_Uart, BleDMARx_Buffer, RX_BUFFER_SIZE); // 重启接收
	 }
}

DMA_TX_Status Get_DMA_TX_Status(UART_HandleTypeDef *huart)
{
    if (huart->gState == HAL_UART_STATE_READY) {
        return DMA_TX_IDLE;
    } else if (huart->gState == HAL_UART_STATE_BUSY_TX) {
        return DMA_TX_BUSY;
    } else if (huart->ErrorCode != HAL_UART_ERROR_NONE) {
        return DMA_TX_ERROR;
    }
    return DMA_TX_BUSY;
}

/*!
* \brief 无线模块GPIO初始化
* \param   none
* \return none
*/
void Wireless_Gpio_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /*Configure GPIO pin : */
    GPIO_InitStruct.Pin = RESET_WIFI_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/*!
* \brief 无线模块复位使能
* \param   none
* \return none
*/
void Wireless_Reset_Enable(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /*Configure GPIO pin : */
    GPIO_InitStruct.Pin = RESET_WIFI_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, RESET_WIFI_Pin, GPIO_PIN_RESET);
}

/*!
* \brief 无线模块复位失能
* \param   none
* \return none
*/
void Wireless_Reset_Disable(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /*Configure GPIO pin : */
    GPIO_InitStruct.Pin = RESET_WIFI_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/*!
* \brief 无线模块串口发送
* \param   pData：发送数据指针，
*          Size: 发送数据长度
* \return none
*/
void Wireless_Uart_Send(const uint8_t *pData, uint16_t Size)
{
    HAL_UART_Transmit_DMA(WirelessComm_Uart, pData, Size);
}

/*!
* \brief 无线模块环形缓冲区发送
* \param   pData：发送数据指针，
*          Size: 发送数据长度
* \return none
*/
void Wireless_Ring_Send(const uint8_t *pData, uint16_t Size)
{
    CBuff_Write(&Ring_WireLessSendBuffer,(uint8_t *)&Size,2);   // 发送字节长度
    CBuff_Write(&Ring_WireLessSendBuffer,pData,Size); // 发送数据
    //HAL_UART_Transmit_DMA(WirelessComm_Uart, pData, Size);
}

/*!
* \brief 无线模块串口通过环形缓冲区的方式发送数据
* \param  none
* \return none
*/
void BLE_Ring_Send_Handle()
{
    static Ble_Send_State_enum Ble_Send_State = Ble_Handle_Data;
    static uint8_t Err_Cnt;
    switch(Ble_Send_State)
    {
        case Ble_Send_Waiting:			 
            if(Send_Rev_Ok_Flag == 1){
                Ble_Send_State = Ble_Handle_Data; 
            }
            else if(Send_Rev_Ok_Flag == 2) {
                Ble_Send_State = Ble_DMA_Send;                
                Err_Cnt++;
            }
            else {
                Err_Cnt++;
				
            }    
            if(Err_Cnt*WIRELESS_TASK_DELAY >= 500){
                //Report Err
                Ble_Send_State = Ble_Handle_Data;
            }
			break;
        case Ble_Handle_Data:
            if(CBuff_GetLength(&Ring_WireLessSendBuffer) >= 8 && Get_DMA_TX_Status(WirelessComm_Uart) == DMA_TX_IDLE) {
                CBuff_Pop(&Ring_WireLessSendBuffer,(uint8_t*)&Ring_WireLessSendBuffer.HandleDataLength,2);
                CBuff_Pop(&Ring_WireLessSendBuffer,Ring_WireLessSendBuffer.RevData,Ring_WireLessSendBuffer.HandleDataLength);               
                Ble_Send_State = Ble_DMA_Send;
            } 
			else {
				break;
			}		
        case Ble_DMA_Send:	
			Send_Rev_Ok_Flag = 0;
            
            if(HAL_UART_Transmit_DMA(WirelessComm_Uart, Ring_WireLessSendBuffer.RevData, Ring_WireLessSendBuffer.HandleDataLength) == HAL_OK){               
				Err_Cnt = 0;
                Ble_Send_State = Ble_Send_Waiting;            
            } 
            else {
                Err_Cnt++;
            }  
			if(Err_Cnt*WIRELESS_TASK_DELAY >= 500){
                //Report Err
                Ble_Send_State = Ble_Handle_Data;
            }
            break;
        default:
            break;
    }

}
/*!
* \brief 无线模块发送AT命令
* \param   cmd：AT命令字符串指针
* \return none
*/
void App_Wireless_Send(char *cmd)
{
    uint16_t Send_Len;
    Send_Len = strlen(cmd);
    Wireless_Uart_Send((uint8_t *)cmd, Send_Len);
}

/*!
* \brief 无线模块AT命令检测
* \param   Ring_Comm：接收环形缓冲区指针，
*          cmd：AT命令字符串指针，
*          wait_time: 等待响应时间，单位ms
*          out: 期望的响应类型
* \return 响应结果枚举
*/
WireLess_Recv_EnumDef WireLess_Modlue_AT_Check(CBuff *Ring_Comm,char *cmd, uint16_t wait_time,WireLess_Recv_EnumDef out)
{
    WireLess_Recv_EnumDef ret = wRecv_None;
    uint16_t Send_Len, Recv_Len;
    uint16_t Wait_Cnt = wait_time/5;
    
    Send_Len = strlen((char *)cmd);
    CBuff_Clear(Ring_Comm);
    for(uint8_t j = 0; j < 3; j++) {
        Wireless_Uart_Send((uint8_t *)cmd, Send_Len);     
        for(uint8_t i = 0; i < Wait_Cnt; i++)   // 10ms
        {
            osDelay(5);
            Recv_Len = (uint16_t)CBuff_GetLength(Ring_Comm);
            if(Recv_Len >= 4) {				
                for(uint16_t j = 0; j < Recv_Len - 3; j++) {
					osDelay(5);
					Recv_Len = (uint16_t)CBuff_GetLength(Ring_Comm);
                    CBuff_Read(Ring_Comm, Ring_Comm->RevData, Recv_Len);
                    if(((Ring_Comm->RevData[0] == '+')&&(Ring_Comm->RevData[1] == 'Q'))
                            ||((Ring_Comm->RevData[0] == 'O')&&(Ring_Comm->RevData[1] == 'K'))
                            ||((Ring_Comm->RevData[0] == 'E')&&(Ring_Comm->RevData[1] == 'R'))
                            ||((Ring_Comm->RevData[0] == 'C')&&(Ring_Comm->RevData[1] == 'O')))
                    {                  
                        for(int i = 0; i <= Recv_Len; i++) {
                            if(Ring_Comm->RevData[i] == BLE_CR) {
                                if(Ring_Comm->RevData[i+1] == BLE_LF) {
                                    CBuff_Pop(Ring_Comm,Ring_Comm->RevData,i+2);
                                    Ring_Comm->RevData[i] = 0x00;
                                    if(Ring_Comm->RevData[0] != '+') {
                                        ret = wRecv_Char;
                                        
                                    }else{
                                        ret = wRecv_Puls;
                                    }
                                    if(out == ret) {
										return ret;
                                    }
                                    else {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    else {
                        CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);	
                    }
                }
            }
        }
    }
	return ret;
}

/*!
* \brief 无线模块初始化
* \param   none
* \return none
*/
void App_Module_Reset()
{
    CBuff_Init(&Ring_WireLessComm,Ring_WireLessCommBuff,Ring_WireLessCommRecv,RING_BLE_BUFFSIZE);
    CBuff_Init(&Ring_WireLessSendBuffer,Ring_WireLessSendBuff,Ring_WireLessSendRecv,RING_BLE_BUFFSIZE);
    Wireless_Gpio_Init();
    Wireless_Reset_Enable();
    osDelay(100);  // 等待无线模块复位
    Wireless_Reset_Disable();
    osDelay(1000);
    WirelessComm_Uart = &huart4;
    HAL_UART_MspInit(WirelessComm_Uart);
    __HAL_UART_ENABLE_IT(WirelessComm_Uart, UART_IT_IDLE);
    HAL_UART_Receive_DMA(WirelessComm_Uart, BleDMARx_Buffer, RX_BUFFER_SIZE); 
}

void Dev_Info_Save_Check()
{
    static uint16_t Update_Tick;
    static uint8_t Self_Check_Flag = 1;
    uint32_t World_Time;
    Update_Tick += WIRELESS_TASK_DELAY;
    if(Update_Tick%30000 == 0) {  
		Time_IOControl(TIME_WORLD, TIME_GET, &World_Time);
        Update_Tick = 0;
        Log_Status.type = DEV_LOG;
        Log_Status.TimeStamp = World_Time;
        Log_Status.Charge_Status = g_PowerVoltage.State;
        Log_Status.DC_Voltage = g_PowerVoltage.DCIN/10;
        Log_Status.BAT_Voltage = g_PowerVoltage.BAT/10;
        Log_Status.V5_Voltage = g_PowerVoltage.Vol_5V0/10;
        Log_Status.V33_Voltage = g_PowerVoltage.Vol_3V3/10;
        Log_Status.Ble_State = s_BLE_State;
        Log_Status.Wifi_State = s_Wifi_State;
        Log_Status.CPR_State = Get_CPR_State();
		
        osMessageQueuePut(Log_SaveHandle,&Log_Status,0,0);
    }

    if(Self_Check_Flag == 1) {
        if((g_Err.Cpr.byte != 0)&&(g_Err.Power.byte != 0)&&(g_Err.Audio.byte != 0)
        &&(g_Err.Wireless.byte != 0)&&(g_Err.Memory.byte != 0))
        {
            Self_Check_Flag = 0;
            Self_Check_Data.type = SELF_CHECK;
            Self_Check_Data.FeedBack_Self_Check = g_Err.Cpr.byte;
            Self_Check_Data.Power_Self_Check = g_Err.Power.byte;
            Self_Check_Data.Audio_Self_Check = g_Err.Audio.byte;
            Self_Check_Data.WirelessModlue_Self_Check = g_Err.Wireless.byte;
            Self_Check_Data.Memory_Self_Check = g_Err.Memory.byte;
            Self_Check_Data.TimeStamp = World_Time;

            osMessageQueuePut(Self_Check_SaveHandle,&Self_Check_Data,0,0);
        }
    }
}

/*!
* \brief 十六进制字符转换为数值
* \param   c：十六进制字符
* \return 转换后的数值，非法字符返回0xFF
*/
uint8_t hexCharToValue(char c)
{
    if (c >= '0' && c <= '9') {
        return (uint8_t)(c - '0');      // 数字0-9
    }
    else if (c >= 'a' && c <= 'f') {
        return (uint8_t)(c - 'a' + 10); // 小写a-f
    }
    else if (c >= 'A' && c <= 'F') {
        return (uint8_t)(c - 'A' + 10); // 大写A-F
    }
    else {
        return 0xFF; // 非法字符
    }
}
/*!
* \brief 字符串转换为无符号整数
* \param   str：字符串指针
* \return 转换后的无符号整数
*/
uint16_t String_To_Hex(const char* str) {
    uint16_t result = 0;
    
    // 跳过前导空格
    while(*str == ' ') str++;
    
    // 处理空字符串
    if(*str == '\0') return 0;
    
    // 转换数字
    while(*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return result;
}
/*!
* \brief 蓝牙模块初始化
* \param   none
* \return 初始化结果，0失败，1成功
*/
uint8_t App_Ble_Init(void)
{
    uint8_t advData[32];  // 蓝牙广播数据最大31字节
    uint8_t advLen = 0;
    char Device_Name[] = "PRIMEDIC-CPRSensor-";
    
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QRST\r\n",400,wRecv_Char) == wRecv_Char) {
			if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
							return 0;
					}
		}
        else {
			return 2;
		}
    osDelay(300);

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QSTASTOP\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEINIT=2\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
    snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLENAME=PRIMEDIC-CPRSensor-%s\r\n", Factory_Info.Name);
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,(char *)su8_AT_Command,400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}
                        
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEGATTSSRV=fe60\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}
                   
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEGATTSCHAR=fe61\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEGATTSCHAR=fe62\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEADVPARAM=150,150\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}
                    
    memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
    advData[advLen++] = 0;  // 长度
    advData[advLen++] = 0x09;                 // 类型：Complete Local Name
    memcpy(&advData[advLen], Device_Name, sizeof(Device_Name));  // 序列号
    advLen += sizeof(Device_Name);
    memcpy(&advData[advLen-1], Factory_Info.Name, 5);     // 名称数据
    advLen += 4;
	advData[advLen++] = '-';
	memcpy(&advData[advLen], &Factory_Info.Device_SN[9], 5);
	advLen += 4;
    advData[0] = advLen-1;
    snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command),"AT+QBLEADVDATA=%02X", advData[0]);
    for (int i = 1; i < advLen; i++) {
        char temp[5] = {0};
        snprintf(temp, sizeof(temp), "%02X", advData[i]);
        strcat((char *)su8_AT_Command, temp);
    }                      
    strcat((char *)su8_AT_Command, "\r\n");
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,(char *)su8_AT_Command,400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEADDR?\r\n",400,wRecv_Puls) == wRecv_Puls) {
		Ring_WireLessComm.RevData[9] = 0;
		if(strcmp((const char*)Ring_WireLessComm.RevData, "+QBLEADDR") != 0) {
            return 0;
        }
        for(int i = 0; i < 6; i++) {
            MAC_ADRESS[i] = hexCharToValue(Ring_WireLessComm.RevData[10+3*i])<<4;
            MAC_ADRESS[i] += hexCharToValue(Ring_WireLessComm.RevData[11+3*i]);
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QVERSION\r\n",400,wRecv_Puls) == wRecv_Puls) {
        Ring_WireLessComm.RevData[9] = 0;
		if(strcmp((const char*)Ring_WireLessComm.RevData, "+QVERSION") != 0) {
            return 0;
        }
        memcpy(BLE_VER, &Ring_WireLessComm.RevData[10], 33);
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QBLEADVSTART\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}
    return 1;
}
/*!
* \brief WiFi模块初始化
* \param   none
* \return 初始化结果，0失败，1成功
*/
uint8_t Wifi_Init()
{
	if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QRST\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}
    osDelay(300);

    //获取WiFi的MAC地址
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QWLMAC\r\n",400,wRecv_Puls) == wRecv_Puls) {
        Ring_WireLessComm.RevData[7] = 0;
        if(strcmp((const char*)Ring_WireLessComm.RevData, "+QWLMAC") != 0) {
            return 0;
        }
        for(int i = 0; i < 6; i++) {
            MAC_ADRESS[i] = hexCharToValue(Ring_WireLessComm.RevData[8+3*i])<<4;
            MAC_ADRESS[i] += hexCharToValue(Ring_WireLessComm.RevData[9+3*i]);
        }
    }
	return 1;
}

/*!
* \brief 无线模块数据处理  
* \param   Ring_Comm：接收环形缓冲区指针
* \return 处理结果，0无效数据，1有效数据，2普通字符串，3带+的字符串
*/
uint8_t Wireless_Handle_Data(CBuff *Ring_Comm)
{
    uint8_t ret = 0;
	uint16_t Cbuff_Len;
    uint16_t u16CRC_Sum=0,u16CRC_Calc=0xFFFF;

    Cbuff_Len = CBuff_GetLength(Ring_Comm);
    if(Cbuff_Len < 4){
		return ret;
	} 
    else if(Cbuff_Len > 32) {
        CBuff_Read(Ring_Comm,Ring_Comm->RevData,32);
        Cbuff_Len = 32;
    }
    CBuff_Read(Ring_Comm,Ring_Comm->RevData,Cbuff_Len);
    if(Ring_Comm->RevData[0] == 0xFA && Ring_Comm->RevData[1] == 0xFC)  
    {
        // Ring_WireLessComm.DataLen = (Ring_Comm->RevData[4])*256+Ring_Comm->RevData[5];
        // // 蓝牙工作模式才按加密的处理     
		// if(WireLess_Work_State == BLE_WORK_STATE) {			
		// 	Ring_WireLessComm.DataLen = (Ring_WireLessComm.DataLen + 15) & ~15; // 取16的倍数
		// }    
        // Ring_Comm->HandleDataLength = Ring_WireLessComm.DataLen+8; //CRC16
		Ring_WireLessComm.DataLen = (Ring_Comm->RevData[4])*256+Ring_Comm->RevData[5];
        Ring_Comm->HandleDataLength = (Ring_Comm->RevData[4])*256+Ring_Comm->RevData[5]+8; //CRC16
		
        if(Ring_Comm->HandleDataLength > 64)
        {
            CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1); 	// detect if the length over the range
            return ret;
        }
        else if(CBuff_GetLength(Ring_Comm) < Ring_Comm->HandleDataLength)
        {
            Ring_Comm->HandleDataOverTime++;
            if(Ring_Comm->HandleDataOverTime >= 20)   // 5ms*20 = 100ms
            {
                Ring_Comm->HandleDataOverTime = 0;
                CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);				// 100ms don't receive the rest of the data, remove header and return
            }
            return ret;
        }
        else
        {
            Ring_Comm->HandleDataOverTime = 0;
            CBuff_Read(Ring_Comm,Ring_Comm->RevData,Ring_Comm->HandleDataLength);			// Pull out all the data   
			u16CRC_Calc = Crc16Compute(Ring_Comm->RevData+3,(Ring_Comm->HandleDataLength-5)); 
            u16CRC_Sum = Ring_Comm->RevData[Ring_Comm->HandleDataLength-2]*256 + Ring_Comm->RevData[Ring_Comm->HandleDataLength-1];			
            if(u16CRC_Calc == u16CRC_Sum) {  	
                CBuff_Pop(Ring_Comm,Ring_Comm->RevData,Ring_Comm->HandleDataLength);
                //Ble_Data_Unpack(Ring_Comm->RevData);
                ret = 1;
                return ret;
            }
			else
			{
				CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);
			}
            return ret; 
        }
    }
    else if(((Ring_Comm->RevData[0] == '+')&&(Ring_Comm->RevData[1] == 'Q'))
        ||((Ring_Comm->RevData[0] == 'O')&&(Ring_Comm->RevData[1] == 'K'))
        ||((Ring_Comm->RevData[0] == 'E')&&(Ring_Comm->RevData[1] == 'R'))
        ||((Ring_Comm->RevData[0] == 'C')&&(Ring_Comm->RevData[1] == 'N'))
        ||((Ring_Comm->RevData[0] == 'N')&&(Ring_Comm->RevData[1] == 'O')))
    {
        Cbuff_Len = CBuff_GetLength(Ring_Comm);
        if(Cbuff_Len >= 256) {
            CBuff_Read(Ring_Comm,Ring_Comm->RevData,256);
        }
        else {
            CBuff_Read(Ring_Comm,Ring_Comm->RevData,Cbuff_Len);
        }
        for(int i = 0; i <= Cbuff_Len; i++) {
            if(Ring_Comm->RevData[i] == BLE_CR) {
                if(Ring_Comm->RevData[i+1] == BLE_LF) {
                    Ring_Comm->HandleDataOverTime = 0;
                    CBuff_Pop(Ring_Comm,Ring_Comm->RevData,i+2);
                    Ring_Comm->RevData[i] = 0x00;
                    if(Ring_Comm->RevData[0] != '+') {
                        ret = 2;
                    }
                    else{
                        ret = 3;
                    }
                    
                    return ret;
                }
            }
        }
        if(Ring_Comm->HandleDataOverTime >= 100) {
            Ring_Comm->HandleDataOverTime = 0;
            CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);
        }
        Ring_Comm->HandleDataOverTime++;
    }
    else {
        CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);
        for(int i = 1; i < Cbuff_Len; i++) {
            if((Ring_Comm->RevData[i] == 0xFA )
            ||(Ring_Comm->RevData[i] == '+')
			||(Ring_Comm->RevData[i] == 'O')
            ||(Ring_Comm->RevData[i] == 'E')
			||(Ring_Comm->RevData[i] == 'C')
            ||(Ring_Comm->RevData[i] == 'N')){
                break;
            }
			else {
				CBuff_Pop(Ring_Comm,Ring_Comm->RevData,1);
			}
        }
		return ret;
	}
	return ret;
}

/*!
* \brief 十六进制数值转换为字符
* \param   val：数值
*          out1：高4位字符指针
*          out2：低4位字符指针
* \return none
*/
void HexChangeChar(uint8_t val, uint8_t *out1, uint8_t *out2)
{
    // 将高4位转换为十六进制字符
    uint8_t highNibble = (val >> 4) & 0x0F;
    *out1 = (highNibble < 10) ? ('0' + highNibble) : ('A' + highNibble - 10);
    
    // 将低4位转换为十六进制字符
    uint8_t lowNibble = val & 0x0F;
    *out2 = (lowNibble < 10) ? ('0' + lowNibble) : ('A' + lowNibble - 10);
}

/*!
* \brief 无线模块发送握手包
* \param   none
* \return none
*/
void Wireless_Send_HandShake()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_HANDSHAKE;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(MAC_ADRESS, Encrypt_Data, 16);

        memcpy(&sSendBuffer[u16Send_Cnt], Encrypt_Data, 16);
        u16Send_Cnt += 16;
        u16Data_Cnt = 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], MAC_ADRESS, 6);
        u16Send_Cnt += 6;
    }
    
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256;  
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';

        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
        }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
	osDelay(20);
}

/*! 
* \brief 无线模块发送断开连接包
* \param   none
* \return none
*/
void Wireless_Send_Disconnect()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_DISCONNECT;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */

    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256;
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 无线模块发送设备信息包
* \param   none
* \return none
*/
void Wireless_Send_Dev_Info()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_DEV_INFO;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */
    su8_DataBuffer[u16Data_Cnt++] = Factory_Info.Type;
    for(int i = 0; i < 13; i++) {
        su8_DataBuffer[u16Data_Cnt++] = (Factory_Info.Device_SN[i]);
    }
    su8_DataBuffer[u16Data_Cnt++] = 0x01;
    su8_DataBuffer[u16Data_Cnt++] = SoftWare_Version;
    su8_DataBuffer[u16Data_Cnt++] = SoftSub_Version;
    su8_DataBuffer[u16Data_Cnt++] = SoftBuild_Version;

    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 32);
        memcpy(&sSendBuffer[6], Encrypt_Data, 32);
        u16Send_Cnt += 32;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }    
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256;  
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 无线模块发送蓝牙信息包
* \param   none
* \return none
*/
void Wireless_Send_BLE_Info()
{
	uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_BLE_INFO;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */
    for(int i = 0; i < sizeof(BLE_VER); i++) {
        su8_DataBuffer[u16Data_Cnt++] = BLE_VER[i];
    }

    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 48);
        memcpy(&sSendBuffer[6], Encrypt_Data, 48);
        u16Send_Cnt += 48;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 无线模块回复语言
* \param   none
* \return none
*/
void Wireless_Send_BLE_Language()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_LANGUAGE;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */
    su8_DataBuffer[u16Data_Cnt++] = Audio_Language_Rev;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 16);
        memcpy(&sSendBuffer[6], Encrypt_Data, 16);
        u16Send_Cnt += 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 无线模块发送音量包
* \param   none
* \return none
*/
void Wireless_Send_BLE_Volume()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_VOLUME;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */
    su8_DataBuffer[u16Data_Cnt++] = Audio_Volume_Rev;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 16);
        memcpy(&sSendBuffer[6], Encrypt_Data, 16);
        u16Send_Cnt += 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 无线模块发送电池包
* \param   none
* \return none
*/
void Wireless_Send_Battery()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_BATTERY;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */
    if (osMutexAcquire(Power_MutexHandle, osWaitForever) == osOK) {
        su8_DataBuffer[u16Data_Cnt++] = g_PowerVoltage.BatValue;
        su8_DataBuffer[u16Data_Cnt++] = g_PowerVoltage.BAT >> 8;
        su8_DataBuffer[u16Data_Cnt++] = g_PowerVoltage.BAT & 0xFF;
        su8_DataBuffer[u16Data_Cnt++] = (uint8_t)g_PowerVoltage.State;
        osMutexRelease(Power_MutexHandle);
    }
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 16);
        memcpy(&sSendBuffer[6], Encrypt_Data, 16);
        u16Send_Cnt += 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 无线模块发送心跳包
* \param   none
* \return none
*/
void Wireless_Send_HeartBeat()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_HEARTBEAT;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */

    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256;  
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 无线模块发送通讯设置包
* \param   none
* \return none
*/
void Wireless_Send_Comm_Setting_Info()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_COMM_SETTING;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    su8_DataBuffer[u16Data_Cnt++] = Comm_Priority;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 16);
        memcpy(&sSendBuffer[6], Encrypt_Data, 16);
        u16Send_Cnt += 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256;
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 无线模块发送上传方式包
* \param   Recv_Method：上传方式
* \return none
*/
void Wireless_Send_CPR_Upload_Method()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_UPLOAD_METHOD;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    su8_DataBuffer[u16Data_Cnt++] = Memory_SendState.Status;
    
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 16);
        memcpy(&sSendBuffer[6], Encrypt_Data, 16);
        u16Send_Cnt += 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

void MQTT_Send_CPR_Upload_Method()
{
    char timestr[16] = {0};
	uint32_t timestamp;
    Time_IOControl(TIME_REAL, TIME_GET, &timestamp);
    snprintf(timestr,sizeof(timestr), "%d", timestamp);
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    su8_DataBuffer[u16Data_Cnt++] = 0xFA;
    su8_DataBuffer[u16Data_Cnt++] = 0xFC;
    su8_DataBuffer[u16Data_Cnt++] = 0x01; 
    su8_DataBuffer[u16Data_Cnt++] = E_CMD_UPLOAD_METHOD;   // CMD
    u16Data_Cnt += 2; // 预留长度字段位置
    /***************************************** */
    su8_DataBuffer[u16Data_Cnt++] = Memory_SendState.Status;
	
    su8_DataBuffer[4] = u16Data_Cnt / 256;
    su8_DataBuffer[5] = u16Data_Cnt % 256; 
	// 计算CRC
    u16CRC_Calc = Crc16Compute(&su8_DataBuffer[3], (u16Data_Cnt - 3)); 
    su8_DataBuffer[u16Data_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = u16CRC_Calc & 0xFF;
	
	for(int i = 0; i < u16Data_Cnt; i++)
    {       
        // 将每个字节转换为两个十六进制字符
        HexChangeChar(su8_DataBuffer[i], &sSendBuffer[u16Send_Cnt], &sSendBuffer[u16Send_Cnt + 1]);
        u16Send_Cnt += 2;
    }
	// 填充长度字段
    u16Send_Cnt = (u16Send_Cnt + 15) & ~15; // 填充到16的倍数
    my_aes_encrypt(sSendBuffer, Encrypt_Data, u16Send_Cnt);
	
    /***************************************** */
	memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
    snprintf((char *)su8_AT_Command,sizeof(su8_AT_Command),"{\"timestamp\":\"%s\",\"messageId\":\"%s\",\"data\":\"",timestr,"1234567812345678");
    u16String_Cnt = strlen((char *)su8_AT_Command);
    for(int i = 0; i < u16Send_Cnt; i++)
    {
        // 检查缓冲区边界，防止溢出
        if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
        
        // 将每个字节转换为两个十六进制字符
        HexChangeChar(Encrypt_Data[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
        u16String_Cnt += 2;
    }
    su8_AT_Command[u16String_Cnt++] = '\"';
    su8_AT_Command[u16String_Cnt++] = '}';
    su8_AT_Command[u16String_Cnt++] = '\r';
    su8_AT_Command[u16String_Cnt++] = '\n'; 
    MQTT_Publish("event/transfer",(char *)su8_AT_Command);
}

/*!
* \brief 无线模块发送时间同步包
* \param   Recv_World_Time_Tick：世界时间戳
* \return none
*/
void Wireless_Send_Time_Sync()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_TIME_SYCN;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    su8_DataBuffer[u16Data_Cnt++] = Recv_World_Time_Tick>>24;
    su8_DataBuffer[u16Data_Cnt++] = Recv_World_Time_Tick>>16;
    su8_DataBuffer[u16Data_Cnt++] = Recv_World_Time_Tick>>8;
    su8_DataBuffer[u16Data_Cnt++] = Recv_World_Time_Tick&0x000000FF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 16);
        memcpy(&sSendBuffer[6], Encrypt_Data, 16);
        u16Send_Cnt += 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 无线模块发送上电时间
* \param   Recv_World_Time_Tick：世界时间戳
* \return none
*/
void Wireless_Send_Boot_Time()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_BOOT_TIME;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    su8_DataBuffer[u16Data_Cnt++] = CPR_Time.TimeStamp >> 24;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Time.TimeStamp >>16;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Time.TimeStamp >>8;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Time.TimeStamp &0x000000FF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 16);
        memcpy(&sSendBuffer[6], Encrypt_Data, 16);
        u16Send_Cnt += 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

void Wireless_Send_Memory_Data()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_HISTORY_DATA;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    uint16_t mem_data_len = (Memory_SendState.Index + 15) & ~15;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(Memory_SendState.SendBuffer, Encrypt_Data, mem_data_len);
        memcpy(&sSendBuffer[6], Encrypt_Data, mem_data_len);
        u16Send_Cnt += mem_data_len;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], Memory_SendState.SendBuffer, mem_data_len);
        u16Send_Cnt += mem_data_len;
    }
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = mem_data_len; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
        Memory_SendState.SendBusyFlag = false;
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

void MQTT_Send_Memory_Data()
{
    static volatile char atcmd[1024] = {0};
    char timestr[16] = {0};
    uint16_t datalen;
	uint32_t timestamp;
    Time_IOControl(TIME_REAL, TIME_GET, &timestamp);
    snprintf(timestr,sizeof(timestr), "%d", timestamp);
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    su8_DataBuffer[u16Data_Cnt++] = 0xFA;
    su8_DataBuffer[u16Data_Cnt++] = 0xFC;
    su8_DataBuffer[u16Data_Cnt++] = 0x01; 
    su8_DataBuffer[u16Data_Cnt++] = E_CMD_HISTORY_DATA;   
    u16Data_Cnt += 2; // 预留长度字段位置
    /***************************************** */
    uint16_t mem_data_len = (Memory_SendState.Index + 15) & ~15;
    memcpy(&su8_DataBuffer[u16Data_Cnt], Memory_SendState.SendBuffer, mem_data_len);
    u16Data_Cnt += mem_data_len;
	
    su8_DataBuffer[4] = Memory_SendState.Index / 256;
    su8_DataBuffer[5] = Memory_SendState.Index % 256; 
	// 计算CRC
    u16CRC_Calc = Crc16Compute(&su8_DataBuffer[3], (u16Data_Cnt - 3)); 
    su8_DataBuffer[u16Data_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = u16CRC_Calc & 0xFF;
	
	for(int i = 0; i < u16Data_Cnt; i++)
    {       
        // 将每个字节转换为两个十六进制字符
        HexChangeChar(su8_DataBuffer[i], &sSendBuffer[u16Send_Cnt], &sSendBuffer[u16Send_Cnt + 1]);
        u16Send_Cnt += 2;
    }
	// 填充长度字段
    u16Send_Cnt = (u16Send_Cnt + 15) & ~15; // 填充到16的倍数
    my_aes_encrypt(sSendBuffer, Encrypt_Data, u16Send_Cnt);
	
    /***************************************** */
	memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
    snprintf((char *)su8_AT_Command,sizeof(su8_AT_Command),"{\"timestamp\":\"%s\",\"messageId\":\"%s\",\"data\":\"",timestr,"7777777777777777");
    u16String_Cnt = strlen((char *)su8_AT_Command);
    for(int i = 0; i < u16Send_Cnt; i++)
    {
        // 检查缓冲区边界，防止溢出
        if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
        
        // 将每个字节转换为两个十六进制字符
        HexChangeChar(Encrypt_Data[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
        u16String_Cnt += 2;
    }
    su8_AT_Command[u16String_Cnt++] = '\"';
    su8_AT_Command[u16String_Cnt++] = '}';
    su8_AT_Command[u16String_Cnt++] = '\r';
    su8_AT_Command[u16String_Cnt++] = '\n'; 
    
    memset((uint8_t *)atcmd,0x00,1024);
    snprintf((char *)atcmd,sizeof(atcmd),"AT+QMTPUB=0,0,0,1,\"CPR/%s/%s\",%d,\"%s\"\r\n",Factory_Info.Device_SN, "event/transfer", u16String_Cnt, su8_AT_Command);
    datalen = strlen((char *)atcmd);
    // 如果dma忙，则等待DMA发送完成
    if(HAL_UART_Transmit_DMA(WirelessComm_Uart, (uint8_t *)atcmd, datalen) == HAL_OK) {
        Memory_SendState.SendBusyFlag = false;
    }   
}
/*!
* \brief 无线模块发送心肺复苏数据包
* \param   none
* \return none
*/
void Wireless_Send_CPR_Data()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_CPR_DATA;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.TimeStamp>>24;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.TimeStamp>>16;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.TimeStamp>>8;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.TimeStamp&0x000000FF;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.Freq >> 8;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.Freq & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.Depth;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.RealseDepth;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.Interval;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.BootStamp>>24;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.BootStamp>>16;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.BootStamp>>8;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.BootStamp&0x000000FF;

    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 16);
        memcpy(&sSendBuffer[6], Encrypt_Data, 16);
        u16Send_Cnt += 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 无线模块发送上传方式包
* \param   none
* \return none
*/
void Wireless_Send_Upload_Method()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_UPLOAD_METHOD;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    su8_DataBuffer[u16Data_Cnt++] = Memory_SendState.Status;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 16);
        memcpy(&sSendBuffer[6], Encrypt_Data, 16);
        u16Send_Cnt += 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
	/***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 无线模块发送WIFI设置包
* \param   none
* \return none
*/
void Wireless_Send_WifiSetting() 
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    uint16_t Data_Encry_Len;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_WIFI_SETTING;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    su8_DataBuffer[u16Data_Cnt++] = strlen((const char*)Wifi_Module.SSID);
    for(int i = 0; i < strlen((const char*)Wifi_Module.SSID); i++) {
       su8_DataBuffer[u16Data_Cnt++] = Wifi_Module.SSID[i];
    }
    su8_DataBuffer[u16Data_Cnt++] = strlen((const char*)Wifi_Module.PWD);
    for(int i = 0; i < strlen((const char*)Wifi_Module.PWD); i++) {
       su8_DataBuffer[u16Data_Cnt++] = Wifi_Module.PWD[i];
    }
    Data_Encry_Len = (u16Data_Cnt + 15)&~15;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, Data_Encry_Len);
        memcpy(&sSendBuffer[6], Encrypt_Data, Data_Encry_Len);
        u16Send_Cnt += Data_Encry_Len;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 无线模块发送TCP设置包
* \param   none
* \return none
*/
void Wireless_Send_TCPSetting() {
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_TCP_SETTING;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 

    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

void Wireless_Send_UTC_Setting() {
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_UTC_SETTING;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    for(int i = 0; i < 6; i++){
        su8_DataBuffer[u16Data_Cnt++] = (UTC_Offset_Time[i]);
    }
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 16);
        memcpy(&sSendBuffer[6], Encrypt_Data, 16);
        u16Send_Cnt += 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';  
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief  无线模块发送波形数据包
* \param   pData：波形数据缓冲区
* \return none
*/
void Wireless_Send_Waveform_Data(uint8_t *pData)
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_CPR_RAW_DATA;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    memcpy(su8_DataBuffer, pData, 8);
    u16Data_Cnt += 8;
    /***************************************** */
    // 填充长度字段
    //u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   

}

void Wireless_Send_Log_Data()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_LOG_DATA;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    su8_DataBuffer[u16Data_Cnt++] = (uint8_t)(Log_Status.TimeStamp >> 24);
	su8_DataBuffer[u16Data_Cnt++] = (uint8_t)(Log_Status.TimeStamp >> 16);
	su8_DataBuffer[u16Data_Cnt++] = (uint8_t)(Log_Status.TimeStamp >> 8);
	su8_DataBuffer[u16Data_Cnt++] = (uint8_t)(Log_Status.TimeStamp);
    su8_DataBuffer[u16Data_Cnt++] = Log_Status.Charge_Status;
    su8_DataBuffer[u16Data_Cnt++] = Log_Status.DC_Voltage;
    su8_DataBuffer[u16Data_Cnt++] = Log_Status.BAT_Voltage;
    su8_DataBuffer[u16Data_Cnt++] = Log_Status.V5_Voltage;
    su8_DataBuffer[u16Data_Cnt++] = Log_Status.V33_Voltage;
    su8_DataBuffer[u16Data_Cnt++] = Log_Status.Ble_State;
    su8_DataBuffer[u16Data_Cnt++] = Log_Status.Wifi_State;
    su8_DataBuffer[u16Data_Cnt++] = Log_Status.CPR_State;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 16);
        memcpy(&sSendBuffer[6], Encrypt_Data, 16);
        u16Send_Cnt += 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt );
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 发送自检结果
* \param  none
* \return none
*/
void Wireless_Send_SelfCheck_Result()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_SELF_CHECK;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    su8_DataBuffer[u16Data_Cnt++] = Self_Check_Data.FeedBack_Self_Check ;
    su8_DataBuffer[u16Data_Cnt++] = Self_Check_Data.Power_Self_Check;
    su8_DataBuffer[u16Data_Cnt++] = Self_Check_Data.Audio_Self_Check;
    su8_DataBuffer[u16Data_Cnt++] = Self_Check_Data.WirelessModlue_Self_Check;
    su8_DataBuffer[u16Data_Cnt++] = Self_Check_Data.Memory_Self_Check;
    su8_DataBuffer[u16Data_Cnt++] = (Self_Check_Data.TimeStamp >> 24) & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = (Self_Check_Data.TimeStamp >> 16) & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = (Self_Check_Data.TimeStamp >> 8) & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = (Self_Check_Data.TimeStamp) & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 16);
        memcpy(&sSendBuffer[6], Encrypt_Data, 16);
        u16Send_Cnt += 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt );
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 无线模块回复节拍器频率
* \param   none
* \return none
*/
void Wireless_Reply_Metronome()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_METRONOME;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    su8_DataBuffer[u16Data_Cnt++] = CPR_Metronome_Freq_Recv;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 16);
        memcpy(&sSendBuffer[6], Encrypt_Data, 16);
        u16Send_Cnt += 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt );
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}
/*!
* \brief 无线模块发送清除存储数据包
* \param   none
* \return none
*/
void Wireless_Send_Memory_Clear()
{
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01; 
    sSendBuffer[u16Send_Cnt++] = E_CMD_CLEAR_MEMORY;   // CMD
    u16Send_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    su8_DataBuffer[u16Data_Cnt++] = Get_Memory_Clear();
    if(WireLess_Work_State == BLE_WORK_STATE) {
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, 16);
        memcpy(&sSendBuffer[6], Encrypt_Data, 16);
        u16Send_Cnt += 16;
    }
    else {
        memcpy(&sSendBuffer[u16Send_Cnt], su8_DataBuffer, u16Data_Cnt);
        u16Send_Cnt += u16Data_Cnt;
    }
    /***************************************** */
    // 填充长度字段
    u16Data_Cnt = (u16Data_Cnt + 15) & ~15; // 填充到16的倍数
    sSendBuffer[4] = u16Data_Cnt / 256;
    sSendBuffer[5] = u16Data_Cnt % 256; 
    // 计算CRC
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3)); 
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;
    if(WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,",u16Send_Cnt );
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for(int i = 0; i < u16Send_Cnt; i++)
        {
            // 检查缓冲区边界，防止溢出
            if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
            
            // 将每个字节转换为两个十六进制字符
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';
        Wireless_Ring_Send(su8_AT_Command,u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer,u16Send_Cnt);   
    }
}

/*!
* \brief 无线模块数据解包处理
* \param   cmd：命令字节
* \param   Recv_Data：接收数据缓冲区
* \return none
*/
void Data_Unpack_Handle(uint8_t cmd,uint8_t *Recv_Data, uint16_t Recv_Len)
{
    switch(cmd) {
        case E_CMD_HEARTBEAT:
            Wireless_SendFlag.bits.Reply_HeartBeat = 1; 
            break;
        case E_CMD_DISCONNECT:
            Wireless_SendFlag.bits.Reply_Disconnect = 1;
            BLE_StateFlag.bits.Disconnect = 1;
            break;
        case E_CMD_DEV_INFO:
            Wireless_SendFlag.bits.Reply_Dev_Info = 1;
            break;
        case E_CMD_BLE_INFO:
            Wireless_SendFlag.bits.Reply_BLE_Info = 1;
            break;
        case E_CMD_WIFI_SETTING:
            Wireless_SendFlag.bits.Reply_WIFI_Setting = 1;  
            memset(Wifi_Module.SSID,0x00,sizeof(Wifi_Module.SSID));
            memcpy(Wifi_Module.SSID,&Recv_Data[1],Recv_Data[0]);
            memset(Wifi_Module.PWD,0x00,sizeof(Wifi_Module.PWD));
            memcpy(Wifi_Module.PWD,&Recv_Data[Recv_Data[0]+2],Recv_Data[Recv_Data[0]+1]);
						Set_WifiSave_Flag();
            break;
        case E_CMD_COMM_SETTING:
            Wireless_SendFlag.bits.Reply_COM_Setting = 1;
            Comm_Priority = (Comm_Priority_EnumDef)Recv_Data[0];
            break;
        case E_CMD_TCP_SETTING:
            Wireless_SendFlag.bits.Reply_TCP_Setting = 1;
            memset(Wifi_Module.IP,0x00,sizeof(Wifi_Module.IP));
            memcpy(Wifi_Module.IP,&Recv_Data[1],Recv_Data[0]);
            memset(Wifi_Module.Port,0x00,sizeof(Wifi_Module.Port));
            memcpy(Wifi_Module.Port,&Recv_Data[Recv_Data[0]+2],Recv_Data[Recv_Data[0]+1]);
            break;
        case E_CMD_UPLOAD_METHOD:
            Memory_SendState.Upload = (CPR_Upload_Method_EnumDef)Recv_Data[0];
            break;
        case E_CMD_CPR_DATA:
            break;
        case E_CMD_TIME_SYCN:
            Wireless_SendFlag.bits.Reply_Time_Sync = 1;            
            Recv_World_Time_Tick = ((uint64_t)Recv_Data[0] << 24) +
                            ((uint64_t)Recv_Data[1] << 16) +
                            ((uint64_t)Recv_Data[2] << 8) +
                            (uint64_t)Recv_Data[3];
            Time_IOControl(TIME_WORLD, TIME_UPDATE, &Recv_World_Time_Tick);
            break;
        case E_CMD_BATTERY:
            Wireless_SendFlag.bits.Reply_Battery = 1;
            break;
        case E_CMD_LANGUAGE:
            Wireless_SendFlag.bits.Reply_Language = 1;
            Audio_Language_Rev = (Audio_Language_EnumDef)Recv_Data[0];
            Audio_Change_Language_Flag = 1;
            break;
        case E_CMD_VOLUME:
            Wireless_SendFlag.bits.Reply_Volume = 1;
            Audio_Volume_Rev = Recv_Data[0];
            break;
        case E_CMD_CLEAR_MEMORY:
            Wireless_SendFlag.bits.Reply_Clear_Result = 1;
            Set_Memory_Clear(0x01);
            break;
        case E_CMD_METRONOME:
            Wireless_SendFlag.bits.Reply_Metronome_Freq = 1;
            CPR_Metronome_Freq_Recv = Recv_Data[0];
            break;
        case E_CMD_UTC_SETTING:
            Wireless_SendFlag.bits.Reply_Sync_UTC_Time = 1;
            for(int i = 0; i < 6; i++) {
                UTC_Offset_Time[i] = Recv_Data[i];
            }
            memcpy(&UTC_Offset_Time_Trans, (uint8_t*)UTC_Offset_Time, 6);
            break;
        case E_CMD_OTA_REQUEST:
        case E_CMD_OTA_FILE_INFO:
        case E_CMD_OTA_OFFSET:
        case E_CMD_OTA_DATA:
        case E_CMD_OTA_RESULT:
            App_Flash_PushPacket(cmd, Recv_Data, Recv_Len);
            break;
        default:
            break;
    }
}

static void Wireless_Send_Frame(uint8_t cmd, const uint8_t *payload, uint16_t payload_len)
{
    uint16_t u16CRC_Calc = 0xFFFF;
    uint16_t u16Send_Cnt = 0;
    uint16_t u16String_Cnt = 0;
    uint16_t u16Data_Cnt = payload_len;
    uint16_t padded_len;

    if (u16Data_Cnt > sizeof(su8_DataBuffer)) {
        u16Data_Cnt = sizeof(su8_DataBuffer);
    }

    memset(sSendBuffer, 0x00, sizeof(sSendBuffer));
    memset(su8_DataBuffer, 0x00, sizeof(su8_DataBuffer));
    memset(su8_AT_Command, 0x00, sizeof(su8_AT_Command));
    sSendBuffer[u16Send_Cnt++] = 0xFA;
    sSendBuffer[u16Send_Cnt++] = 0xFC;
    sSendBuffer[u16Send_Cnt++] = 0x01;
    sSendBuffer[u16Send_Cnt++] = cmd;
    u16Send_Cnt += 2;

    padded_len = (u16Data_Cnt + 15U) & (uint16_t)~15U;
    if (WireLess_Work_State == BLE_WORK_STATE) {
        memset(Encrypt_Data, 0x00, sizeof(Encrypt_Data));
        if ((payload != NULL) && (u16Data_Cnt > 0U)) {
            memcpy(su8_DataBuffer, payload, u16Data_Cnt);
        }
        my_aes_encrypt(su8_DataBuffer, Encrypt_Data, padded_len);
        memcpy(&sSendBuffer[6], Encrypt_Data, padded_len);
        u16Send_Cnt += padded_len;
    }
    else {
        if ((payload != NULL) && (u16Data_Cnt > 0U)) {
            memcpy(&sSendBuffer[u16Send_Cnt], payload, u16Data_Cnt);
            u16Send_Cnt += u16Data_Cnt;
        }
    }

    sSendBuffer[4] = padded_len / 256;
    sSendBuffer[5] = padded_len % 256;
    u16CRC_Calc = Crc16Compute(&sSendBuffer[3], (u16Send_Cnt - 3));
    sSendBuffer[u16Send_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    sSendBuffer[u16Send_Cnt++] = u16CRC_Calc & 0xFF;

    if (WireLess_Work_State == BLE_WORK_STATE) {
        snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,", u16Send_Cnt);
        u16String_Cnt = strlen((char *)su8_AT_Command);
        for (int i = 0; i < u16Send_Cnt; i++) {
            if (u16String_Cnt + 2 >= WIRELESS_MAX_LEN) {
                break;
            }
            HexChangeChar(sSendBuffer[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
            u16String_Cnt += 2;
        }
        su8_AT_Command[u16String_Cnt++] = '\r';
        su8_AT_Command[u16String_Cnt++] = '\n';
        Wireless_Ring_Send(su8_AT_Command, u16String_Cnt);
    }
    else {
        Wireless_Uart_Send(sSendBuffer, u16Send_Cnt);
    }
}

uint8_t Wireless_Get_Connect_Status()
{
	if(s_BLE_State == BLE_CONNECTED_STATE){
		return 1;
	}
	return 0;
}

/*!
* \brief 无线模块发送处理
* \param   none
* \return none
*/
void Wireless_Send_Handle()
{
	static uint8_t Circle_Update_Data_Tick;
    App_Flash_Reply_TypeDef ota_reply;

    if (App_Flash_FetchReply(&ota_reply)) {
        Wireless_Send_Frame(ota_reply.cmd, ota_reply.data, ota_reply.len);
        App_Flash_NotifyReplySent();
        return;
    }

    if(Wireless_SendFlag.bits.Reply_Disconnect == 1) {
		Wireless_SendFlag.bits.Reply_Disconnect = 0;
		Wireless_Send_Disconnect();
	}

    if(Memory_SendState.UpdateStatus == true) {
        Memory_SendState.UpdateStatus = false;
        Wireless_Send_CPR_Upload_Method();
    }
    
    if(Memory_SendState.SendBusyFlag == true) {
        Wireless_Send_Memory_Data();
    }

    if(Memory_SendState.Upload == UPLOAD_NONE) 
    {
        Circle_Update_Data_Tick += WIRELESS_TASK_DELAY;
        if(Wireless_SendFlag.bits.Reply_HandShake == 1) {
            Wireless_SendFlag.bits.Reply_HandShake = 0;
            Wireless_Send_HandShake();		
        }

        if(osMessageQueueGet(Boot_Time_SendHandleHandle, &CPR_Time, NULL, 0) == osOK) {
            Wireless_Send_Boot_Time();
        }
        
        if(s_BLE_State == BLE_CONNECTED_STATE)
        {
            if(osMessageQueueGet(CPR_Time_SendHandle, &CPR_Time, NULL, 0) == osOK) {
            	Recv_World_Time_Tick = CPR_Time.TimeStamp;
            	Wireless_Send_Time_Sync();
            }

            if(osMessageQueueGet(CPR_Data_SendHandle, &CPR_Data, NULL, 0) == osOK) {
                Wireless_Send_CPR_Data();
            }
            
            // if(osMessageQueueGet(Log_SendHandle, &Log_Status, NULL, 0) == osOK) {
            // 	Wireless_Send_Log_Data();
            // }

            if(osMessageQueueGet(Self_Check_SendHandle, &Self_Check_Data, NULL, 0) == osOK) {
            	Wireless_Send_SelfCheck_Result();
            }

            if((Wireless_SendFlag.bits.Reply_Battery == 1)||(Circle_Update_Data_Tick%1000 == 0)) {
                Wireless_SendFlag.bits.Reply_Battery = 0;
                Wireless_Send_Battery();
            }
        }

        if(Wireless_SendFlag.bits.Reply_HeartBeat == 1) {
            Wireless_SendFlag.bits.Reply_HeartBeat = 0;
            Wireless_Send_HeartBeat();
        }

        if(Wireless_SendFlag.bits.Reply_Dev_Info == 1) {
            Wireless_SendFlag.bits.Reply_Dev_Info = 0;
            Wireless_Send_Dev_Info();
        }

        if(Wireless_SendFlag.bits.Reply_Time_Sync == 1) {
            Wireless_SendFlag.bits.Reply_Time_Sync = 0;
            Wireless_Send_Time_Sync();
        }

        if(Wireless_SendFlag.bits.Reply_BLE_Info == 1) {
            Wireless_SendFlag.bits.Reply_BLE_Info = 0;
            Wireless_Send_BLE_Info();
        }

        if(Wireless_SendFlag.bits.Reply_COM_Setting == 1) {
            Wireless_SendFlag.bits.Reply_COM_Setting = 0;
            Wireless_Send_Comm_Setting_Info();
        }

        if(Wireless_SendFlag.bits.Reply_Upload_Method == 1) {
            Wireless_SendFlag.bits.Reply_Upload_Method = 0;
            Wireless_Send_Upload_Method();
        }

        if(Wireless_SendFlag.bits.Reply_Language == 1) {
            Wireless_SendFlag.bits.Reply_Language = 0;
            Wireless_Send_BLE_Language();
        }

        if(Wireless_SendFlag.bits.Reply_Volume == 1) {
            Wireless_SendFlag.bits.Reply_Volume = 0;
            Wireless_Send_BLE_Volume();
        }

        if(Wireless_SendFlag.bits.Reply_WIFI_Setting == 1) {
            Wireless_SendFlag.bits.Reply_WIFI_Setting = 0;
            Wireless_Send_WifiSetting();
        }

        if(Wireless_SendFlag.bits.Reply_TCP_Setting == 1) {
            Wireless_SendFlag.bits.Reply_TCP_Setting = 0;
            Wireless_Send_TCPSetting();
        }

        if(Wireless_SendFlag.bits.Reply_Sync_UTC_Time == 1) {
            Wireless_SendFlag.bits.Reply_Sync_UTC_Time = 0;
            Wireless_Send_UTC_Setting();
        }

        if(Wireless_SendFlag.bits.Reply_SelfCheck_Result == 1) {
            if((g_Err.Cpr.byte != 0)&&(g_Err.Power.byte != 0)&&(g_Err.Audio.byte != 0)
            &&(g_Err.Wireless.byte != 0)&&(g_Err.Memory.byte != 0)&&(s_BLE_State == BLE_CONNECTED_STATE))
            {
                Wireless_SendFlag.bits.Reply_SelfCheck_Result = 0;
                Self_Check_Data.FeedBack_Self_Check = g_Err.Cpr.byte;
                Self_Check_Data.Power_Self_Check = g_Err.Power.byte;
                Self_Check_Data.Audio_Self_Check = g_Err.Audio.byte;
                Self_Check_Data.WirelessModlue_Self_Check = g_Err.Wireless.byte;
                Self_Check_Data.Memory_Self_Check = g_Err.Memory.byte;
                Time_IOControl(TIME_POWERUP, TIME_GET, &Self_Check_Data.TimeStamp);
                Wireless_Send_SelfCheck_Result();
            }
        }

        if(Wireless_SendFlag.bits.Reply_Clear_Result == 1) {
            Wireless_SendFlag.bits.Reply_Clear_Result = 0;
            Wireless_Send_Memory_Clear();
            if(Get_Memory_Clear() == 2) {
                Set_Memory_Clear(0x00);
            }
        }

        if(Get_Memory_Clear() == 2) {
            Wireless_SendFlag.bits.Reply_Clear_Result = 1;
        }

        if(Wireless_SendFlag.bits.Reply_Metronome_Freq == 1) {
            Wireless_SendFlag.bits.Reply_Metronome_Freq = 0;
            Wireless_Reply_Metronome();
        }
        
        if(Wireless_SendFlag.bits.Reply_Sync_Boot_Time == 1) {
            Wireless_SendFlag.bits.Reply_Sync_Boot_Time = 0;
            Time_IOControl(TIME_POWERUP, TIME_GET, &CPR_Time.TimeStamp);
            Wireless_Send_Boot_Time();
        }
    }
}

/*!
* \brief 无线模块状态切换
* \param   state：目标状态
* \return none
*/
void Change_Ble_State(BLE_FSM_EnumDef state)
{
    if((s_BLE_State != state)&&(state < BLE_STATE_MAX)){
        s_BLE_State = state;
    }
}

/*!
* \brief 从接收缓冲区提取IP地址
* \param   start_index：IP地址在缓冲区中的起始索引
* \return none
*/
void ExtractIPFromBuffer(uint16_t start_index)
{
    uint8_t *src_ptr = &Ring_WireLessComm.RevData[start_index];
    uint8_t *dest_ptr = Wifi_Module.Dev_IP;
    uint16_t max_len = sizeof(Wifi_Module.Dev_IP) - 1; // 保留一个字节给'\0'
    uint16_t copied_len = 0;
    
    memset(Wifi_Module.Dev_IP, 0x00, sizeof(Wifi_Module.Dev_IP));
    
    while (*src_ptr != ',' && copied_len < max_len)
    {
        *dest_ptr++ = *src_ptr++;
        copied_len++;
    }
    
    Wifi_Module.Dev_IP[copied_len] = '\0';
    
}

void Init_MD5_key()
{
    char Unpack_MD5[33] = {0};
    MD5_To_32Upper((char *)MAC_ADRESS,(char *)Unpack_MD5);
    // 把Unpack_MD5转换为16进制
    for(int i = 0; i < 16; i++) {
        sscanf(&Unpack_MD5[i*2], "%2hhx", &MD5_MAC_KEY[i]);
    }
    aesInfo.key = (const void *)MD5_MAC_KEY;
	my_aes_init();
}


BLE_FSM_EnumDef* Wireless_Get_State()
{
    return &s_BLE_State;
}

/*!
* \brief 无线模块状态机
* \param   none
* \return none
*/
void App_Ble_FSM()
{
    static uint16_t s_HandShake_Tick,s_Offline_Tick;
    static uint8_t Ble_Init_Err_Cnt;

    switch(s_BLE_State) {
        case BLE_INIT_STATE:
            if(App_Ble_Init() == 1) {
                Init_MD5_key();
                s_BLE_State = BLE_DISCONNECT_STATE;
                SEGGER_RTT_printf (0,"BLE Init OK\r\n");
				g_WirelessModlue_Self_Check = 1;
                g_Err.Wireless.bits.Self_Check_Ok = 1;
				Wireless_SendFlag.bits.Reply_SelfCheck_Result = 1;
            } 
            else {
                osDelay(500);
                Ble_Init_Err_Cnt++;				
                // Report error rumi
            }
            if(Ble_Init_Err_Cnt >= 3) {
                Ble_Init_Err_Cnt = 0;
                App_Module_Reset();
                g_Err.Wireless.bits.Init_Err = 1;
                g_WirelessModlue_Self_Check = 2;
            }
            break;
        case BLE_DISCONNECT_STATE:
            s_Wireless_Data_Recv_Flag = (WireLess_Recv_EnumDef)Wireless_Handle_Data(&Ring_WireLessComm);
            if(s_Wireless_Data_Recv_Flag == wRecv_Puls) {
                s_Wireless_Data_Recv_Flag = wRecv_None;
                if(strcmp((const char*)Ring_WireLessComm.RevData, "+QBLESTAT:CONNECTED") == 0) {
                    SEGGER_RTT_printf (0,"BLE Connected Start HandShake\r\n");
                    if(I2C_GetConnectionStatus() == true) {
                        s_HandShake_Tick = 0; 
                    } 
                    else {
                        s_HandShake_Tick = 30000;
                    }
                      				
                }
				else if(strcmp((const char*)Ring_WireLessComm.RevData, "+QBLESTAT:DISCONNECTED") == 0) {
                    SEGGER_RTT_printf (0,"BLE Disconnected\r\n");
                    s_HandShake_Tick = 0;
                }
            }
            else if (s_Wireless_Data_Recv_Flag == wRecv_Data){
                s_Wireless_Data_Recv_Flag = wRecv_None;
                memcpy(Encrypt_Data,Ring_WireLessComm.RevData+6,Ring_WireLessComm.DataLen);
				my_aes_decrypt(Encrypt_Data,Decrypt_Data,Ring_WireLessComm.DataLen);
				if(strcmp((const char*)Decrypt_Data, (const char*)MAC_ADRESS) == 0) {
					BLE_StateFlag.bits.HandShake_OK = 1;
				}
            }

            if(s_HandShake_Tick > 0) {
				s_HandShake_Tick -= WIRELESS_TASK_DELAY;
				if(BLE_StateFlag.bits.HandShake_OK == 1) {
						BLE_StateFlag.bits.HandShake_OK = 0;
						Wireless_SendFlag.bits.Reply_HandShake = 1;
						Wireless_SendFlag.bits.Reply_Language = 1;
						Wireless_SendFlag.bits.Reply_Volume = 1;
						Wireless_SendFlag.bits.Reply_Metronome_Freq = 1;
						Wireless_SendFlag.bits.Reply_WIFI_Setting = 1;
						Wireless_SendFlag.bits.Reply_Sync_Boot_Time = 1;
                        Wireless_SendFlag.bits.Reply_Sync_UTC_Time = 1;
						s_BLE_State = BLE_CONNECTED_STATE;
						s_HandShake_Tick = 0;
                        SEGGER_RTT_printf (0,"BLE Connected\r\n");
						break;
				}

				if(s_HandShake_Tick < WIRELESS_TASK_DELAY) {

					Wireless_SendFlag.bits.Reply_Disconnect = 1;
                    BLE_StateFlag.bits.Disconnect = 1;
					s_HandShake_Tick = 0;
                    osDelay(300);
				}
            }

            if(Comm_Priority == WIFI_PRIORITY) {
                WireLess_Work_State = WIFI_WORK_STATE;
                //WireLess_Work_State = BLE_WORK_STATE;
                s_BLE_State = BLE_INIT_STATE; 
                s_Wifi_State = WIFI_INIT_STATE;       
            }
            break;
        case BLE_CONNECTED_STATE:
            s_Wireless_Data_Recv_Flag = (WireLess_Recv_EnumDef)Wireless_Handle_Data(&Ring_WireLessComm);
            if(s_Wireless_Data_Recv_Flag == wRecv_Data) {
                s_Wireless_Data_Recv_Flag = wRecv_None;
                memcpy(Encrypt_Data,Ring_WireLessComm.RevData+6,Ring_WireLessComm.DataLen);
                my_aes_decrypt(Encrypt_Data,Decrypt_Data,Ring_WireLessComm.DataLen);
				Data_Unpack_Handle(Ring_WireLessComm.RevData[3],Decrypt_Data, Ring_WireLessComm.DataLen);
            }
            else if(s_Wireless_Data_Recv_Flag == wRecv_Puls){
                s_Wireless_Data_Recv_Flag = wRecv_None;
                if(strcmp((const char*)Ring_WireLessComm.RevData, "+QBLESTAT:CONNECTED") == 0) {
                    BLE_StateFlag.bits.Ble_Offline = 0;  
                    s_Offline_Tick = 0;                 
                }
				else if(strcmp((const char*)Ring_WireLessComm.RevData, "+QBLESTAT:DISCONNECTED") == 0) {
                    BLE_StateFlag.bits.Ble_Offline = 1;
                }              
            }
			else if(s_Wireless_Data_Recv_Flag == wRecv_Char) {
                if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") == 0) {
                    Send_Rev_Ok_Flag = 1;                
                }				
                else if(strcmp((const char*)Ring_WireLessComm.RevData, "ERROR") == 0) {
                    Send_Rev_Ok_Flag = 2;
                }
			}
			
			if(BLE_StateFlag.bits.Ble_Offline == 1) {
				s_Offline_Tick += WIRELESS_TASK_DELAY;
				if(s_Offline_Tick >= 3000) {  // 3s
					s_Offline_Tick = 0;
					BLE_StateFlag.bits.Ble_Offline = 0;
					Wireless_SendFlag.bits.Reply_Disconnect = 1;
                    BLE_StateFlag.bits.Disconnect = 1;
				}
			}

            if(Comm_Priority == WIFI_PRIORITY) {
                WireLess_Work_State = WIFI_WORK_STATE;
                //WireLess_Work_State = BLE_WORK_STATE;
                s_BLE_State = BLE_INIT_STATE; 
                s_Wifi_State = WIFI_INIT_STATE;       
            }
            break;
        default:
            break;
    }
	
	if(BLE_StateFlag.bits.Disconnect == 1) {
        BLE_StateFlag.bits.Disconnect = 0;
        Memory_SendState.Upload = UPLOAD_NONE;
        s_BLE_State = BLE_INIT_STATE;
		App_Wireless_Send("AT+QBLEDISCONN\r\n");
        SEGGER_RTT_printf (0,"BLE Disconnected Upload Data\r\n");
        osDelay(300); 
    }
}

/*!
* \brief 无线模块初始化
* \param   none
* \return none
*/
void App_WireLess_Init()
{
    App_Module_Reset();
    
    osDelay(500);
	if(Comm_Priority == BLE_PRIORITY) {
        WireLess_Work_State = BLE_WORK_STATE;
        s_BLE_State = BLE_INIT_STATE;
        s_Wifi_State = WIFI_INIT_STATE;
    }
    else {
        WireLess_Work_State = WIFI_WORK_STATE;
        s_BLE_State = BLE_INIT_STATE;
        s_Wifi_State = WIFI_INIT_STATE;
    }
}

uint8_t WIFI_HTTP_Init()
{
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QHTTPCFG=\"header\",\"Content-Type\",\"application/json\"\r\n",400,wRecv_Char) == wRecv_Char) {
        if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
    }

    return 1;
}

uint8_t WIFI_MQTT_Init()
{
    char atcmd[256] = {0};

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QMTCFG=\"keepalive\",0,60\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QMTCFG=\"recv/mode\",0,1\r\n",400,wRecv_Char) == wRecv_Char) {
		if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
	}

    memset(atcmd,0x00,sizeof(atcmd));
    snprintf(atcmd,sizeof(atcmd),"AT+QMTOPEN=0,\"%s\",%d\r\n",g_NetCfg.mqttServer,g_NetCfg.mqttPort); 
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,atcmd,1000,wRecv_Puls) == wRecv_Puls) {
        if(strcmp((const char*)Ring_WireLessComm.RevData, "+QMTOPEN: 0,0") != 0) {
			return 0;
        }
    }

    return 1;
}

uint8_t MQTT_SignIn()
{
    char atcmd[256] = {0};
    char timestr[16] = {0};
    char md5_pwd[100] = {0};
    memset(atcmd,0x00,sizeof(atcmd));
    
    // 获取毫秒时间戳
    uint32_t timestamp;
    Time_IOControl(TIME_REAL, TIME_GET, &timestamp);
    snprintf(timestr,sizeof(timestr), "%d000", timestamp);
    snprintf(sMqttUserName,sizeof(sMqttUserName), "%s|%s",Factory_Info.Device_SN,timestr);
    snprintf(md5_pwd,sizeof(md5_pwd), "%s|%s",sMqttUserName,Device_Secret);
    MD5_To_32Lower(md5_pwd,sMqttPassword);
	
	snprintf(atcmd,sizeof(atcmd),"AT+QMTCONN=0,\"%s\",\"%s\",\"%s\"\r\n",Factory_Info.Device_SN,sMqttUserName,sMqttPassword);
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,atcmd,400,wRecv_Puls) == wRecv_Puls) {
        if(strcmp((const char*)Ring_WireLessComm.RevData, "+QMTCONN: 0,0,0") != 0) {
            if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QMTDISC=0\r\n",400,wRecv_Char) == wRecv_Char) {
                if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
                    WIFI_MQTT_Init();
                    return 2;
                }
            }
        }
		else {
			return 1;
		}
    }
    return 0;
}

int JSON_Extract_Value(const char *json, const char *key, char *value, int max_len) {
    // 构建键名搜索模式（带引号）
    char key_search[64];
    snprintf(key_search, sizeof(key_search), "%s\"", key);
    
    // 查找键名位置（允许键名前后有空格）
    const char *key_ptr = json;
    while((key_ptr = strstr(key_ptr, key_search)) != NULL) {
        // 检查键名是否完整（前后有冒号或引号）
        if((key_ptr > json && *(key_ptr-1) != '"') || 
           (*(key_ptr + strlen(key_search)) != ':' && 
            *(key_ptr + strlen(key_search)) != '"')) {
            key_ptr++;
            continue;
        }
        
        // 定位值起始位置（跳过键名和可能的空格/冒号）
        const char *val_ptr = key_ptr + strlen(key_search);
        while(*val_ptr && (*val_ptr == ' ' || *val_ptr == ':' || *val_ptr == '\t')) 
            val_ptr++;
        
        // 检查值是否用引号包裹
        if(*val_ptr != '"') {
            // 尝试处理不带引号的值
            const char *val_end = val_ptr;
            while(*val_end && *val_end != ',' && *val_end != '}' && *val_end != ' ' && *val_end != '\t' && *val_end != '\r' && *val_end != '\n')
                val_end++;
            
            int val_len = val_end - val_ptr;
            if(val_len > 0 && val_len < max_len) {
                strncpy(value, val_ptr, val_len);
                value[val_len] = '\0';
                return 0;
            }
            return -1;
        }
        
        // 跳过值起始引号
        val_ptr++;
        
        // 查找值结束引号
        const char *val_end = val_ptr;
        while(*val_end && *val_end != '"') val_end++;
        if(!*val_end) return -1;
        
        // 复制值内容
        int val_len = val_end - val_ptr;
        if(val_len >= max_len) return -1;
        
        strncpy(value, val_ptr, val_len);
        value[val_len] = '\0';
        return 0;
    }
    
    return -1; // 键名未找到
}

uint8_t MQTT_SignUp()
{
    char atcmd[256] = {0};
    char serverUrl[128] = {0};
    char cmdbuf[256] = {0};
    char productSecret_c32[64] = {0};
    char sign_md5_c32[33] = {0};
    char Result[128] = {0};
	char MAC_String[20];
	uint16_t Result_Len;
    uint8_t Json_Data[64];
	uint16_t Json_Code;

    //将mac地址变为stirng
    sprintf(MAC_String,"%02X%02X%02X%02X%02X%02X",MAC_ADRESS[0],MAC_ADRESS[1],MAC_ADRESS[2],MAC_ADRESS[3],MAC_ADRESS[4],MAC_ADRESS[5]);
    snprintf(productSecret_c32,sizeof(productSecret_c32), "%s%s%s%s",Factory_Info.Device_SN,MAC_String,"1234567812345678",g_NetCfg.secret);
    MD5_To_32Upper(productSecret_c32,sign_md5_c32);
    snprintf(atcmd,sizeof(atcmd),"{\"deviceId\":\"%s\",\"moduleId\":\"%s\",\"random\":\"%s\",\"sign\":\"%s\"}",Factory_Info.Device_SN,MAC_String,"1234567812345678",sign_md5_c32);
    snprintf(serverUrl,sizeof(serverUrl), "http://%s:%d/device/secret-key",g_NetCfg.serverName,g_NetCfg.serverPort);

    snprintf(cmdbuf,sizeof(cmdbuf),"AT+QHTTPCFG=\"url\",\"%s\"\r\n",serverUrl); 
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,cmdbuf,1000,wRecv_Char) == wRecv_Char) {
        if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
            return 0;
        }
    }

    memset(cmdbuf,0x00,sizeof(cmdbuf));
    snprintf(cmdbuf,sizeof(cmdbuf),"AT+QHTTPPOST=%d,60,60\r\n",strlen(atcmd));
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,cmdbuf,1000,wRecv_Char) == wRecv_Char) {
        if(strcmp((const char*)Ring_WireLessComm.RevData, "CONNECT") != 0) {
            return 0;
        }
    }
	
	
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,atcmd,1000,wRecv_Puls) == wRecv_Puls) {
		Ring_WireLessComm.RevData[10] = 0;
        if(strcmp((const char*)Ring_WireLessComm.RevData, "+QHTTPPOST") != 0) {
            return 0;
        }
		if((Ring_WireLessComm.RevData[14] == '2')&&(Ring_WireLessComm.RevData[15] == '0')&&(Ring_WireLessComm.RevData[16] == '0'))
        {
            //从Ring_WireLessComm.RevData[18]开始一直读到00 为止
            Result_Len = 0;
            for(int i = 18; i < 25; i++) {
                if(Ring_WireLessComm.RevData[i] == 0x00) {
                    break;
                }
                Result[Result_Len++] = Ring_WireLessComm.RevData[i];
            }
			// 将Result字符串里的数据转换为16进制
            Result_Len = String_To_Hex(Result);      
        }
        else {
            return 0;
        }

    }

    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QHTTPREAD=60\r\n",1000,wRecv_Char) == wRecv_Char) {
		osDelay(100);
        if(strcmp((const char*)Ring_WireLessComm.RevData, "CONNECT") != 0) {
            return 0;
        }
		for (int i = 0; i < Result_Len; i++) {
			Result[i] = ((const char *)Ring_WireLessComm.RevData)[9 + i];
		}
    }
    JSON_Extract_Value(Result, "code", (char *)&Json_Data, sizeof(Result));
	sscanf((const char *)Json_Data, "%d", (int *)&Json_Code);
    if(Json_Code == 200) {
        JSON_Extract_Value(Result, "productId", (char *)&productId, sizeof(Result));
        JSON_Extract_Value(Result, "deviceSecret", (char *)&Device_Secret, sizeof(Result));
        xSemaphoreGive(SaveSecretSemHandle);
        return 1;
    }
    return 0;
    
}

void MQTT_Publish(char *topic, char *payload)
{
    char atcmd[256] = {0};
    uint16_t datalen = strlen(payload);
//    uint16_t u16String_Cnt;
//    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QMTCFG=\"datatype\",0,1\r\n",1000,wRecv_Char) == wRecv_Char) {
//        if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
//            return;
//        }
//    }
    
    snprintf(atcmd,sizeof(atcmd),"AT+QMTPUB=0,0,0,1,\"CPR/%s/%s\",%d,\"%s\"\r\n",Factory_Info.Device_SN, topic, datalen, payload);
     if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,atcmd,1000,wRecv_Char) == wRecv_Char) {
        if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
             return;
        }
     }
//    u16String_Cnt = strlen(atcmd);
//    Wireless_Ring_Send((uint8_t *)atcmd,u16String_Cnt);
   
//    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QMTCFG=\"datatype\",0,0\r\n",1000,wRecv_Char) == wRecv_Char) {
//        if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") != 0) {
//            return;
//        }
//    }
}

bool MQTT_Subscribe(const char *topic,int msgid,int qos)
{
    char atcmd[256] = {0};
    snprintf(atcmd,sizeof(atcmd),"AT+QMTSUB=%d,%d,\"%s/%s%s\",%d\r\n",0,msgid,"CPR",Factory_Info.Device_SN, topic, qos);
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,atcmd,1000,wRecv_Char) == wRecv_Char) {
        if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") == 0) {
            return 1;
        }
        else {
            return 0;
        }
    }
    return 0;
}

bool MQTT_SetRevMode(int mode)
{
    char atcmd[256] = {0};
    snprintf(atcmd,sizeof(atcmd),"AT+QMTCFG=\"recv/mode\",0,%d\r\n",mode);
    if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,atcmd,1000,wRecv_Char) == wRecv_Char) {
        if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") == 0) {
            return 1;
        } else {
            return 0;
        }
    }
    return 0;
}

void MQTT_Send_SN_Number()
{
    char timestr[16] = {0};
	uint32_t timestamp;
    Time_IOControl(TIME_REAL, TIME_GET, &timestamp);
    snprintf(timestr,sizeof(timestr), "%d", timestamp);
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    su8_DataBuffer[u16Data_Cnt++] = 0xFA;
    su8_DataBuffer[u16Data_Cnt++] = 0xFC;
    su8_DataBuffer[u16Data_Cnt++] = 0x01; 
    su8_DataBuffer[u16Data_Cnt++] = E_CMD_DEV_INFO;   // CMD
    u16Data_Cnt += 2; // 预留长度字段位置
    /***************************************** */
    su8_DataBuffer[u16Data_Cnt++] = Factory_Info.Type;
    for(int i = 0; i < 13; i++) {
        su8_DataBuffer[u16Data_Cnt++] = (Factory_Info.Device_SN[i]);
    }
    su8_DataBuffer[u16Data_Cnt++] = 0x01;
    su8_DataBuffer[u16Data_Cnt++] = SoftWare_Version;
    su8_DataBuffer[u16Data_Cnt++] = SoftSub_Version;
    su8_DataBuffer[u16Data_Cnt++] = SoftBuild_Version;
	
    su8_DataBuffer[4] = u16Data_Cnt / 256;
    su8_DataBuffer[5] = u16Data_Cnt % 256; 
	// 计算CRC
    u16CRC_Calc = Crc16Compute(&su8_DataBuffer[3], (u16Data_Cnt - 3)); 
    su8_DataBuffer[u16Data_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = u16CRC_Calc & 0xFF;
	
	for(int i = 0; i < u16Data_Cnt; i++)
    {       
        // 将每个字节转换为两个十六进制字符
        HexChangeChar(su8_DataBuffer[i], &sSendBuffer[u16Send_Cnt], &sSendBuffer[u16Send_Cnt + 1]);
        u16Send_Cnt += 2;
    }
	// 填充长度字段
    u16Send_Cnt = (u16Send_Cnt + 15) & ~15; // 填充到16的倍数
    my_aes_encrypt(sSendBuffer, Encrypt_Data, u16Send_Cnt);
	
    /***************************************** */
	memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
    snprintf((char *)su8_AT_Command,sizeof(su8_AT_Command),"{\"timestamp\":\"%s\",\"messageId\":\"%s\",\"data\":\"",timestr,"1234567812345678");
    u16String_Cnt = strlen((char *)su8_AT_Command);
    for(int i = 0; i < u16Send_Cnt; i++)
    {
        // 检查缓冲区边界，防止溢出
        if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
        
        // 将每个字节转换为两个十六进制字符
        HexChangeChar(Encrypt_Data[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
        u16String_Cnt += 2;
    }
    su8_AT_Command[u16String_Cnt++] = '\"';
    su8_AT_Command[u16String_Cnt++] = '}';
    su8_AT_Command[u16String_Cnt++] = '\r';
    su8_AT_Command[u16String_Cnt++] = '\n'; 
    MQTT_Publish("event/transfer",(char *)su8_AT_Command);
}

void MQTT_Send_CPR_Data()
{
    char timestr[16] = {0};
	uint32_t timestamp;
    Time_IOControl(TIME_REAL, TIME_GET, &timestamp);
    snprintf(timestr,sizeof(timestr), "%d", timestamp);

    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    u16Send_Cnt = 0;
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
	
    su8_DataBuffer[u16Data_Cnt++] = 0xFA;
    su8_DataBuffer[u16Data_Cnt++] = 0xFC;
    su8_DataBuffer[u16Data_Cnt++] = 0x01; 
    su8_DataBuffer[u16Data_Cnt++] = E_CMD_CPR_DATA;   // CMD
    u16Data_Cnt += 2; // 预留长度字段位置
    /***************************************** */ 
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.TimeStamp>>24;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.TimeStamp>>16;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.TimeStamp>>8;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.TimeStamp&0x000000FF;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.Freq >> 8;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.Freq & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.Depth;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.RealseDepth;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.Interval;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.BootStamp>>24;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.BootStamp>>16;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.BootStamp>>8;
    su8_DataBuffer[u16Data_Cnt++] = CPR_Data.BootStamp&0x000000FF;
    /***************************************** */
    su8_DataBuffer[4] = (u16Data_Cnt-6) / 256;
    su8_DataBuffer[5] = (u16Data_Cnt-6) % 256; 
    u16CRC_Calc = Crc16Compute(&su8_DataBuffer[3], (u16Data_Cnt - 3)); 
    su8_DataBuffer[u16Data_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = u16CRC_Calc & 0xFF;
	
	for(int i = 0; i < u16Data_Cnt; i++)
    {       
        // 将每个字节转换为两个十六进制字符
        HexChangeChar(su8_DataBuffer[i], &sSendBuffer[u16Send_Cnt], &sSendBuffer[u16Send_Cnt + 1]);
        u16Send_Cnt += 2;
    }	
    // 填充长度字段
    u16Send_Cnt = (u16Send_Cnt + 15) & ~15; // 填充到16的倍数
    my_aes_encrypt(sSendBuffer, Encrypt_Data, u16Send_Cnt);
    
	memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
    snprintf((char *)su8_AT_Command,sizeof(su8_AT_Command),"{\"timestamp\":\"%s\",\"messageId\":\"%s\",\"data\":\"",timestr,"1234567812345678");
    u16String_Cnt = strlen((char *)su8_AT_Command);
    for(int i = 0; i < u16Send_Cnt; i++)
    {
        // 检查缓冲区边界，防止溢出
        if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
        
        // 将每个字节转换为两个十六进制字符
        HexChangeChar(Encrypt_Data[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
        u16String_Cnt += 2;
    }
    su8_AT_Command[u16String_Cnt++] = '\"';
    su8_AT_Command[u16String_Cnt++] = '}';
    su8_AT_Command[u16String_Cnt++] = '\r';
    su8_AT_Command[u16String_Cnt++] = '\n';
    MQTT_Publish("event/transfer",(char *)su8_AT_Command);
}

void MQTT_Send_SelfCheck_Result()
{
    char timestr[16] = {0};
	uint32_t timestamp;
    Time_IOControl(TIME_REAL, TIME_GET, &timestamp);
    snprintf(timestr,sizeof(timestr), "%d", timestamp);
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    su8_DataBuffer[u16Data_Cnt++] = 0xFA;
    su8_DataBuffer[u16Data_Cnt++] = 0xFC;
    su8_DataBuffer[u16Data_Cnt++] = 0x01; 
    su8_DataBuffer[u16Data_Cnt++] = E_CMD_SELF_CHECK;   // CMD
    u16Data_Cnt += 2; // 预留长度字段位置
    /***************************************** */
    su8_DataBuffer[u16Data_Cnt++] = Self_Check_Data.FeedBack_Self_Check ;
    su8_DataBuffer[u16Data_Cnt++] = Self_Check_Data.Power_Self_Check;
    su8_DataBuffer[u16Data_Cnt++] = Self_Check_Data.Audio_Self_Check;
    su8_DataBuffer[u16Data_Cnt++] = Self_Check_Data.WirelessModlue_Self_Check;
    su8_DataBuffer[u16Data_Cnt++] = Self_Check_Data.Memory_Self_Check;
    su8_DataBuffer[u16Data_Cnt++] = (Self_Check_Data.TimeStamp >> 24) & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = (Self_Check_Data.TimeStamp >> 16) & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = (Self_Check_Data.TimeStamp >> 8) & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = (Self_Check_Data.TimeStamp) & 0xFF;

    su8_DataBuffer[4] = u16Data_Cnt / 256;
    su8_DataBuffer[5] = u16Data_Cnt % 256; 
	// 计算CRC
    u16CRC_Calc = Crc16Compute(&su8_DataBuffer[3], (u16Data_Cnt - 3)); 
    su8_DataBuffer[u16Data_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = u16CRC_Calc & 0xFF;
	
	for(int i = 0; i < u16Data_Cnt; i++)
    {       
        // 将每个字节转换为两个十六进制字符
        HexChangeChar(su8_DataBuffer[i], &sSendBuffer[u16Send_Cnt], &sSendBuffer[u16Send_Cnt + 1]);
        u16Send_Cnt += 2;
    }
	// 填充长度字段
    u16Send_Cnt = (u16Send_Cnt + 15) & ~15; // 填充到16的倍数
    my_aes_encrypt(sSendBuffer, Encrypt_Data, u16Send_Cnt);
	
    /***************************************** */
	memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
    snprintf((char *)su8_AT_Command,sizeof(su8_AT_Command),"{\"timestamp\":\"%s\",\"messageId\":\"%s\",\"data\":\"",timestr,"1234567812345678");
    u16String_Cnt = strlen((char *)su8_AT_Command);
    for(int i = 0; i < u16Send_Cnt; i++)
    {
        // 检查缓冲区边界，防止溢出
        if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
        
        // 将每个字节转换为两个十六进制字符
        HexChangeChar(Encrypt_Data[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
        u16String_Cnt += 2;
    }
    su8_AT_Command[u16String_Cnt++] = '\"';
    su8_AT_Command[u16String_Cnt++] = '}';
    su8_AT_Command[u16String_Cnt++] = '\r';
    su8_AT_Command[u16String_Cnt++] = '\n'; 
    MQTT_Publish("event/transfer",(char *)su8_AT_Command);    
}

void MQTT_Send_Battery()
{
    char timestr[16] = {0};
	uint32_t timestamp;
    Time_IOControl(TIME_REAL, TIME_GET, &timestamp);
    snprintf(timestr,sizeof(timestr), "%d", timestamp);
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    su8_DataBuffer[u16Data_Cnt++] = 0xFA;
    su8_DataBuffer[u16Data_Cnt++] = 0xFC;
    su8_DataBuffer[u16Data_Cnt++] = 0x01; 
    su8_DataBuffer[u16Data_Cnt++] = E_CMD_BATTERY;   // CMD
    u16Data_Cnt += 2; // 预留长度字段位置
    /***************************************** */
    if (osMutexAcquire(Power_MutexHandle, osWaitForever) == osOK) {
        su8_DataBuffer[u16Data_Cnt++] = g_PowerVoltage.BatValue;
        su8_DataBuffer[u16Data_Cnt++] = g_PowerVoltage.BAT >> 8;
        su8_DataBuffer[u16Data_Cnt++] = g_PowerVoltage.BAT & 0xFF;
        su8_DataBuffer[u16Data_Cnt++] = (uint8_t)g_PowerVoltage.State;
        osMutexRelease(Power_MutexHandle);
    }
	
    su8_DataBuffer[4] = u16Data_Cnt / 256;
    su8_DataBuffer[5] = u16Data_Cnt % 256; 
	// 计算CRC
    u16CRC_Calc = Crc16Compute(&su8_DataBuffer[3], (u16Data_Cnt - 3)); 
    su8_DataBuffer[u16Data_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = u16CRC_Calc & 0xFF;
	
	for(int i = 0; i < u16Data_Cnt; i++)
    {       
        // 将每个字节转换为两个十六进制字符
        HexChangeChar(su8_DataBuffer[i], &sSendBuffer[u16Send_Cnt], &sSendBuffer[u16Send_Cnt + 1]);
        u16Send_Cnt += 2;
    }
	// 填充长度字段
    u16Send_Cnt = (u16Send_Cnt + 15) & ~15; // 填充到16的倍数
    my_aes_encrypt(sSendBuffer, Encrypt_Data, u16Send_Cnt);
	
    /***************************************** */
	memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
    snprintf((char *)su8_AT_Command,sizeof(su8_AT_Command),"{\"timestamp\":\"%s\",\"messageId\":\"%s\",\"data\":\"",timestr,"1234567812345678");
    u16String_Cnt = strlen((char *)su8_AT_Command);
    for(int i = 0; i < u16Send_Cnt; i++)
    {
        // 检查缓冲区边界，防止溢出
        if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
        
        // 将每个字节转换为两个十六进制字符
        HexChangeChar(Encrypt_Data[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
        u16String_Cnt += 2;
    }
    su8_AT_Command[u16String_Cnt++] = '\"';
    su8_AT_Command[u16String_Cnt++] = '}';
    su8_AT_Command[u16String_Cnt++] = '\r';
    su8_AT_Command[u16String_Cnt++] = '\n'; 
    MQTT_Publish("event/transfer",(char *)su8_AT_Command);
}

void MQTT_Send_UTC_Time()
{
    char timestr[16] = {0};
	uint32_t timestamp;
    Time_IOControl(TIME_REAL, TIME_GET, &timestamp);
    snprintf(timestr,sizeof(timestr), "%d", timestamp);
    uint16_t u16CRC_Calc=0xFFFF;
    uint16_t u16Send_Cnt=0,u16String_Cnt=0,u16Data_Cnt=0;
    
    memset(su8_DataBuffer,0x00,sizeof(su8_DataBuffer));
    su8_DataBuffer[u16Data_Cnt++] = 0xFA;
    su8_DataBuffer[u16Data_Cnt++] = 0xFC;
    su8_DataBuffer[u16Data_Cnt++] = 0x01; 
    su8_DataBuffer[u16Data_Cnt++] = E_CMD_UTC_SETTING;   // CMD
    u16Data_Cnt += 2; // 预留长度字段位置
    /***************************************** */
    for(int i = 0; i < 6; i++){
        su8_DataBuffer[u16Data_Cnt++] = UTC_Offset_Time[i];
    }	
    su8_DataBuffer[4] = u16Data_Cnt / 256;
    su8_DataBuffer[5] = u16Data_Cnt % 256; 
	// 计算CRC
    u16CRC_Calc = Crc16Compute(&su8_DataBuffer[3], (u16Data_Cnt - 3)); 
    su8_DataBuffer[u16Data_Cnt++] = (u16CRC_Calc >> 8) & 0xFF;
    su8_DataBuffer[u16Data_Cnt++] = u16CRC_Calc & 0xFF;
	
	for(int i = 0; i < u16Data_Cnt; i++)
    {       
        // 将每个字节转换为两个十六进制字符
        HexChangeChar(su8_DataBuffer[i], &sSendBuffer[u16Send_Cnt], &sSendBuffer[u16Send_Cnt + 1]);
        u16Send_Cnt += 2;
    }
	// 填充长度字段
    u16Send_Cnt = (u16Send_Cnt + 15) & ~15; // 填充到16的倍数
    my_aes_encrypt(sSendBuffer, Encrypt_Data, u16Send_Cnt);
	
    /***************************************** */
	memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
    snprintf((char *)su8_AT_Command,sizeof(su8_AT_Command),"{\"timestamp\":\"%s\",\"messageId\":\"%s\",\"data\":\"",timestr,"1234567812345678");
    u16String_Cnt = strlen((char *)su8_AT_Command);
    for(int i = 0; i < u16Send_Cnt; i++)
    {
        // 检查缓冲区边界，防止溢出
        if(u16String_Cnt + 2 >= WIRELESS_MAX_LEN) break;
        
        // 将每个字节转换为两个十六进制字符
        HexChangeChar(Encrypt_Data[i], &su8_AT_Command[u16String_Cnt], &su8_AT_Command[u16String_Cnt + 1]);
        u16String_Cnt += 2;
    }
    su8_AT_Command[u16String_Cnt++] = '\"';
    su8_AT_Command[u16String_Cnt++] = '}';
    su8_AT_Command[u16String_Cnt++] = '\r';
    su8_AT_Command[u16String_Cnt++] = '\n'; 
    MQTT_Publish("event/transfer",(char *)su8_AT_Command);
}

uint8_t* WirelessGetBleMacAdress()
{
    return MAC_ADRESS;
}


void Power_Down_Check()
{
    static uint8_t Power_Off_State = 0;
    static uint32_t Power_Off_Tick = 0;
    switch(Power_Off_State)
    {
        case 0:
            if(Power_State.state == DEV_POWER_OFF)
            {
                Power_Off_State = 1;
                Wireless_SendFlag.bits.Reply_Disconnect = 1;
            }
            break;
        case 1:
            Power_Off_Tick += WIRELESS_TASK_DELAY;
            if(Power_Off_Tick >= 200)  // 
            {
                Power_Off_Tick = 0;
                Power_Off_State = 2;    
            }  
            break;
        case 2:
            App_Module_Reset();
            Power_Off_State = 3;
            Power_State.flag.bits.wireless = 0;
            break;
        case 3:
        default:
            break;
    }
}

static void MQTT_Send_Process()
{
    static uint32_t mqtt_listen_tick = 0;
    static uint16_t wifi_wait_tick = 0;
    
    if(Get_DMA_TX_Status(WirelessComm_Uart) == DMA_TX_BUSY) {
        wifi_wait_tick = 0;
        return;
    }

    wifi_wait_tick += WIRELESS_TASK_DELAY;
    if(wifi_wait_tick < 30) {
        return;
    }
    
    if(Memory_SendState.UpdateStatus == true) {
        Memory_SendState.UpdateStatus = false;
        MQTT_Send_CPR_Upload_Method();
        wifi_wait_tick = 0;
        return;
    }

    if(Memory_SendState.SendBusyFlag == true) {       
        MQTT_Send_Memory_Data();
        wifi_wait_tick = 0;
        return;
    }

    if(Wireless_SendFlag.bits.Reply_Upload_Method) {
        Wireless_SendFlag.bits.Reply_Upload_Method = 0;
        return;
    }

    if(Memory_SendState.Upload == UPLOAD_NONE) 
    {
        mqtt_listen_tick += WIRELESS_TASK_DELAY;

        if(Wireless_SendFlag.bits.Reply_HandShake == 1) {
            Wireless_SendFlag.bits.Reply_HandShake = 0;
            MQTT_Send_SN_Number();	
            return;
        }

        if(osMessageQueueGet(CPR_Data_SendHandle, &CPR_Data, NULL, 0) == osOK) {
            MQTT_Send_CPR_Data();
            return;
        }

        if(mqtt_listen_tick >= 10000) { // 10s
            mqtt_listen_tick = 0;
            // 发送电池电量数据
            MQTT_Send_Battery();
            return;
        }

        if(Wireless_SendFlag.bits.Reply_Sync_UTC_Time == 1) {
            Wireless_SendFlag.bits.Reply_Sync_UTC_Time = 0;
            MQTT_Send_UTC_Time();
            return;
        }
        
        if(Wireless_SendFlag.bits.Reply_SelfCheck_Result == 1) {
            Wireless_SendFlag.bits.Reply_SelfCheck_Result = 0;
            MQTT_Send_SelfCheck_Result();
            return;
        }
    }
}

SelfCheck_Typedef* GetSelfTestData() 
{
    return &Self_Check_Data;
}

// 处理payload数据的函数
void Process_Payload_Data(uint8_t *payload, uint16_t len)
{
    uint8_t cmd = (((payload[6]-0x30)<<4)+(payload[7]-0x30));
    switch(cmd) {
        case E_CMD_UPLOAD_METHOD:
            // 处理上传方式命令
            Memory_SendState.Upload = (CPR_Upload_Method_EnumDef)(((payload[12]-0x30)<<4)+(payload[13]-0x30));
            SEGGER_RTT_printf(0,"MQTT Set Upload Method: %d\r\n",Memory_SendState.Upload);
            break;
        default:
            // 处理其他命令
            break;
    }
}

static unsigned char hex_char_to_byte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0; // 无效字符返回0
}

int hex_string_to_bytes(const char *hex_str, unsigned char *byte_array, int max_len) {
    int hex_len = strlen(hex_str);
    int byte_len = hex_len / 2;
    
    if (hex_len % 2 != 0) {
        return -1;
    }
    
    if (byte_len > max_len) {
        return -1;
    }
    
    for (int i = 0; i < byte_len; i++) {
        char high_nibble = hex_str[2 * i];
        char low_nibble = hex_str[2 * i + 1];
        
        byte_array[i] = (hex_char_to_byte(high_nibble) << 4) | hex_char_to_byte(low_nibble);
    }
    
    return byte_len;
}
static void MQTT_Read_Process(CBuff *Ring_Comm)
{
    uint8_t discard = 0;
    uint16_t Cbuff_Len, i;
    uint8_t is_valid_msg = 0;
    
    // 获取缓冲区长度
    Cbuff_Len = CBuff_GetLength(Ring_Comm);
    if(Cbuff_Len < 4) {
        return;
    }
    
    // 读取数据到临时缓冲区
    CBuff_Read(Ring_Comm, Ring_Comm->RevData, Cbuff_Len);
    
    // 检查消息起始格式
    if(Ring_Comm->RevData[0] == '+' && Ring_Comm->RevData[1] == 'Q' && 
       Ring_Comm->RevData[2] == 'M' && Ring_Comm->RevData[3] == 'T' && 
       Ring_Comm->RevData[4] == 'R' && Ring_Comm->RevData[5] == 'E' &&  
       Ring_Comm->RevData[6] == 'C' && Ring_Comm->RevData[7] == 'V' && 
       Ring_Comm->RevData[8] == ':') {
        
        MQTT_Message_t mqtt_msg = {0};
        uint8_t param_count = 0,quote_count = 0,topic_count = 0;
        uint8_t topic_start_idx = 0, length_idx = 0,payload_idx = 0;
        char length_str[4] = {0};
        
        // 从"+QMTRECV:"后开始解析
        for(i = 9; i < Cbuff_Len; i++) {  // 注意：从11开始，因为"+QMTRECV:"是10个字符
            if(Ring_Comm->RevData[i] == '\"'){
                quote_count++;
                continue;
            } else if(Ring_Comm->RevData[i] == '\r' || Ring_Comm->RevData[i] == '\n') {
                // 遇到换行符，结束解析
                if(quote_count == 4) {
                    mqtt_msg.payload_len = String_To_Hex(length_str);
                    is_valid_msg = 1;
                } else {
                    is_valid_msg = 0;
                }
                CBuff_Pop(Ring_Comm, Ring_Comm->RevData, i);
                break;
            }
            if(quote_count == 1) {
                if(Ring_Comm->RevData[i] == '/') {
                    topic_count++;
                }
                if(topic_count >= 2) {
                    mqtt_msg.topic[topic_start_idx++] = Ring_Comm->RevData[i];
                }
            } else if(quote_count == 2) {
                if(Ring_Comm->RevData[i] == ',') {
                    param_count++;
                    continue;
                }
                if(param_count == 1) {
                    length_str[length_idx++] = Ring_Comm->RevData[i];
                }
            } else if(quote_count == 3) {
                // 解析payload
                mqtt_msg.payload[payload_idx++] = Ring_Comm->RevData[i];
            }
        }
        if(is_valid_msg) {         
            if(strcmp(mqtt_msg.topic, "/function/invoke") == 0) {
                int actual_len = hex_string_to_bytes((char *)mqtt_msg.payload, Encrypt_Data, mqtt_msg.payload_len);
                my_aes_decrypt(Encrypt_Data, Decrypt_Data, actual_len);
                Process_Payload_Data(Decrypt_Data, actual_len);
            }
        } else {
            // 消息不完整，丢弃第一个字节
            CBuff_Pop(Ring_Comm, &discard, 1);
        }
    } else if(Ring_Comm->RevData[0] == 'O' && Ring_Comm->RevData[1] == 'K' && 
       Ring_Comm->RevData[2] == '\r' && Ring_Comm->RevData[3] == '\n') {
        // 处理OK消息
        CBuff_Pop(Ring_Comm, Ring_Comm->RevData, 4);
        Send_Rev_Ok_Flag = 1;   
    }
    else if(Ring_Comm->RevData[0] == 'E' && Ring_Comm->RevData[1] == 'R' && 
            Ring_Comm->RevData[2] == 'R' && Ring_Comm->RevData[3] == 'O'&& 
            Ring_Comm->RevData[4] == 'R' && Ring_Comm->RevData[5] == '\r' && 
            Ring_Comm->RevData[6] == '\n') {
        // 处理ERROR消息
        CBuff_Pop(Ring_Comm, Ring_Comm->RevData, 7);
        Send_Rev_Ok_Flag = 2;
    }
    else if(Ring_Comm->RevData[0] == '+' && Ring_Comm->RevData[1] == 'Q' && 
        Ring_Comm->RevData[2] == 'M' && Ring_Comm->RevData[3] == 'T'){
        if(Ring_Comm->RevData[4] != 'R' && Cbuff_Len >= 5){
            CBuff_Pop(Ring_Comm, &discard, 5);
        }
    }
    else{
        // 不是有效消息，丢弃第一个字节
        for(i = 0; i < Cbuff_Len-2; i++){
            if((Ring_Comm->RevData[i] == '+' && Ring_Comm->RevData[i+1] == 'Q')
            || (Ring_Comm->RevData[i] == 'O' && Ring_Comm->RevData[i+1] == 'K') 
            || (Ring_Comm->RevData[i] == 'E' && Ring_Comm->RevData[i+1] == 'R')) {
                break;
            } else {
                // 不是有效消息，丢弃第一个字节
                CBuff_Pop(Ring_Comm, &discard, 1);
            }
        }  
    }
}

static void MQTT_Listen_Handle()
{
    MQTT_Read_Process(&Ring_WireLessComm);
    MQTT_Send_Process();
} 



/*!
* \brief 无线模块WIFI状态机
* \param   none
* \return none
*/void App_Wifi_FSM()
{
    static uint8_t s_Err_Cnt = 0, ret;
    static uint16_t mqtt_wait_Cnt = 0;
    switch(s_Wifi_State) {
        case WIFI_INIT_STATE:
			App_Module_Reset();
            if(Wifi_Init() == 1) {
                s_Wifi_State = WIFI_CONNECT_AP_STATE;
                s_Err_Cnt = 0;
            }
            break;
        case WIFI_CONNECT_AP_STATE:
            memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
            snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QSTAAPINFO=%s,%s\r\n", Wifi_Module.SSID, Wifi_Module.PWD);
            if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,(char *)su8_AT_Command,400,wRecv_Puls) == wRecv_Puls) {
                if(strcmp((const char*)Ring_WireLessComm.RevData, "+QSTASTAT:WLAN_CONNECTED") == 0) {
                    s_Wifi_State = WIFI_GET_IP_STATE;
                    SEGGER_RTT_printf(0,"Connect AP Success\r\n");
                    s_Err_Cnt = 0;
                    break;
                }
            }
            osDelay(1000);
            // 如果连接失败次数过多则返回ble模式
            s_Err_Cnt++;
            if(s_Err_Cnt >= 5) {
                s_Err_Cnt = 0;
                s_Wifi_State = WIFI_INIT_STATE;
                WireLess_Work_State = BLE_WORK_STATE;
				Comm_Priority = BLE_PRIORITY;
            }
            break;
        case WIFI_GET_IP_STATE:
            for(int i = 0; i <= 3; i++) {
                osDelay(300);
                if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"AT+QGETIP=station\r\n",400,wRecv_Puls) == wRecv_Puls) {
                    Ring_WireLessComm.RevData[7] = 0x00;
                    if(strcmp((const char*)Ring_WireLessComm.RevData, "+QGETIP") == 0) {		
                        if(Ring_WireLessComm.RevData[11] != 0x30) {
#if WAVE_GET_TEST == 0
                            s_Wifi_State = WIFI_INIT_MQTT_STATE;
                            SEGGER_RTT_printf(0,"Get IP Success\r\n");
#else
                            s_Wifi_State = WIFI_CONNECT_TCP_STATE;
#endif
                            s_Err_Cnt = 0;
							ExtractIPFromBuffer(11);
							osDelay(200);
                            break;
                        }
                    }                 
                }
            }
            SEGGER_RTT_printf(0,"Get IP Failed\r\n");
            osDelay(1000);
            // 如果连接失败次数过多则返回ble模式
            s_Err_Cnt++;
            if(s_Err_Cnt >= 5) {
                s_Err_Cnt = 0;
                s_Wifi_State = WIFI_INIT_STATE;
                WireLess_Work_State = BLE_WORK_STATE;
				Comm_Priority = BLE_PRIORITY;
            }
            break;
        case WIFI_CONNECT_TCP_STATE:
            memset(su8_AT_Command,0x00,sizeof(su8_AT_Command));
            snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QIOPEN=0,\"TCP\",\"%s\",%s,2020,2\r\n", Wifi_Module.IP, Wifi_Module.Port);
            if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,(char *)su8_AT_Command,1000,wRecv_Char) == wRecv_Char) {
                if(strcmp((const char*)Ring_WireLessComm.RevData, "CONNECT") == 0) {
                    s_Wifi_State = WIFI_CONNECTED_STATE;
                    s_Err_Cnt = 0;
                    break;
                }
            }
            osDelay(1000);
            // 如果连接失败次数过多则返回ble模式
            s_Err_Cnt++;
            if(s_Err_Cnt >= 5) {
                s_Err_Cnt = 0;
                s_Wifi_State = WIFI_INIT_STATE;
                WireLess_Work_State = BLE_WORK_STATE;
				Comm_Priority = BLE_PRIORITY;
            }
            break;
        case WIFI_CONNECTED_STATE:
            s_Wireless_Data_Recv_Flag = (WireLess_Recv_EnumDef)Wireless_Handle_Data(&Ring_WireLessComm);
			if(s_Wireless_Data_Recv_Flag == wRecv_Data){
				memcpy(Encrypt_Data,Ring_WireLessComm.RevData+6,Ring_WireLessComm.DataLen);
                my_aes_decrypt(Encrypt_Data,Decrypt_Data,Ring_WireLessComm.DataLen);
                Data_Unpack_Handle(Ring_WireLessComm.RevData[3],Decrypt_Data, Ring_WireLessComm.DataLen);
            }
            else if(s_Wireless_Data_Recv_Flag == wRecv_Char){
                if(strcmp((const char*)Ring_WireLessComm.RevData, "NO CARRIER") == 0) {
                    s_Wifi_State = WIFI_CONNECT_TCP_STATE;
                    break;
                }
            }
            if(Comm_Priority == BLE_PRIORITY) {
                if(WireLess_Modlue_AT_Check(&Ring_WireLessComm,"+++",400,wRecv_Char) == wRecv_Char) {
                    if(strcmp((const char*)Ring_WireLessComm.RevData, "OK") == 0) {
                        //WireLess_Work_State = WIFI_WORK_STATE;
                        WireLess_Work_State = BLE_WORK_STATE;
                        s_BLE_State = BLE_INIT_STATE; 
                        s_Wifi_State = WIFI_INIT_STATE;
                        break;
                    }
                }
            }
            break; 
        case WIFI_IDLE_STATE:
            break;
        case WIFI_INIT_MQTT_STATE:
            if(WIFI_MQTT_Init() == 1) {
                s_Wifi_State = WIFI_INIT_HTTP_STATE;
                SEGGER_RTT_printf(0,"MQTT Init Success\r\n");
                s_Err_Cnt = 0;
            }
            osDelay(1000);
            // 如果连接失败次数过多则返回ble模式
            s_Err_Cnt++;
            if(s_Err_Cnt >= 5) {
                s_Err_Cnt = 0;
                s_Wifi_State = WIFI_INIT_STATE;
                WireLess_Work_State = BLE_WORK_STATE;
				Comm_Priority = BLE_PRIORITY;
            }
            break; 
        case WIFI_INIT_HTTP_STATE:
            if(WIFI_HTTP_Init() == 1) {
                s_Wifi_State = MQTT_SIGNIN_STATE;
                s_Err_Cnt = 0;
            }
            osDelay(1000);
            // 如果连接失败次数过多则返回ble模式
            s_Err_Cnt++;
            if(s_Err_Cnt >= 5) {
                s_Err_Cnt = 0;
                s_Wifi_State = WIFI_INIT_STATE;
                WireLess_Work_State = BLE_WORK_STATE;
				Comm_Priority = BLE_PRIORITY;
            }
            break;
        case MQTT_SIGNIN_STATE:
            ret = MQTT_SignIn();
            if(ret == 1 ){
                s_Wifi_State = MQTT_RECVMODE_STATE;
                SEGGER_RTT_printf(0,"MQTT SignIn Success\r\n");
                s_Err_Cnt = 0;
            }
            else if(ret == 2) {
                s_Wifi_State = MQTT_SIGNUP_STATE;
                SEGGER_RTT_printf(0,"MQTT SignIn Failed, Need SignUp\r\n");
            }
            break;
        case MQTT_SIGNUP_STATE:
            if(MQTT_SignUp() == 1) {
                s_Wifi_State = MQTT_RECVMODE_STATE;
                SEGGER_RTT_printf(0,"MQTT SignUp Success\r\n");
                s_Err_Cnt = 0;
            }
            break;
        case MQTT_RECVMODE_STATE:
            if(MQTT_SetRevMode(0) == 1) {
                s_Wifi_State = MQTT_SUBSCRIBE_STATE;
                aesInfo.key = (const void *)Device_Secret;
	            my_aes_init();	
                s_Err_Cnt = 0;
            }
            break;
        case MQTT_SUBSCRIBE_STATE:
            if(MQTT_Subscribe("/function/invoke",2,2) == 1) {
                s_Wifi_State = MQTT_WAIT_STATE;
                s_Err_Cnt = 0;
            }
            break;
        case MQTT_WAIT_STATE:
            mqtt_wait_Cnt += WIRELESS_TASK_DELAY;
            if(mqtt_wait_Cnt >= 200) {
                mqtt_wait_Cnt = 0;
                s_Wifi_State = MQTT_LISTEN_STATE;
                Wireless_SendFlag.bits.Reply_Sync_UTC_Time = 1;
                Wireless_SendFlag.bits.Reply_SelfCheck_Result = 1;
                Wireless_SendFlag.bits.Reply_HandShake = 1;
                SEGGER_RTT_printf(0,"MQTT Connected\r\n");
            }
            break;
        case MQTT_LISTEN_STATE:
            MQTT_Listen_Handle();
            break;
        default:
            break;
    }
}

/*!
* \brief    无线模块处理
* \param   none
* \return none
*/
void WireLess_Process()
{
    Power_Down_Check();
    Dev_Info_Save_Check();
    switch(WireLess_Work_State)
    {
        case BLE_WORK_STATE:
            App_Ble_FSM();
            Wireless_Send_Handle();
            break;
        case WIFI_WORK_STATE:
            App_Wifi_FSM();
            break;
        default:
            break;
    }
    BLE_Ring_Send_Handle();	
}

/**************************End of file********************************/
