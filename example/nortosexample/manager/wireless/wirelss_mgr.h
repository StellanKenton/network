/***********************************************************************************
* @file     : wirelss_mgr.h
* @brief    : Wireless manager declarations.
* @details  : Owns FC41D BLE bring-up for the cooperative system scheduler.
* @author   : GitHub Copilot
* @date     : 2026-04-21
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_WIRELSS_MGR_H
#define CPRSENSORBOOT_WIRELSS_MGR_H

#include <stdbool.h>
#include <stdint.h>

#include "../../../rep/module/fc41d/fc41d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIRELESS_MGR_BLE_NAME              "CprSensorBoot"
#define WIRELESS_MGR_BLE_SERVICE_UUID      "FE60"
#define WIRELESS_MGR_BLE_CHAR_UUID_RX      "FE61"
#define WIRELESS_MGR_BLE_CHAR_UUID_TX      "FE62"
#define WIRELESS_MGR_MAC_ADDRESS_TEXT_MAX_LENGTH FC41D_MAC_ADDRESS_TEXT_MAX_LENGTH

typedef enum eWirelessMgrHandshakeState {
	WIRELESS_MGR_HANDSHAKE_IDLE = 0,
	WIRELESS_MGR_HANDSHAKE_WAIT_PEER,
	WIRELESS_MGR_HANDSHAKE_READY,
} eWirelessMgrHandshakeState;

void wirelessMgrReset(void);
bool wirelessMgrInit(void);
void wirelessMgrProcess(uint32_t nowTickMs);
bool wirelessMgrIsReady(void);
eWirelessMgrHandshakeState wirelessMgrGetHandshakeState(void);
bool wirelessMgrIsHandshakeReady(void);
uint16_t wirelessMgrGetRxLength(void);
uint16_t wirelessMgrReadRxData(uint8_t *buffer, uint16_t bufferSize);
bool wirelessMgrWriteData(const uint8_t *buffer, uint16_t length);
bool wirelessMgrGetMacAddress(char *buffer, uint16_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_WIRELSS_MGR_H
/**************************End of file********************************/
