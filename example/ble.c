/*
 * bleprotocol.cpp
 *
 *  Created on: 2023/04/04
 *      Author: svnadministrator
 */

#include "stdio.h"
#include <includes.h>
#include "encryption.h"
#include "lwrb.h"
#include "bleprotocol.h"
#include "wifidevice.h"
#include "wireless_driver.h"
#include "MD5.h"
#include "wireless_driver.h"
#include "Iot_Esp32C5.h"
#include "Iot_Manager.h"
#include "Iot_ProtocolDataTrans.h"

#define BLE_IOT_UPLOAD_INTERVAL_MS 5000U
#define BLE_IOT_UPLOAD_INTERVAL_TICKS ((BLE_IOT_UPLOAD_INTERVAL_MS + BLE_TASK_RUN_PERIOD - 1U) / BLE_TASK_RUN_PERIOD)

BleRunStateInfoDef BleCtrlInfo;
static uint8_t sringSendBuffer[BLE_MAX_DATA_LEN];
static lwrb_t sSendRing_Comm;

static uint8_t Ble_MapIotCmdToBleCmd(IOT_PROTOCOL_KEY_E cmd);
static bool BleAESEncryptInitWithMacBytes(const uint8_t *macBytes, uint8_t macLen);
static uint16_t Ble_BuildEncryptedPacket(uint8_t cmd, const uint8_t *plain, uint16_t plain_len, uint8_t *packet, uint16_t packet_size);
static void Ble_Send_IotStatusPacket(uint8_t cmd, const uint8_t *plain, uint16_t plain_len);
static void Ble_Send_IotRunMode(const IotTxDataTransDef *iotTxData);
static void Ble_Send_IotBatteryState(const IotTxDataTransDef *iotTxData);
static void Ble_Send_IotCommState(const IotTxDataTransDef *iotTxData);
static void Ble_Send_IotDeviceInfo(void);
static void Ble_ProcessPeriodicIotUpload(void);

static uint16_t Ble_Build_IotRunModePacket(uint8_t *packet, uint16_t packet_size, const IotTxDataTransDef *iotTxData);
static uint16_t Ble_Build_IotBatteryStatePacket(uint8_t *packet, uint16_t packet_size, const IotTxDataTransDef *iotTxData);
static uint16_t Ble_Build_IotCommStatePacket(uint8_t *packet, uint16_t packet_size, const IotTxDataTransDef *iotTxData);
static uint16_t Ble_Build_IotDeviceInfoPacket(uint8_t *packet, uint16_t packet_size);


uint16_t BleProtocolCrc16Compute(const uint8_t *data, uint16_t length) 
{
    uint16_t crc = 0x0000;
    
    while (length--) {
        uint8_t b = *data++;
        
        // 输入反转（使用循环，节省代码空间）
        uint8_t r = 0;
        for (uint8_t i = 0; i < 8; i++) {
            r = (r << 1) | (b & 0x01);
            b >>= 1;
        }
        
        crc ^= (uint16_t)r << 8;
        
        // 处理8位
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x8005;
            } else {
                crc <<= 1;
            }
        }
    }
    
    // 输出反转（使用循环）
    uint16_t result = 0;
    for (uint8_t i = 0; i < 16; i++) {
        result = (result << 1) | (crc & 0x01);
        crc >>= 1;
    }
    
    return result;
}


/* Parse MAC string like "AA:BB:CC:DD:EE:FF" or "AABBCCDDEEFF" into 6 bytes.
   Returns true on success. */
static bool BleParseMacString(const char* s, uint8_t out_mac[6])
{
    if(s == NULL || out_mac == NULL) return false;

    uint8_t idx = 0;
    uint8_t have_high = 0;
    uint8_t byte = 0;

    for(; *s != '\0' && idx < 6; s++) {
        char c = *s;

        /* Skip common separators */
        if(c == ':' || c == '-' || c == ' ') {
            continue;
        }

        uint8_t v;
        if(c >= '0' && c <= '9') v = (uint8_t)(c - '0');
        else if(c >= 'a' && c <= 'f') v = (uint8_t)(c - 'a' + 10);
        else if(c >= 'A' && c <= 'F') v = (uint8_t)(c - 'A' + 10);
        else return false;

        if(!have_high) {
            byte = (uint8_t)(v << 4);
            have_high = 1;
        } else {
            byte |= v;
            out_mac[idx++] = byte;
            have_high = 0;
            byte = 0;
        }
    }

    /* Must end on nibble boundary and have 6 bytes */
    if(have_high) return false;
    return (idx == 6) ? true : false;
}

static bool BleAESEncryptInitWithMacBytes(const uint8_t *macBytes, uint8_t macLen)
{
    static uint8_t macMd5Key[16];

    if((macBytes == NULL) || (macLen == 0U)) {
        return false;
    }

    MD5_Data((uint8_t *)macBytes, macLen, macMd5Key);
    GetAesInfoPtr()->key = macMd5Key;
    my_aes_init();
    return true;
}

bool BleAESEncryptInit(void)
{
    char* ble_mac = NULL;
    uint8_t MAC[12] = {0};
    ble_mac = WifiGetMACAddr();

    if(ble_mac != NULL)
    {
        /* Convert ble_mac string to hex bytes into MAC[] */
        if(!BleParseMacString(ble_mac, MAC)) {
            return false;
        }
        for(int i = 0; i < 6; i++) {
            BleCtrlInfo.MAC[i] = MAC[i];
        }
        return BleAESEncryptInitWithMacBytes(MAC, 6U);
    }
    return false;
}

bool BleAESEncryptInitByMacBytes(const uint8_t mac[6])
{
    return BleAESEncryptInitWithMacBytes(mac, 6U);
}

