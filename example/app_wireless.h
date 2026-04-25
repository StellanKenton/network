/**
* Copyright (c) 2023, AstroCeta, Inc. All rights reserved.
* \file app_Wireless.h
* \brief Implementation of a ring buffer for efficient data handling.
* \date 2025-07-30
* \author AstroCeta, Inc.
**/
#ifndef APP_WIRELESS_H
#define APP_WIRELESS_H

#include <string.h>
#include <stdbool.h>
#include "stdint.h"
#include "MD5.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif
#include "lib_ringbuffer.h"
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "app_system.h"
#include "usart.h"
#include "lib_comm.h"
#include "net_config.h"
#include "Content.h"

#define BLE_CR  0x0D    // 或 '\r'
#define BLE_LF  0x0A    // 或 '\n'

#define WIRELESS_TASK_DELAY 5
#define WIRELESS_MAX_LEN 512

#define WAVE_GET_TEST 0

typedef enum {
    DMA_TX_IDLE,
    DMA_TX_BUSY,
    DMA_TX_COMPLETE,
    DMA_TX_ERROR
} DMA_TX_Status;

typedef struct 
{
    uint8_t SSID[32];
    uint8_t PWD[32];
    uint8_t IP[16];
    uint8_t Port[6];
    uint8_t MAC[18];
    uint8_t Dev_IP[16];
    uint8_t RSSI;
}Wifi_Module_Typedef;


typedef enum
{
    wRecv_None = 0x00,
    wRecv_Data = 0x01,
    wRecv_Char = 0x02,
    wRecv_Puls = 0x03,
}WireLess_Recv_EnumDef;

typedef enum
{
    BLE_INIT_START,
    BLE_INIT_CLOSE_STA,
    BLE_INIT_BLE_INIT,
    BLE_INIT_BLENAME,
    BLE_INIT_SET_SERV,
    BLE_INIT_SET_CHAR1,
    BLE_INIT_SET_CHAR2,
    BLE_INIT_SET_AD_PARA,
    BLE_INIT_SET_AD1,
    BLE_INIT_GET_MAC,
    BLE_INIT_SET_AD2,
    BLE_INIT_GET_VER,
    BLE_INIT_START_ADV,
    BLE_INIT_DONE,
}BLE_INIT_EnumDef;

typedef enum
{
	BLE_INIT_STATE,
    BLE_WAITING_STATE,
    BLE_CONNECTED_STATE,
    BLE_DISCONNECT_STATE,
    BLE_IDLE_STATE,
    BLE_STATE_MAX,
}BLE_FSM_EnumDef;

typedef enum
{
	WIFI_INIT_STATE,
    WIFI_CONNECT_AP_STATE,
    WIFI_GET_IP_STATE,
    WIFI_CONNECT_TCP_STATE,
    WIFI_DISCONNECT_STATE,
    WIFI_CONNECTED_STATE,
    WIFI_IDLE_STATE,
    WIFI_INIT_MQTT_STATE,
    WIFI_INIT_HTTP_STATE,  
    MQTT_SIGNIN_STATE,
    MQTT_SIGNUP_STATE,
    MQTT_RECVMODE_STATE,
    MQTT_SUBSCRIBE_STATE,
    MQTT_WAIT_STATE, 
    MQTT_LISTEN_STATE, 
    WIFI_STATE_MAX,
}WIFI_FSM_EnumDef;

typedef enum
{
    BLE_PRIORITY  = 0,
	WIFI_PRIORITY = 1,

}Comm_Priority_EnumDef;

typedef enum
{
    Ble_Send_Waiting  = 0,
	Ble_Handle_Data,
    Ble_DMA_Send,
    Ble_Send_MAX,
}Ble_Send_State_enum;

typedef enum
{
    BLE_WORK_STATE,
    WIFI_WORK_STATE,
}WIRELESS_WORK_EnumDef;

typedef union {
    uint32_t byte;
    struct {
        uint32_t Reply_HandShake : 1;
        uint32_t Reply_Disconnect : 1;
        uint32_t Reply_Dev_Info : 1;
        uint32_t Reply_BLE_Info : 1;
        uint32_t Reply_WIFI_Setting : 1;
        uint32_t Reply_COM_Setting : 1;
        uint32_t Reply_Upload_Method : 1;
        uint32_t Reply_CPR_Data : 1;

        uint32_t Reply_Time_Sync : 1;
        uint32_t Reply_Battery : 1;
        uint32_t Reply_Volume : 1;
        uint32_t Reply_HeartBeat : 1;
        uint32_t Reply_TCP_Setting : 1;
        uint32_t Reply_Language : 1;
        uint32_t Reply_SelfCheck_Result : 1;
        uint32_t Reply_Clear_Result : 1;

        uint32_t Reply_Metronome_Freq : 1; 
        uint32_t Reply_Sync_Boot_Time : 1;
        uint32_t Reply_Sync_UTC_Time  : 1;
    } bits;
} Ble_Send_ByteUnion;


typedef struct {
    char topic[100];    // 完整的功能路径
    uint8_t payload[100];       // 数据包内容
    uint16_t payload_len;       // 数据包实际长度
} MQTT_Message_t;

typedef union {
    uint16_t byte;  
    struct {
        uint16_t HandShake_OK : 1;
        uint16_t Ble_Offline : 1;
        uint16_t Disconnect : 1;
        uint16_t bit3 : 1;
        uint16_t bit4 : 1;
        uint16_t bit5 : 1;
        uint16_t bit6 : 1;
        uint16_t bit7 : 1;
    } bits;
} Ble_State_ByteUnion;

typedef struct 
{
    uint8_t type;
	uint8_t  Charge_Status;
    uint8_t  DC_Voltage;
    uint8_t  BAT_Voltage;
    uint8_t  V5_Voltage;
    uint8_t  V33_Voltage;
    uint8_t  Ble_State;
    uint8_t  Wifi_State;
	uint32_t TimeStamp;
    uint8_t  CPR_State;
    uint8_t  Reserve;
    uint16_t CheckSum;
}LogUpdate_Typedef;


typedef struct 
{
    ENUM_MEM_DATA_TYPE type;
    uint8_t  FeedBack_Self_Check;
    uint8_t  Power_Self_Check;
    uint8_t  Audio_Self_Check;
    uint8_t  WirelessModlue_Self_Check;
    uint8_t  Memory_Self_Check;
    uint8_t  Reserve1[2];
    uint32_t TimeStamp;
    uint8_t  Reserve2[2];
    uint16_t CheckSum;
    
}SelfCheck_Typedef;

void App_WireLess_Init(void);
void WireLess_Process(void);
void WirelessComm_DMA_Recive(void);
uint8_t Wireless_Get_Connect_Status(void);
void MQTT_Publish(char *topic, char *payload);
uint8_t* WirelessGetBleMacAdress(void);
SelfCheck_Typedef* GetSelfTestData(void);
BLE_FSM_EnumDef* Wireless_Get_State(void);
#ifdef __cplusplus
}
#endif
#endif  // APP_WIRELESS_H@
/**************************End of file********************************/