void BleAESEncrypt(const uint8_t* input, uint8_t* output, uint16_t length)
{
    // 如果lte也需要AES加密，则需要每次都初始化，不然会冲突
    // if LTE also needs AES encryption, it needs to be initialized each time, otherwise it will conflict
    //BleAESEncryptInit();  
    my_aes_encrypt((unsigned char *)input, output, length);
}

void BleAESDecrypt(const uint8_t* input, uint8_t* output, uint16_t length)
{
    // 如果lte也需要AES加密，则需要每次都初始化，不然会冲突
    // if LTE also needs AES encryption, it needs to be initialized each time, otherwise it will conflict
    //BleAESEncryptInit();
    my_aes_decrypt((unsigned char *)input, output, length);
}


void BleProtocolStateInit(void)
{
    BleCtrlInfo.conn_timeout = 0;
}


uint8_t Wireless_Handle_Data(lwrb_t *Ring_Comm)
{
    uint8_t ret = 0;
	uint16_t Cbuff_Len;
    uint16_t u16CRC_Sum=0,u16CRC_Calc=0xFFFF;

    Cbuff_Len = lwrb_get_linear_block_read_length(Ring_Comm);

    if(BleCtrlInfo.SendState == wSend_Data) {
        lwrb_peek(Ring_Comm,0,BleCtrlInfo.RevData,Cbuff_Len);
        for(int i = 0; i < Cbuff_Len; i++) {
            if(BleCtrlInfo.RevData[i] == '>') {
                BleCtrlInfo.flag.bits.ReciveSendFlag = 1;
                break;
            }
        }
    }

    if(Cbuff_Len < 4){
		return ret;
	}
    else if(Cbuff_Len > BLE_MAX_PACK_SIZE) {
        lwrb_peek(Ring_Comm,0,BleCtrlInfo.RevData,BLE_MAX_PACK_SIZE);
        Cbuff_Len = BLE_MAX_PACK_SIZE;
    }
    lwrb_peek(Ring_Comm,0,BleCtrlInfo.RevData,Cbuff_Len);
    if(BleCtrlInfo.RevData[0] == 0xFA && BleCtrlInfo.RevData[1] == 0xFC)  
    {
        // Ring_WireLessComm.DataLen = (BleCtrlInfo.RevData[4])*256+BleCtrlInfo.RevData[5];
        // // 蓝牙工作模式才按加密的处理     
		// if(WireLess_Work_State == BLE_WORK_STATE) {			
		// 	Ring_WireLessComm.DataLen = (Ring_WireLessComm.DataLen + 15) & ~15; // 取16的倍数
		// }    
        // BleCtrlInfo.RevIndex = Ring_WireLessComm.DataLen+8; //CRC16
		BleCtrlInfo.DecryptIndex = (BleCtrlInfo.RevData[4])*256+BleCtrlInfo.RevData[5];
        BleCtrlInfo.RevIndex = (BleCtrlInfo.RevData[4])*256+BleCtrlInfo.RevData[5]+8; //CRC16
        if(BleCtrlInfo.RevIndex > BLE_MAX_PACK_SIZE)
        {
            lwrb_read(Ring_Comm,BleCtrlInfo.RevData,1); 	// detect if the length over the range
            return ret;
        }
        else if(lwrb_get_linear_block_read_length(Ring_Comm) < BleCtrlInfo.RevIndex)
        {
            BleCtrlInfo.OverTime+=BLE_TASK_RUN_PERIOD;
            if(BleCtrlInfo.OverTime >= BLE_REV_TIMEOUT)   // 5ms*20 = 100ms
            {
                BleCtrlInfo.OverTime = 0;
                lwrb_read(Ring_Comm,BleCtrlInfo.RevData,1);				// 100ms don't receive the rest of the data, remove header and return
            }
            return ret;
        }
        else
        {
            BleCtrlInfo.OverTime = 0;
            lwrb_peek(Ring_Comm,0,BleCtrlInfo.RevData,BleCtrlInfo.RevIndex);			// Pull out all the data   
			u16CRC_Calc = BleProtocolCrc16Compute(BleCtrlInfo.RevData+3,(BleCtrlInfo.RevIndex-5)); 
            u16CRC_Sum = BleCtrlInfo.RevData[BleCtrlInfo.RevIndex-2]*256 + BleCtrlInfo.RevData[BleCtrlInfo.RevIndex-1];			
            if(u16CRC_Calc == u16CRC_Sum) {  	
                lwrb_read(Ring_Comm,BleCtrlInfo.RevData,BleCtrlInfo.RevIndex);
                //Ble_Data_Unpack(BleCtrlInfo.RevData);
                ret = wRecv_Data;
                return ret;
            }
			else
			{
				lwrb_read(Ring_Comm,BleCtrlInfo.RevData,1);
			}
            return ret; 
        }
    }
    else if(((BleCtrlInfo.RevData[0] == '+')&&(BleCtrlInfo.RevData[1] == 'B'))
        ||((BleCtrlInfo.RevData[0] == 'O')&&(BleCtrlInfo.RevData[1] == 'K'))
        ||((BleCtrlInfo.RevData[0] == 'E')&&(BleCtrlInfo.RevData[1] == 'R'))
        ||((BleCtrlInfo.RevData[0] == 'C')&&(BleCtrlInfo.RevData[1] == 'N'))
        ||((BleCtrlInfo.RevData[0] == 'N')&&(BleCtrlInfo.RevData[1] == 'O')))
    {
        Cbuff_Len = lwrb_get_linear_block_read_length(Ring_Comm);
        if(Cbuff_Len >= 256) {
            lwrb_peek(Ring_Comm,0,BleCtrlInfo.RevData,256);
        }
        else {
            lwrb_peek(Ring_Comm,0,BleCtrlInfo.RevData,Cbuff_Len);
        }
        for(int i = 0; i <= Cbuff_Len; i++) {
            if(BleCtrlInfo.RevData[i] == BLE_CR) {
                if(BleCtrlInfo.RevData[i+1] == BLE_LF) {
                    BleCtrlInfo.OverTime = 0;
                    lwrb_read(Ring_Comm,BleCtrlInfo.RevData,i+2);
                    BleCtrlInfo.RevData[i] = 0x00;
                    if((BleCtrlInfo.RevData[0] == 'O')&&(BleCtrlInfo.RevData[1] == 'K')) {
                        BleCtrlInfo.flag.bits.ReciveOkFlag = 1;
                    } else if(BleCtrlInfo.RevData[0] != '+') {
                        ret = wRecv_Char;
                    } else{
                        ret = wRecv_Puls;
                    }
                    
                    return ret;
                }
            }
        }
        if(BleCtrlInfo.OverTime >= 100) {
            BleCtrlInfo.OverTime = 0;
            lwrb_read(Ring_Comm,BleCtrlInfo.RevData,1);
        }
        BleCtrlInfo.OverTime++;
    }
    else {
        lwrb_read(Ring_Comm,BleCtrlInfo.RevData,1);
        for(int i = 1; i < Cbuff_Len; i++) {
            if((BleCtrlInfo.RevData[i] == 0xFA )
            ||(BleCtrlInfo.RevData[i] == '+')
			||(BleCtrlInfo.RevData[i] == 'O')
            ||(BleCtrlInfo.RevData[i] == 'E')
			||(BleCtrlInfo.RevData[i] == 'C')
            ||(BleCtrlInfo.RevData[i] == 'N')
            ||(BleCtrlInfo.RevData[i] == '>')){
                break;
            }
			else {
				lwrb_read(Ring_Comm,BleCtrlInfo.RevData,1);
			}
        }
		return ret;
	}
	return ret;
}


void HexChangeChar(uint8_t val, uint8_t *out1, uint8_t *out2)
{
    // 将高4位转换为十六进制字符
    uint8_t highNibble = (val >> 4) & 0x0F;
    *out1 = (highNibble < 10) ? ('0' + highNibble) : ('A' + highNibble - 10);
    
    // 将低4位转换为十六进制字符
    uint8_t lowNibble = val & 0x0F;
    *out2 = (lowNibble < 10) ? ('0' + lowNibble) : ('A' + lowNibble - 10);
}

void Ble_PutData_To_Ring(const uint8_t* data, uint16_t length)
{
    lwrb_write(&sSendRing_Comm, (uint8_t*)&length, 2);
    lwrb_write(&sSendRing_Comm, data, length);
}

static uint16_t Ble_BuildEncryptedPacket(uint8_t cmd, const uint8_t *plain, uint16_t plain_len, uint8_t *packet, uint16_t packet_size)
{
    uint8_t encryptBuffer[128];
    uint16_t crcCalc = 0xFFFF;
    uint16_t sendCnt = 0;
    uint16_t encLen = (uint16_t)((plain_len + 15U) & (uint16_t)~15U);

    if(packet == NULL) {
        return 0U;
    }
    if(encLen > sizeof(encryptBuffer)) {
        return 0U;
    }
    if((uint16_t)(6U + encLen + 2U) > packet_size) {
        return 0U;
    }

    memset(encryptBuffer, 0x00, sizeof(encryptBuffer));
    if((plain != NULL) && (plain_len > 0U)) {
        memcpy(encryptBuffer, plain, plain_len);
    }

    my_aes_encrypt(encryptBuffer, BleCtrlInfo.Encrypt_Data, encLen);

    packet[sendCnt++] = 0xFA;
    packet[sendCnt++] = 0xFC;
    packet[sendCnt++] = 0x01;
    packet[sendCnt++] = cmd;
    packet[sendCnt++] = (uint8_t)((encLen >> 8) & 0xFF);
    packet[sendCnt++] = (uint8_t)(encLen & 0xFF);
    memcpy(&packet[sendCnt], BleCtrlInfo.Encrypt_Data, encLen);
    sendCnt = (uint16_t)(sendCnt + encLen);

    crcCalc = BleProtocolCrc16Compute(&packet[3], (uint16_t)(sendCnt - 3U));
    packet[sendCnt++] = (uint8_t)((crcCalc >> 8) & 0xFF);
    packet[sendCnt++] = (uint8_t)(crcCalc & 0xFF);

    return sendCnt;
}


static void Ble_Send_EncryptedPacket(uint8_t cmd, const uint8_t *plain, uint16_t plain_len)
{
    static uint8_t su8_AT_Command[256];
    uint8_t sSendBuffer[128];
    uint16_t u16Send_Cnt = 0, u16String_Cnt = 0;

    u16Send_Cnt = Ble_BuildEncryptedPacket(cmd, plain, plain_len, sSendBuffer, (uint16_t)sizeof(sSendBuffer));
    if(u16Send_Cnt == 0U) {
        return;
    }

    /* To AT command hex string */
    //snprintf((char *)su8_AT_Command, sizeof(su8_AT_Command), "AT+QBLEGATTSNTFY=fe62,%d,", u16Send_Cnt);
    //u16String_Cnt = (uint16_t)strlen((char *)su8_AT_Command);
    u16String_Cnt = 0;
    for(int i = 0; i < u16Send_Cnt; i++)
    {
        HexChangeChar(sSendBuffer[i], &su8_AT_Command[2*i], &su8_AT_Command[2*i + 1]);
        u16String_Cnt += 2;
    }

    // su8_AT_Command[u16String_Cnt++] = '\r';
    // su8_AT_Command[u16String_Cnt++] = '\n';
    // Send_Data_To_Wireless(su8_AT_Command, u16String_Cnt);
    Ble_PutData_To_Ring(su8_AT_Command, u16String_Cnt);
}

static uint8_t Ble_MapIotCmdToBleCmd(IOT_PROTOCOL_KEY_E cmd)
{
    uint8_t highNibble = (uint8_t)(((uint16_t)cmd / 100U) & 0x0FU);
    uint8_t lowNibble = (uint8_t)((uint16_t)cmd % 10U);

    return (uint8_t)((highNibble << 4) | lowNibble);
}

static void Ble_Send_IotStatusPacket(uint8_t cmd, const uint8_t *plain, uint16_t plain_len)
{
    static uint8_t bleAtCommand[256];
    uint8_t sendBuffer[128];
    uint16_t sendCnt = 0;
    uint16_t stringCnt = 0;

    sendCnt = Ble_BuildEncryptedPacket(cmd, plain, plain_len, sendBuffer, (uint16_t)sizeof(sendBuffer));
    if(sendCnt == 0U) {
        return;
    }

    for(uint16_t i = 0; i < sendCnt; i++) {
        HexChangeChar(sendBuffer[i], &bleAtCommand[2U * i], &bleAtCommand[2U * i + 1U]);
        stringCnt = (uint16_t)(stringCnt + 2U);
    }

    Ble_PutData_To_Ring(bleAtCommand, stringCnt);
}

static void Ble_Send_IotRunMode(const IotTxDataTransDef *iotTxData)
{
    uint8_t payload[1];

    if(iotTxData == NULL) {
        return;
    }

    payload[0] = (uint8_t)iotTxData->Run.RunMode;
    Ble_Send_IotStatusPacket(Ble_MapIotCmdToBleCmd(IOT_PROTOCOL_KEY_RUN_MODE), payload, (uint16_t)sizeof(payload));
}

static uint16_t Ble_Build_IotRunModePacket(uint8_t *packet, uint16_t packet_size, const IotTxDataTransDef *iotTxData)
{
    uint8_t payload[1];

    if(iotTxData == NULL) {
        return 0U;
    }

    payload[0] = (uint8_t)iotTxData->Run.RunMode;
    return Ble_BuildEncryptedPacket(Ble_MapIotCmdToBleCmd(IOT_PROTOCOL_KEY_RUN_MODE), payload, (uint16_t)sizeof(payload), packet, packet_size);
}

static void Ble_Send_IotBatteryState(const IotTxDataTransDef *iotTxData)
{
    uint8_t payload[5];

    if(iotTxData == NULL) {
        return;
    }

    payload[0] = iotTxData->Battery.BatteryIndex;
    payload[1] = iotTxData->Battery.Bars;
    payload[2] = (uint8_t)(iotTxData->Battery.Percentage & 0xFF);
    payload[3] = (uint8_t)((iotTxData->Battery.Percentage >> 8) & 0xFF);
    payload[4] = (uint8_t)iotTxData->Battery.Status;
    Ble_Send_IotStatusPacket(Ble_MapIotCmdToBleCmd(IOT_PROTOCOL_KEY_BATTERY_STATE), payload, (uint16_t)sizeof(payload));
}

static uint16_t Ble_Build_IotBatteryStatePacket(uint8_t *packet, uint16_t packet_size, const IotTxDataTransDef *iotTxData)
{
    uint8_t payload[5];

    if(iotTxData == NULL) {
        return 0U;
    }

    payload[0] = iotTxData->Battery.BatteryIndex;
    payload[1] = iotTxData->Battery.Bars;
    payload[2] = (uint8_t)(iotTxData->Battery.Percentage & 0xFF);
    payload[3] = (uint8_t)((iotTxData->Battery.Percentage >> 8) & 0xFF);
    payload[4] = (uint8_t)iotTxData->Battery.Status;
    return Ble_BuildEncryptedPacket(Ble_MapIotCmdToBleCmd(IOT_PROTOCOL_KEY_BATTERY_STATE), payload, (uint16_t)sizeof(payload), packet, packet_size);
}

static void Ble_Send_IotCommState(const IotTxDataTransDef *iotTxData)
{
    uint8_t payload[5];

    if(iotTxData == NULL) {
        return;
    }

    payload[0] = iotTxData->Comm.State4G;
    payload[1] = iotTxData->Comm.State5G;
    payload[2] = iotTxData->Comm.StateWired;
    payload[3] = iotTxData->Comm.StateWifi;
    payload[4] = iotTxData->Comm.StateBle;
    Ble_Send_IotStatusPacket(Ble_MapIotCmdToBleCmd(IOT_PROTOCOL_KEY_COMM_STATE), payload, (uint16_t)sizeof(payload));
}

static uint16_t Ble_Build_IotCommStatePacket(uint8_t *packet, uint16_t packet_size, const IotTxDataTransDef *iotTxData)
{
    uint8_t payload[5];

    if(iotTxData == NULL) {
        return 0U;
    }

    payload[0] = iotTxData->Comm.State4G;
    payload[1] = iotTxData->Comm.State5G;
    payload[2] = iotTxData->Comm.StateWired;
    payload[3] = iotTxData->Comm.StateWifi;
    payload[4] = iotTxData->Comm.StateBle;
    return Ble_BuildEncryptedPacket(Ble_MapIotCmdToBleCmd(IOT_PROTOCOL_KEY_COMM_STATE), payload, (uint16_t)sizeof(payload), packet, packet_size);
}

static void Ble_Send_IotDeviceInfo(void)
{
    uint8_t payload[40];
    unsigned int versionPart0 = 0U;
    unsigned int versionPart1 = 0U;
    unsigned int versionPart2 = 0U;
    unsigned int versionPart3 = 0U;

    memset(payload, 0, sizeof(payload));
    (void)memcpy(payload, SERIAL_NUMBER, IOT_MIN((unsigned int)(sizeof(payload) / 2U), (unsigned int)(sizeof(SERIAL_NUMBER) - 1U)));

    if (sscanf(FIRMWARE_VERSION, "V%u.%u.%u.%u", &versionPart0, &versionPart1, &versionPart2, &versionPart3) == 4) {
        payload[20] = (uint8_t)versionPart0;
        payload[21] = (uint8_t)versionPart1;
        payload[22] = (uint8_t)versionPart2;
        payload[23] = (uint8_t)versionPart3;
    } else {
        (void)memcpy(&payload[20], FIRMWARE_VERSION, IOT_MIN((unsigned int)(sizeof(payload) - 20U), (unsigned int)(sizeof(FIRMWARE_VERSION) - 1U)));
    }

    Ble_Send_IotStatusPacket(Ble_MapIotCmdToBleCmd(IOT_PROTOCOL_KEY_DEVICE_INFO), payload, (uint16_t)sizeof(payload));
}

static uint16_t Ble_Build_IotDeviceInfoPacket(uint8_t *packet, uint16_t packet_size)
{
    uint8_t payload[40];
    unsigned int versionPart0 = 0U;
    unsigned int versionPart1 = 0U;
    unsigned int versionPart2 = 0U;
    unsigned int versionPart3 = 0U;

    memset(payload, 0, sizeof(payload));
    (void)memcpy(payload, SERIAL_NUMBER, IOT_MIN((unsigned int)(sizeof(payload) / 2U), (unsigned int)(sizeof(SERIAL_NUMBER) - 1U)));

    if (sscanf(FIRMWARE_VERSION, "V%u.%u.%u.%u", &versionPart0, &versionPart1, &versionPart2, &versionPart3) == 4) {
        payload[20] = (uint8_t)versionPart0;
        payload[21] = (uint8_t)versionPart1;
        payload[22] = (uint8_t)versionPart2;
        payload[23] = (uint8_t)versionPart3;
    } else {
        (void)memcpy(&payload[20], FIRMWARE_VERSION, IOT_MIN((unsigned int)(sizeof(payload) - 20U), (unsigned int)(sizeof(FIRMWARE_VERSION) - 1U)));
    }

    return Ble_BuildEncryptedPacket(Ble_MapIotCmdToBleCmd(IOT_PROTOCOL_KEY_DEVICE_INFO), payload, (uint16_t)sizeof(payload), packet, packet_size);
}

static void Ble_ProcessPeriodicIotUpload(void)
{
    static uint16_t uploadTicks = 0U;
    const IotTxDataTransDef *iotTxData;

    if(BleCtrlInfo.state != BLE_STATE_DEV_CONNECTED) {
        uploadTicks = 0U;
        return;
    }

    uploadTicks = (uint16_t)(uploadTicks + 1U);
    if(uploadTicks < BLE_IOT_UPLOAD_INTERVAL_TICKS) {
        return;
    }
    uploadTicks = 0U;

    iotTxData = IotTxDataTrans_Get();
    Ble_Send_IotRunMode(iotTxData);
    Ble_Send_IotBatteryState(iotTxData);
    Ble_Send_IotCommState(iotTxData);
    Ble_Send_IotDeviceInfo();
}

uint8_t BleProtocol_SendPeriodicIotPackets(BleProtocolRawSendFn sendFn, void *context)
{
    const IotTxDataTransDef *iotTxData;
    uint8_t packet[128];
    uint16_t packetLen;
    uint8_t sentCount = 0U;

    if(sendFn == NULL) {
        return 0U;
    }

    IotTxDataTrans_RefreshRealtimeStatus();
    iotTxData = IotTxDataTrans_Get();

    packetLen = Ble_Build_IotRunModePacket(packet, (uint16_t)sizeof(packet), iotTxData);
    if((packetLen == 0U) || !sendFn(packet, packetLen, context)) {
        return sentCount;
    }
    sentCount++;

    packetLen = Ble_Build_IotBatteryStatePacket(packet, (uint16_t)sizeof(packet), iotTxData);
    if((packetLen == 0U) || !sendFn(packet, packetLen, context)) {
        return sentCount;
    }
    sentCount++;

    packetLen = Ble_Build_IotCommStatePacket(packet, (uint16_t)sizeof(packet), iotTxData);
    if((packetLen == 0U) || !sendFn(packet, packetLen, context)) {
        return sentCount;
    }
    sentCount++;

    packetLen = Ble_Build_IotDeviceInfoPacket(packet, (uint16_t)sizeof(packet));
    if((packetLen == 0U) || !sendFn(packet, packetLen, context)) {
        return sentCount;
    }
    sentCount++;

    return sentCount;
}

void Ble_Send_Dev_Info()
{
    TxDeviceInfoDef device_info;
    uint8_t su8_DataBuffer[64];
    uint16_t u16Data_Cnt = 0;
    memset(su8_DataBuffer, 0x00, sizeof(su8_DataBuffer));

    /* payload: pack device_info fields in order */
    memset(&device_info, 0x00, sizeof(device_info));

    /* Ensure buffer is large enough */
    if(sizeof(su8_DataBuffer) < sizeof(TxDeviceInfoDef)) {
        return;
    }
    device_info.device_sn[0] =  'S';
    device_info.device_sn[1] =  'N';
    memcpy(&su8_DataBuffer[u16Data_Cnt], device_info.device_type, sizeof(device_info.device_type));
    u16Data_Cnt += (uint16_t)sizeof(device_info.device_type);

    memcpy(&su8_DataBuffer[u16Data_Cnt], device_info.device_sn, sizeof(device_info.device_sn));
    u16Data_Cnt += (uint16_t)sizeof(device_info.device_sn);
    device_info.main_ctrl_sw_ver[0] = 1;
    memcpy(&su8_DataBuffer[u16Data_Cnt], device_info.main_ctrl_sw_ver, sizeof(device_info.main_ctrl_sw_ver));
    u16Data_Cnt += (uint16_t)sizeof(device_info.main_ctrl_sw_ver);

    memcpy(&su8_DataBuffer[u16Data_Cnt], device_info.control_board_sw_ver, sizeof(device_info.control_board_sw_ver));
    u16Data_Cnt += (uint16_t)sizeof(device_info.control_board_sw_ver);

    memcpy(&su8_DataBuffer[u16Data_Cnt], device_info.power_board_sw_ver, sizeof(device_info.power_board_sw_ver));
    u16Data_Cnt += (uint16_t)sizeof(device_info.power_board_sw_ver);

    /* uint16_t fields: little-endian */
    device_info.main_ctrl_hw_ver = 0x1111;
    su8_DataBuffer[u16Data_Cnt++] = (uint8_t)((device_info.main_ctrl_hw_ver >> 0) & 0xFF);
    su8_DataBuffer[u16Data_Cnt++] = (uint8_t)((device_info.main_ctrl_hw_ver >> 8) & 0xFF);

    su8_DataBuffer[u16Data_Cnt++] = (uint8_t)((device_info.power_board_hw_ver >> 0) & 0xFF);
    su8_DataBuffer[u16Data_Cnt++] = (uint8_t)((device_info.power_board_hw_ver >> 8) & 0xFF);

    Ble_Send_EncryptedPacket(BLE_CMD_DEVICE_INFO, su8_DataBuffer, u16Data_Cnt);
}



void Ble_Send_HandShake()
{
    char* ble_mac = NULL;
    uint8_t su8_DataBuffer[64];
    uint16_t u16Data_Cnt = 0;

    memset(su8_DataBuffer, 0x00, sizeof(su8_DataBuffer));

    /* payload: MAC 地址前 12 位（ASCII） */
    ble_mac = WifiGetMACAddr();
    if(ble_mac != NULL) {
        for(int i = 0; i < 12; i++) {
            su8_DataBuffer[u16Data_Cnt++] = (uint8_t)ble_mac[i];
        }
    }

    Ble_Send_EncryptedPacket(BLE_CMD_HANDSHAKE, su8_DataBuffer, u16Data_Cnt);
}

void Ble_Send_Heart_Beat()
{
    Ble_Send_EncryptedPacket(BLE_CMD_HEARTBEAT, NULL, 0);
}

void Ble_Send_Disconnect()
{
    Ble_Send_EncryptedPacket(BLE_CMD_DISCONNECT, NULL, 0);
}


void Ble_Send_Power_Status()
{
    uint8_t su8_DataBuffer[64];
    uint16_t u16Data_Cnt = 0;
    PowerStatusDef power_status;

    memset(su8_DataBuffer, 0x00, sizeof(su8_DataBuffer));
    power_status.adapter_present = 1;
    power_status.battery1_percentage =  300;
    power_status.battery2_percentage = 200;
    power_status.battery1_present = 1;
    power_status.battery2_present = 1;
    power_status.battery1_voltage = 3;
    power_status.battery2_voltage = 3;
    /* payload:  power status data */
    su8_DataBuffer[u16Data_Cnt++] = power_status.adapter_present;
    su8_DataBuffer[u16Data_Cnt++] = power_status.power_supply_status;
    su8_DataBuffer[u16Data_Cnt++] = power_status.charge_status;
    su8_DataBuffer[u16Data_Cnt++] = power_status.battery1_present;
    su8_DataBuffer[u16Data_Cnt++] = power_status.battery1_level_bar;
    su8_DataBuffer[u16Data_Cnt++] = (uint8_t)((power_status.battery1_percentage >> 8) & 0xFF);
    su8_DataBuffer[u16Data_Cnt++] = (uint8_t)((power_status.battery1_percentage >> 0) & 0xFF);
    su8_DataBuffer[u16Data_Cnt++] = (uint8_t)((power_status.battery1_voltage >> 8) & 0xFF);
    su8_DataBuffer[u16Data_Cnt++] = (uint8_t)((power_status.battery1_voltage >> 0) & 0xFF);
    su8_DataBuffer[u16Data_Cnt++] = power_status.battery2_present;
    su8_DataBuffer[u16Data_Cnt++] = power_status.battery2_level_bar;
    su8_DataBuffer[u16Data_Cnt++] = (uint8_t)((power_status.battery2_percentage >> 8) & 0xFF);
    su8_DataBuffer[u16Data_Cnt++] = (uint8_t)((power_status.battery2_percentage >> 0) & 0xFF);
    su8_DataBuffer[u16Data_Cnt++] = (uint8_t)((power_status.battery2_voltage >> 8) & 0xFF);
    su8_DataBuffer[u16Data_Cnt++] = (uint8_t)((power_status.battery2_voltage >> 0) & 0xFF);

    Ble_Send_EncryptedPacket(BLE_CMD_POWER_STATUS, su8_DataBuffer, u16Data_Cnt);
}

void Ble_Data_Send_Handle(void)
{
    // 没有检验发送是否成功，最好加上发送间隔 50ms左右发送1包基本上没问题
    if(BleCtrlInfo.flag.bits.Reply_HandShake){
        BleCtrlInfo.flag.bits.Reply_HandShake = 0;
        Ble_Send_HandShake();
    }

    if(BleCtrlInfo.flag.bits.Reply_Dev_Info) {
        BleCtrlInfo.flag.bits.Reply_Dev_Info = 0;
        Ble_Send_Dev_Info();
    }

    if(BleCtrlInfo.flag.bits.Reply_Heart_Beat) {
        BleCtrlInfo.flag.bits.Reply_Heart_Beat = 0;
        Ble_Send_Heart_Beat();
    }

    if(BleCtrlInfo.flag.bits.Reply_Disconnect) {
        BleCtrlInfo.flag.bits.Reply_Disconnect = 0;
        Ble_Send_Disconnect();
    }

    if(BleCtrlInfo.flag.bits.Reply_Power_Status) {
        BleCtrlInfo.flag.bits.Reply_Power_Status = 0;
        Ble_Send_Power_Status();
    }
        
}


void Ble_Data_Unpack_Handle(uint8_t cmd, uint8_t* data)
{
    switch(cmd)
    {
        case BLE_CMD_HANDSHAKE:
            // Handle handshake command
            break;
        case BLE_CMD_HEARTBEAT:
            // Handle heartbeat command
            BleCtrlInfo.flag.bits.Reply_Heart_Beat = 1;
            break;
        case BLE_CMD_DISCONNECT:
            // Handle control command
            BleCtrlInfo.flag.bits.Process_Disconnect = 1;
            break;
        case BLE_CMD_DEVICE_INFO:
            // Handle device info command
            BleCtrlInfo.flag.bits.Reply_Dev_Info = 1;
            break;
        case BLE_CMD_POWER_STATUS:
            // Handle control command
            BleCtrlInfo.flag.bits.Reply_Power_Status = 1;
            break;
        default:
            // Handle unknown command
            break;
    }
}


void BleProtocolStateProcess(void)
{
    ble_Recv_EnumDef ble_recv_state;
    lwrb_t*  Ring_WireLessComm = Wireless_Get_Rev_Ringbuff();

    switch (BleCtrlInfo.state)
    {
    case BLE_STATE_INIT:
        //设备初始化
        BleCtrlInfo.state = BLE_STATE_DEV_READY;
        BleProtocolStateInit();
        break;
    case BLE_STATE_DEV_READY:
        //设备初始化完成，进入等待连接状态
        if(Ble_GetHardwareState() == BLEM_READY_STATE)
        {
            if(BleAESEncryptInit()) {
                BleCtrlInfo.state = BLE_STATE_DEV_WAIT_CONN;
            } else {
                BleCtrlInfo.state = BLE_STATE_DEV_CONN_FAIL;
            }
        }
        break;
    case BLE_STATE_DEV_WAIT_CONN:
        ble_recv_state = (ble_Recv_EnumDef)Wireless_Handle_Data(Ring_WireLessComm);
        if(ble_recv_state == wRecv_Puls) {
            ble_recv_state = wRecv_None;
            if(strcmp((const char*)BleCtrlInfo.RevData, "+BLECONN") == 0) {
                BleCtrlInfo.conn_timeout = BLE_CONNECT_TIMEOUT;
            } else if(strcmp((const char*)BleCtrlInfo.RevData, "+BLEDISCONN") == 0) {
                BleCtrlInfo.conn_timeout = 0;
            }
        }
        else if (ble_recv_state == wRecv_Data){
            ble_recv_state = wRecv_None;
            memcpy(BleCtrlInfo.Encrypt_Data,BleCtrlInfo.RevData+6,BleCtrlInfo.DecryptIndex);
            BleAESDecrypt(BleCtrlInfo.Encrypt_Data,BleCtrlInfo.DecryptData,BleCtrlInfo.DecryptIndex);
            if(strcmp((const char*)BleCtrlInfo.DecryptData, (const char*)BleCtrlInfo.MAC) == 0) {
                BleCtrlInfo.state = BLE_STATE_DEV_CONNECTED;
                IotMgrSetStatus(NET_DEV_BLE, NET_STATE_CONNECT);
                BleCtrlInfo.flag.bits.Reply_HandShake = 1;
                BleCtrlInfo.conn_timeout = 0;
            }
        }
        if(BleCtrlInfo.conn_timeout > 0) {
            if(BleCtrlInfo.conn_timeout <= BLE_TASK_RUN_PERIOD) {
                // Fail to Connect within the time limit
                BleCtrlInfo.state = BLE_STATE_DEV_CONN_FAIL;
                BleCtrlInfo.conn_timeout = 0;
            } else {
                BleCtrlInfo.conn_timeout  -= BLE_TASK_RUN_PERIOD;
            }
        }
        break;
    case BLE_STATE_DEV_CONNECTED:
        // Device Recive Handle
        ble_recv_state = (ble_Recv_EnumDef)Wireless_Handle_Data(Ring_WireLessComm);
        if(ble_recv_state == wRecv_Data) {
            ble_recv_state = wRecv_None;
            memcpy(BleCtrlInfo.Encrypt_Data,BleCtrlInfo.RevData+6,BleCtrlInfo.DecryptIndex);
            my_aes_decrypt(BleCtrlInfo.Encrypt_Data,BleCtrlInfo.DecryptData,BleCtrlInfo.DecryptIndex);
            Ble_Data_Unpack_Handle(BleCtrlInfo.RevData[3],BleCtrlInfo.DecryptData);
        } else if(ble_recv_state == wRecv_Puls) {
            ble_recv_state = wRecv_None;
            if(strcmp((const char*)BleCtrlInfo.RevData, "+QBLESTAT:CONNECTED") == 0) {
                // Connection is still alive
            } else if(strcmp((const char*)BleCtrlInfo.RevData, "+QBLESTAT:DISCONNECTED") == 0) {
                BleCtrlInfo.state = BLE_STATE_DEV_CONN_FAIL;
            }
        }
        // Device Send Handle
        // Process Disconnect
        if(BleCtrlInfo.flag.bits.Process_Disconnect) {
            BleCtrlInfo.flag.bits.Process_Disconnect = 0;
            BleCtrlInfo.flag.bits.Reply_Disconnect = 1;
            BleCtrlInfo.dis_timeout = 1000;
            BleCtrlInfo.state = BLE_STATE_DEV_CONN_FAIL;
        }
        break;
    case BLE_STATE_DEV_CONN_FAIL:
        // Handle connection failure
        BleCtrlInfo.conn_timeout = 0;
        if(BleCtrlInfo.dis_timeout >= BLE_TASK_RUN_PERIOD) {
            BleCtrlInfo.dis_timeout-=BLE_TASK_RUN_PERIOD;
        }
        if(BleCtrlInfo.flag.bits.Reply_Disconnect == 0 && BleCtrlInfo.dis_timeout<=BLE_TASK_RUN_PERIOD)
        {
            BleCtrlInfo.state = BLE_STATE_DEV_NEED_RESET;
        }
        break;
    case BLE_STATE_DEV_NEED_RESET:
        break;
    }
    Ble_ProcessPeriodicIotUpload();
    BleSendScheduler();
}

void BleSendHeader(uint16_t len)
{
    uint8_t writedata[128];
    snprintf((char*)writedata,sizeof(writedata),"AT+BLEGATTSNTFY=0,1,2,%d\r\n",len);
    Send_Data_To_Wireless(writedata,strlen((const char*)writedata));
}

void BleSendScheduler(void)
{
    uint16_t RingDataLen;
    //uint8_t *sSendBuffer = mymalloc(SRAMEX,256);
    static uint8_t sSendBuffer[256];
    static uint16_t data_len;
    static uint16_t remain_len;
    static uint16_t send_index;

    if(BleCtrlInfo.state <= BLE_STATE_DEV_READY || BleCtrlInfo.state >= BLE_STATE_DEV_NEED_RESET) {
        return;
    }
    Ble_Data_Send_Handle();
    
    switch(BleCtrlInfo.SendState)
    {
        case wSend_None:
            RingDataLen = lwrb_get_full(&sSendRing_Comm);
            if(RingDataLen > 2) {
                lwrb_peek(&sSendRing_Comm,0, sSendBuffer, 2); // 先读出长度
                data_len = (sSendBuffer[0] ) | (sSendBuffer[1]<< 8);
                if(RingDataLen >= (data_len + 2)) {
                    lwrb_peek(&sSendRing_Comm,0, sSendBuffer, data_len + 2); // 读出完整数据
                    BleCtrlInfo.SendState = wSend_Header;
                    remain_len = data_len;
                    send_index = 2; // 数据在环形缓冲区中的起始位置（跳过长度字段）
                } else {
                    break;
                }
            } else {
                break;
            }   
        case wSend_Header:
            BleCtrlInfo.flag.bits.ReciveSendFlag = 0;
            if(remain_len > 20) {
                BleSendHeader(20);
            } else {
                BleSendHeader(remain_len);
            }
            BleCtrlInfo.SendState = wSend_Data;
            break;
        case wSend_Data:
            if(BleCtrlInfo.flag.bits.ReciveSendFlag) {
                BleCtrlInfo.flag.bits.ReciveOkFlag = 0;
                //sSendBuffer[1] = '\"';
                //sSendBuffer[data_len + 2] = '\"';
                //sSendBuffer[data_len + 3] = '\r';
                //sSendBuffer[data_len + 4] = '\n';
                if(remain_len > 20) {
                    Send_Data_To_Wireless(sSendBuffer+send_index, 20);
                } else {
                    Send_Data_To_Wireless(sSendBuffer+send_index, remain_len);
                }
                BleCtrlInfo.SendState = wSend_WaitAck;
            }
            break;
        case wSend_WaitAck:
            if(BleCtrlInfo.flag.bits.ReciveOkFlag) {
                if(remain_len > 20 ){
                    send_index += 20;
                    remain_len -= 20;
                    BleCtrlInfo.SendState = wSend_Header;
                } else {
                    lwrb_read(&sSendRing_Comm, sSendBuffer, data_len + 2); // 读出并丢弃已发送数据
                    BleCtrlInfo.SendState = wSend_None;
                }
            }
            break;
        default:
            break;
    }
    //myfree(sSendBuffer);
}


BleRunStateDef BleGetStateInfoPtr(void)
{
    return BleCtrlInfo.state;
}

void BleSetStateToInit(void)
{
    BleCtrlInfo.state = BLE_STATE_INIT;
}

void BleDisConnectUpper(void)
{
    if(BleCtrlInfo.state == BLE_STATE_DEV_CONNECTED)
    {
        BleCtrlInfo.flag.bits.Process_Disconnect = 1;
    }
}

bool BleHardwareInit(void)
{    
    //设置是否回显命令,0不显示，1显示,这里基于不回显
    lwrb_init(&sSendRing_Comm, sringSendBuffer, sizeof(sringSendBuffer));
    
    BleSwitchEcho(0);
    
    if(BleRequestVersion() == false)
        return false;
    
	if(BleSetMode(2) == false)
		return false;

    // if(WifiSendCmd("AT+BLEGATTSSRVCRE\r\n", strlen("AT+BLEGATTSSRVCRE\r\n"), 1000, "OK", 0) == false)
    //     return false;
    
    // if(WifiSendCmd("AT+BLEGATTSSRVSTART\r\n", strlen("AT+BLEGATTSSRVSRT\r\n"), 1000, "OK", 0) == false)
    //     return false;

	if(BleGetMacAddr() == false)
		return false;
    
//    if(BleSetServerUUID(UUID_APP_SEVER_STR) == false)
//		return RCWifiDeviceInitFailed;
//    
//    if(BleSetRecvUUID(UUID_APP_RECV_STR) == false)
//		return RCWifiDeviceInitFailed;
//    
//    if(BleSetSendUUID(UUID_APP_SEND_STR) == false)
//		return RCWifiDeviceInitFailed;
    
    if(BleSetAdvParam(200, 200) == false)
		return false;
    
    if(BleSetAdvData() == false)
		return false;  
 
    if(BleAdvStart() == false)
		return false;   
    
    //WifiSendCmd("AT+BLEGATTSCHAR?\r\n", strlen("AT+BLEGATTSCHAR?\r\n"), 1000, "OK", 0); // 查询特征值，确保BLE模块已准备好接收数据
    return true;
}

/*********************End of File********************/
