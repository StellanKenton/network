/***********************************************************************************
* @file     : wireless.c
* @brief    : BLE wireless manager built on top of the esp32c5 module.
* @details  : This layer keeps the ESP32-C5 bootstrap in the module, and adds
*             the ble.c-style business protocol state machine here.
**********************************************************************************/
#include <string.h>

#include "wireless.h"

#include "../../port/esp32c5_port.h"
#include "../../../rep/module/esp32c5/esp32c5.h"
#include "../../../rep/service/log/log.h"
#include "../../../rep/service/rtos/rtos.h"
#include "../../../rep/tools/md5/md5.h"
#include "stm32f4xx_cryp.h"
#include "stm32f4xx_rcc.h"

#define WIRELESS_BLE_DEVICE_NAME               "Primedic-VENT-0001"
#define WIRELESS_BLE_ADV_DATA_TEXT             "\"02010613095072696D656469632D56454E542D30303031\""
#define WIRELESS_LOG_TAG                       "wireless"
#define WIRELESS_RETRY_INTERVAL_MS             1000U

#define WIRELESS_PROTO_SYNC0                   0xFAU
#define WIRELESS_PROTO_SYNC1                   0xFCU
#define WIRELESS_PROTO_VERSION                 0x01U
#define WIRELESS_PROTO_AES_BLOCK_SIZE          16U
#define WIRELESS_PROTO_MAX_FRAME_SIZE          128U
#define WIRELESS_PROTO_MAX_RX_BUFFER_SIZE      256U
#define WIRELESS_PROTO_STATUS_INTERVAL_MS      5000U

#define WIRELESS_CMD_HANDSHAKE                 0x01U
#define WIRELESS_CMD_HEARTBEAT                 0x02U
#define WIRELESS_CMD_DISCONNECT                0x03U
#define WIRELESS_CMD_DEVICE_INFO               0x04U
#define WIRELESS_CMD_POWER_STATUS              0x05U

#define WIRELESS_DEVICE_SERIAL                 "UNKNOWN-SN"
#define WIRELESS_DEVICE_VERSION                "V0.0.0.0"

typedef struct stWirelessProtoContext {
	uint8_t rxBuffer[WIRELESS_PROTO_MAX_RX_BUFFER_SIZE];
	uint16_t rxLength;
	uint8_t macBytes[6];
	char macText[13];
	uint8_t aesKey[MD5_DIGEST_SIZE];
	uint8_t hasMac;
	uint8_t isHandshakeDone;
	uint32_t lastStatusTickMs;
} stWirelessProtoContext;

static uint8_t gWirelessStarted = 0u;
static uint32_t gWirelessRetryTickMs = 0u;
static enum bleState gWirelessBleState = BLE_STATE_IDLE;
static stWirelessProtoContext gWirelessProto;

static void wirelessUpdateBleState(void);
static eEsp32c5Status wirelessConfigureEsp32c5BleCfg(stEsp32c5BleCfg *bleCfg);
static eEsp32c5Status wirelessBootstrapEsp32c5(void);
static void wirelessTryBootstrap(void);
static void wirelessPump(void);
static bool wirelessIsBleInitDone(void);
static void wirelessProtoReset(uint8_t clearMac);
static void wirelessProtoProcess(void);
static void wirelessProtoPollRx(void);
static void wirelessProtoProcessFrames(void);
static void wirelessProtoHandleFrame(uint8_t cmd, const uint8_t *payload, uint16_t payloadLen);
static bool wirelessProtoEnsureKey(void);
static bool wirelessProtoParseMacString(const char *text, uint8_t macBytes[6]);
static void wirelessProtoFormatMacText(const uint8_t macBytes[6], char text[13]);
static uint16_t wirelessProtoCrc16Compute(const uint8_t *data, uint16_t length);
static bool wirelessProtoAesCrypt(const uint8_t *input, uint8_t *output, uint16_t length, uint8_t encrypt);
static uint16_t wirelessProtoBuildPacket(uint8_t cmd,
					 const uint8_t *plain,
					 uint16_t plainLen,
					 uint8_t *packet,
					 uint16_t packetSize);
static void wirelessProtoSendPacket(uint8_t cmd, const uint8_t *plain, uint16_t plainLen);
static void wirelessProtoSendHandshake(void);
static void wirelessProtoSendHeartbeat(void);
static void wirelessProtoSendDeviceInfo(void);
static void wirelessProtoSendPowerStatus(void);
static void wirelessProtoAppendRx(const uint8_t *buffer, uint16_t length);
static void wirelessProtoConsumeRx(uint16_t length);
static bool wirelessProtoMatchMacPayload(const uint8_t *payload, uint16_t payloadLen);

static void wirelessProtoReset(uint8_t clearMac)
{
	gWirelessProto.rxLength = 0U;
	gWirelessProto.isHandshakeDone = 0U;
	gWirelessProto.lastStatusTickMs = 0U;
	if (clearMac != 0U) {
		gWirelessProto.hasMac = 0U;
		(void)memset(gWirelessProto.macBytes, 0, sizeof(gWirelessProto.macBytes));
		(void)memset(gWirelessProto.macText, 0, sizeof(gWirelessProto.macText));
		(void)memset(gWirelessProto.aesKey, 0, sizeof(gWirelessProto.aesKey));
	}
}

static bool wirelessProtoParseMacString(const char *text, uint8_t macBytes[6])
{
	uint8_t index;
	uint8_t haveHighNibble;
	uint8_t currentByte;
	uint8_t value;
	char ch;

	if ((text == NULL) || (macBytes == NULL)) {
		return false;
	}

	index = 0U;
	haveHighNibble = 0U;
	currentByte = 0U;
	while ((*text != '\0') && (index < 6U)) {
		ch = *text++;
		if ((ch == ':') || (ch == '-') || (ch == ' ')) {
			continue;
		}

		if ((ch >= '0') && (ch <= '9')) {
			value = (uint8_t)(ch - '0');
		} else if ((ch >= 'a') && (ch <= 'f')) {
			value = (uint8_t)(ch - 'a' + 10);
		} else if ((ch >= 'A') && (ch <= 'F')) {
			value = (uint8_t)(ch - 'A' + 10);
		} else {
			return false;
		}

		if (haveHighNibble == 0U) {
			currentByte = (uint8_t)(value << 4);
			haveHighNibble = 1U;
		} else {
			currentByte = (uint8_t)(currentByte | value);
			macBytes[index++] = currentByte;
			currentByte = 0U;
			haveHighNibble = 0U;
		}
	}

	return (haveHighNibble == 0U) && (index == 6U);
}

static void wirelessProtoFormatMacText(const uint8_t macBytes[6], char text[13])
{
	static const char sHexChars[] = "0123456789ABCDEF";
	uint8_t index;

	if ((macBytes == NULL) || (text == NULL)) {
		return;
	}

	for (index = 0U; index < 6U; index++) {
		text[index * 2U] = sHexChars[(macBytes[index] >> 4) & 0x0FU];
		text[(index * 2U) + 1U] = sHexChars[macBytes[index] & 0x0FU];
	}
	text[12] = '\0';
}

static bool wirelessProtoEnsureKey(void)
{
	char macBuffer[ESP32C5_MAC_ADDRESS_TEXT_MAX_LENGTH];

	if (gWirelessProto.hasMac != 0U) {
		return true;
	}

	(void)memset(macBuffer, 0, sizeof(macBuffer));
	if (!esp32c5GetCachedMac(ESP32C5_DEV0, macBuffer, (uint16_t)sizeof(macBuffer))) {
		return false;
	}

	if (!wirelessProtoParseMacString(macBuffer, gWirelessProto.macBytes)) {
		LOG_W(WIRELESS_LOG_TAG, "invalid cached mac text=%s", macBuffer);
		return false;
	}

	wirelessProtoFormatMacText(gWirelessProto.macBytes, gWirelessProto.macText);
	if (md5CalcData(gWirelessProto.macBytes, (uint32_t)sizeof(gWirelessProto.macBytes), gWirelessProto.aesKey) != MD5_STATUS_OK) {
		LOG_W(WIRELESS_LOG_TAG, "md5 derive key failed");
		return false;
	}

	gWirelessProto.hasMac = 1U;
	LOG_I(WIRELESS_LOG_TAG, "protocol mac=%s", gWirelessProto.macText);
	return true;
}

static uint16_t wirelessProtoCrc16Compute(const uint8_t *data, uint16_t length)
{
	uint16_t crc;
	uint16_t result;
	uint8_t bitIndex;
	uint8_t byteValue;
	uint8_t reversedByte;

	crc = 0x0000U;
	while (length > 0U) {
		length--;
		byteValue = *data++;
		reversedByte = 0U;
		for (bitIndex = 0U; bitIndex < 8U; bitIndex++) {
			reversedByte = (uint8_t)((reversedByte << 1) | (byteValue & 0x01U));
			byteValue >>= 1;
		}

		crc ^= (uint16_t)((uint16_t)reversedByte << 8);
		for (bitIndex = 0U; bitIndex < 8U; bitIndex++) {
			if ((crc & 0x8000U) != 0U) {
				crc = (uint16_t)((crc << 1) ^ 0x8005U);
			} else {
				crc <<= 1;
			}
		}
	}

	result = 0U;
	for (bitIndex = 0U; bitIndex < 16U; bitIndex++) {
		result = (uint16_t)((result << 1) | (crc & 0x01U));
		crc >>= 1;
	}
	return result;
}

static bool wirelessProtoAesCrypt(const uint8_t *input, uint8_t *output, uint16_t length, uint8_t encrypt)
{
	ErrorStatus status;

	if ((input == NULL) || (output == NULL)) {
		return false;
	}

	if (length == 0U) {
		return true;
	}

	if (((uint16_t)(length % WIRELESS_PROTO_AES_BLOCK_SIZE) != 0U) || (gWirelessProto.hasMac == 0U)) {
		return false;
	}

	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_CRYP, ENABLE);
	status = CRYP_AES_ECB(encrypt != 0U ? CRYP_AlgoDir_Encrypt : CRYP_AlgoDir_Decrypt,
				      gWirelessProto.aesKey,
				      128U,
				      (uint8_t *)input,
				      (uint32_t)length,
				      output);
	return status == SUCCESS;
}

static uint16_t wirelessProtoBuildPacket(uint8_t cmd,
					 const uint8_t *plain,
					 uint16_t plainLen,
					 uint8_t *packet,
					 uint16_t packetSize)
{
	uint8_t plainBuffer[WIRELESS_PROTO_MAX_FRAME_SIZE];
	uint8_t encryptedBuffer[WIRELESS_PROTO_MAX_FRAME_SIZE];
	uint16_t encryptedLen;
	uint16_t writeIndex;
	uint16_t crcValue;

	if (packet == NULL) {
		return 0U;
	}

	encryptedLen = (uint16_t)((plainLen + (WIRELESS_PROTO_AES_BLOCK_SIZE - 1U)) & (uint16_t)~(WIRELESS_PROTO_AES_BLOCK_SIZE - 1U));
	if ((plainLen == 0U) && (plain == NULL)) {
		encryptedLen = 0U;
	}

	if ((encryptedLen > sizeof(encryptedBuffer)) || ((uint16_t)(encryptedLen + 8U) > packetSize)) {
		return 0U;
	}

	if (encryptedLen > 0U) {
		(void)memset(plainBuffer, 0, sizeof(plainBuffer));
		(void)memset(encryptedBuffer, 0, sizeof(encryptedBuffer));
		if ((plain != NULL) && (plainLen > 0U)) {
			(void)memcpy(plainBuffer, plain, plainLen);
		}
		if (!wirelessProtoAesCrypt(plainBuffer, encryptedBuffer, encryptedLen, 1U)) {
			return 0U;
		}
	}

	writeIndex = 0U;
	packet[writeIndex++] = WIRELESS_PROTO_SYNC0;
	packet[writeIndex++] = WIRELESS_PROTO_SYNC1;
	packet[writeIndex++] = WIRELESS_PROTO_VERSION;
	packet[writeIndex++] = cmd;
	packet[writeIndex++] = (uint8_t)((encryptedLen >> 8) & 0xFFU);
	packet[writeIndex++] = (uint8_t)(encryptedLen & 0xFFU);
	if (encryptedLen > 0U) {
		(void)memcpy(&packet[writeIndex], encryptedBuffer, encryptedLen);
		writeIndex = (uint16_t)(writeIndex + encryptedLen);
	}

	crcValue = wirelessProtoCrc16Compute(&packet[3], (uint16_t)(writeIndex - 3U));
	packet[writeIndex++] = (uint8_t)((crcValue >> 8) & 0xFFU);
	packet[writeIndex++] = (uint8_t)(crcValue & 0xFFU);
	return writeIndex;
}

static void wirelessProtoSendPacket(uint8_t cmd, const uint8_t *plain, uint16_t plainLen)
{
	uint8_t packet[WIRELESS_PROTO_MAX_FRAME_SIZE];
	uint16_t packetLen;
	eEsp32c5Status status;

	packetLen = wirelessProtoBuildPacket(cmd, plain, plainLen, packet, (uint16_t)sizeof(packet));
	if (packetLen == 0U) {
		LOG_W(WIRELESS_LOG_TAG, "build proto packet failed cmd=0x%02X", (unsigned int)cmd);
		return;
	}

	status = esp32c5WriteData(ESP32C5_DEV0, packet, packetLen);
	if (status != ESP32C5_STATUS_OK) {
		LOG_W(WIRELESS_LOG_TAG, "proto tx failed cmd=0x%02X status=%d", (unsigned int)cmd, (int)status);
	}
}

static void wirelessProtoSendHandshake(void)
{
	wirelessProtoSendPacket(WIRELESS_CMD_HANDSHAKE,
				       (const uint8_t *)gWirelessProto.macText,
				       (uint16_t)strlen(gWirelessProto.macText));
}

static void wirelessProtoSendHeartbeat(void)
{
	wirelessProtoSendPacket(WIRELESS_CMD_HEARTBEAT, NULL, 0U);
}

static void wirelessProtoSendDeviceInfo(void)
{
	uint8_t payload[40];
	uint16_t serialLen;
	uint16_t versionLen;

	(void)memset(payload, 0, sizeof(payload));
	serialLen = (uint16_t)strlen(WIRELESS_DEVICE_SERIAL);
	versionLen = (uint16_t)strlen(WIRELESS_DEVICE_VERSION);
	if (serialLen > 20U) {
		serialLen = 20U;
	}
	if (versionLen > 20U) {
		versionLen = 20U;
	}
	(void)memcpy(&payload[0], WIRELESS_DEVICE_SERIAL, serialLen);
	(void)memcpy(&payload[20], WIRELESS_DEVICE_VERSION, versionLen);
	wirelessProtoSendPacket(WIRELESS_CMD_DEVICE_INFO, payload, (uint16_t)sizeof(payload));
}

static void wirelessProtoSendPowerStatus(void)
{
	uint8_t payload[15];

	(void)memset(payload, 0, sizeof(payload));
	payload[0] = 1U;
	payload[1] = 0U;
	payload[2] = 0U;
	payload[3] = 1U;
	payload[4] = 4U;
	payload[5] = 0x01U;
	payload[6] = 0x2CU;
	payload[7] = 0x00U;
	payload[8] = 0x03U;
	payload[9] = 1U;
	payload[10] = 3U;
	payload[11] = 0x00U;
	payload[12] = 0xC8U;
	payload[13] = 0x00U;
	payload[14] = 0x03U;
	wirelessProtoSendPacket(WIRELESS_CMD_POWER_STATUS, payload, (uint16_t)sizeof(payload));
}

static void wirelessProtoAppendRx(const uint8_t *buffer, uint16_t length)
{
	uint16_t overflow;

	if ((buffer == NULL) || (length == 0U)) {
		return;
	}

	if (length >= (uint16_t)sizeof(gWirelessProto.rxBuffer)) {
		buffer = &buffer[length - (uint16_t)sizeof(gWirelessProto.rxBuffer)];
		length = (uint16_t)sizeof(gWirelessProto.rxBuffer);
		gWirelessProto.rxLength = 0U;
	}

	if ((uint16_t)(gWirelessProto.rxLength + length) > (uint16_t)sizeof(gWirelessProto.rxBuffer)) {
		overflow = (uint16_t)(gWirelessProto.rxLength + length - (uint16_t)sizeof(gWirelessProto.rxBuffer));
		(void)memmove(gWirelessProto.rxBuffer,
			      &gWirelessProto.rxBuffer[overflow],
			      (size_t)(gWirelessProto.rxLength - overflow));
		gWirelessProto.rxLength = (uint16_t)(gWirelessProto.rxLength - overflow);
	}

	(void)memcpy(&gWirelessProto.rxBuffer[gWirelessProto.rxLength], buffer, length);
	gWirelessProto.rxLength = (uint16_t)(gWirelessProto.rxLength + length);
}

static void wirelessProtoConsumeRx(uint16_t length)
{
	if (length >= gWirelessProto.rxLength) {
		gWirelessProto.rxLength = 0U;
		return;
	}

	(void)memmove(gWirelessProto.rxBuffer,
		      &gWirelessProto.rxBuffer[length],
		      (size_t)(gWirelessProto.rxLength - length));
	gWirelessProto.rxLength = (uint16_t)(gWirelessProto.rxLength - length);
}

static bool wirelessProtoMatchMacPayload(const uint8_t *payload, uint16_t payloadLen)
{
	uint16_t actualLen;

	if ((payload == NULL) || (gWirelessProto.hasMac == 0U)) {
		return false;
	}

	actualLen = 0U;
	while ((actualLen < payloadLen) && (payload[actualLen] != 0U)) {
		actualLen++;
	}

	if (actualLen != (uint16_t)strlen(gWirelessProto.macText)) {
		return false;
	}

	return memcmp(payload, gWirelessProto.macText, actualLen) == 0;
}

static void wirelessProtoHandleFrame(uint8_t cmd, const uint8_t *payload, uint16_t payloadLen)
{
	uint32_t nowTickMs;

	if ((cmd == WIRELESS_CMD_HANDSHAKE) && wirelessProtoMatchMacPayload(payload, payloadLen)) {
		if (gWirelessProto.isHandshakeDone == 0U) {
			gWirelessProto.isHandshakeDone = 1U;
			gWirelessProto.lastStatusTickMs = repRtosGetTickMs();
			wirelessProtoSendHandshake();
			wirelessProtoSendDeviceInfo();
			wirelessProtoSendPowerStatus();
			LOG_I(WIRELESS_LOG_TAG, "ble handshake done");
		}
		return;
	}

	if (gWirelessProto.isHandshakeDone == 0U) {
		return;
	}

	switch (cmd) {
	case WIRELESS_CMD_HEARTBEAT:
		wirelessProtoSendHeartbeat();
		break;
	case WIRELESS_CMD_DISCONNECT:
		wirelessProtoSendPacket(WIRELESS_CMD_DISCONNECT, NULL, 0U);
		(void)esp32c5DisconnectBle(ESP32C5_DEV0);
		gWirelessProto.isHandshakeDone = 0U;
		break;
	case WIRELESS_CMD_DEVICE_INFO:
		wirelessProtoSendDeviceInfo();
		break;
	case WIRELESS_CMD_POWER_STATUS:
		wirelessProtoSendPowerStatus();
		break;
	default:
		break;
	}

	nowTickMs = repRtosGetTickMs();
	if ((gWirelessProto.lastStatusTickMs == 0U) ||
	    ((nowTickMs - gWirelessProto.lastStatusTickMs) >= WIRELESS_PROTO_STATUS_INTERVAL_MS)) {
		gWirelessProto.lastStatusTickMs = nowTickMs;
		wirelessProtoSendPowerStatus();
	}
}

static void wirelessProtoProcessFrames(void)
{
	uint16_t frameLength;
	uint16_t encryptedLen;
	uint16_t crcValue;
	uint16_t crcExpected;
	uint16_t index;
	uint8_t plainBuffer[WIRELESS_PROTO_MAX_FRAME_SIZE];

	while (gWirelessProto.rxLength >= 8U) {
		if ((gWirelessProto.rxBuffer[0] != WIRELESS_PROTO_SYNC0) ||
		    (gWirelessProto.rxBuffer[1] != WIRELESS_PROTO_SYNC1)) {
			for (index = 1U; index < gWirelessProto.rxLength; index++) {
				if ((gWirelessProto.rxBuffer[index] == WIRELESS_PROTO_SYNC0) &&
				    ((index + 1U) < gWirelessProto.rxLength) &&
				    (gWirelessProto.rxBuffer[index + 1U] == WIRELESS_PROTO_SYNC1)) {
					break;
				}
			}
			wirelessProtoConsumeRx(index);
			continue;
		}

		if (gWirelessProto.rxBuffer[2] != WIRELESS_PROTO_VERSION) {
			wirelessProtoConsumeRx(1U);
			continue;
		}

		encryptedLen = (uint16_t)(((uint16_t)gWirelessProto.rxBuffer[4] << 8) | gWirelessProto.rxBuffer[5]);
		frameLength = (uint16_t)(encryptedLen + 8U);
		if ((frameLength > WIRELESS_PROTO_MAX_FRAME_SIZE) ||
		    ((encryptedLen != 0U) && ((encryptedLen % WIRELESS_PROTO_AES_BLOCK_SIZE) != 0U))) {
			wirelessProtoConsumeRx(1U);
			continue;
		}

		if (gWirelessProto.rxLength < frameLength) {
			break;
		}

		crcValue = wirelessProtoCrc16Compute(&gWirelessProto.rxBuffer[3], (uint16_t)(frameLength - 5U));
		crcExpected = (uint16_t)(((uint16_t)gWirelessProto.rxBuffer[frameLength - 2U] << 8) |
					gWirelessProto.rxBuffer[frameLength - 1U]);
		if (crcValue != crcExpected) {
			wirelessProtoConsumeRx(1U);
			continue;
		}

		if (encryptedLen > 0U) {
			if (!wirelessProtoEnsureKey()) {
				wirelessProtoConsumeRx(frameLength);
				continue;
			}
			if (!wirelessProtoAesCrypt(&gWirelessProto.rxBuffer[6], plainBuffer, encryptedLen, 0U)) {
				wirelessProtoConsumeRx(frameLength);
				continue;
			}
		} else {
			(void)memset(plainBuffer, 0, sizeof(plainBuffer));
		}

		wirelessProtoHandleFrame(gWirelessProto.rxBuffer[3], plainBuffer, encryptedLen);
		wirelessProtoConsumeRx(frameLength);
	}
}

static void wirelessProtoPollRx(void)
{
	uint8_t buffer[64];
	uint16_t readLen;

	readLen = esp32c5ReadData(ESP32C5_DEV0, buffer, (uint16_t)sizeof(buffer));
	while (readLen > 0U) {
		wirelessProtoAppendRx(buffer, readLen);
		readLen = esp32c5ReadData(ESP32C5_DEV0, buffer, (uint16_t)sizeof(buffer));
	}
}

static void wirelessProtoProcess(void)
{
	const stEsp32c5State *lpState;
	uint32_t nowTickMs;

	lpState = esp32c5GetState(ESP32C5_DEV0);
	if ((lpState == NULL) || !lpState->isReady) {
		wirelessProtoReset(1U);
		return;
	}

	(void)wirelessProtoEnsureKey();
	if (!lpState->isBleConnected) {
		if (gWirelessProto.isHandshakeDone != 0U) {
			LOG_I(WIRELESS_LOG_TAG, "ble link lost");
		}
		wirelessProtoReset(0U);
		return;
	}

	wirelessProtoPollRx();
	wirelessProtoProcessFrames();

	if (gWirelessProto.isHandshakeDone != 0U) {
		nowTickMs = repRtosGetTickMs();
		if ((gWirelessProto.lastStatusTickMs == 0U) ||
		    ((nowTickMs - gWirelessProto.lastStatusTickMs) >= WIRELESS_PROTO_STATUS_INTERVAL_MS)) {
			gWirelessProto.lastStatusTickMs = nowTickMs;
			wirelessProtoSendPowerStatus();
		}
	}
}

static void wirelessUpdateBleState(void)
{
	const stEsp32c5State *lpState;

	lpState = esp32c5GetState(ESP32C5_DEV0);
	if (lpState == NULL) {
		gWirelessBleState = BLE_STATE_ERROR;
		return;
	}

	if (lpState->runState == ESP32C5_RUN_ERROR) {
		gWirelessBleState = BLE_STATE_ERROR;
		return;
	}

	if (!lpState->isReady) {
		gWirelessBleState = BLE_STATE_INITIALIZING;
		return;
	}

	if (lpState->isBleConnected && (gWirelessProto.isHandshakeDone != 0U)) {
		gWirelessBleState = BLE_STATE_CONNECTED;
		return;
	}

	if (lpState->isBleAdvertising || lpState->isBleConnected) {
		gWirelessBleState = BLE_WAITING_CONNECTION;
		return;
	}

	gWirelessBleState = BLE_STATE_READY;
}

static eEsp32c5Status wirelessConfigureEsp32c5BleCfg(stEsp32c5BleCfg *bleCfg)
{
	if (bleCfg == NULL) {
		return ESP32C5_STATUS_INVALID_PARAM;
	}

	(void)memcpy(bleCfg->deviceNameText,
		     WIRELESS_BLE_DEVICE_NAME,
		     sizeof(WIRELESS_BLE_DEVICE_NAME));
	(void)memcpy(bleCfg->advDataText,
		     WIRELESS_BLE_ADV_DATA_TEXT,
		     sizeof(WIRELESS_BLE_ADV_DATA_TEXT));
	return ESP32C5_STATUS_OK;
}

static eEsp32c5Status wirelessBootstrapEsp32c5(void)
{
	stEsp32c5Cfg lCfg;
	stEsp32c5BleCfg lBleCfg;
	eEsp32c5Status lStatus;

	lStatus = esp32c5GetDefCfg(ESP32C5_DEV0, &lCfg);
	if (lStatus != ESP32C5_STATUS_OK) {
		LOG_W(WIRELESS_LOG_TAG, "bootstrap get cfg failed status=%d", (int)lStatus);
		return lStatus;
	}

	lStatus = esp32c5SetCfg(ESP32C5_DEV0, &lCfg);
	if (lStatus != ESP32C5_STATUS_OK) {
		LOG_W(WIRELESS_LOG_TAG, "bootstrap set cfg failed status=%d", (int)lStatus);
		return lStatus;
	}

	lStatus = esp32c5GetDefBleCfg(ESP32C5_DEV0, &lBleCfg);
	if (lStatus != ESP32C5_STATUS_OK) {
		LOG_W(WIRELESS_LOG_TAG, "bootstrap get ble cfg failed status=%d", (int)lStatus);
		return lStatus;
	}

	lStatus = wirelessConfigureEsp32c5BleCfg(&lBleCfg);
	if (lStatus != ESP32C5_STATUS_OK) {
		LOG_W(WIRELESS_LOG_TAG, "bootstrap fill ble cfg failed status=%d", (int)lStatus);
		return lStatus;
	}

	lStatus = esp32c5SetBleCfg(ESP32C5_DEV0, &lBleCfg);
	if (lStatus != ESP32C5_STATUS_OK) {
		LOG_W(WIRELESS_LOG_TAG, "bootstrap set ble cfg failed status=%d", (int)lStatus);
		return lStatus;
	}

	lStatus = esp32c5Init(ESP32C5_DEV0);
	if (lStatus != ESP32C5_STATUS_OK) {
		LOG_W(WIRELESS_LOG_TAG, "bootstrap module init failed status=%d", (int)lStatus);
		return lStatus;
	}

	lStatus = esp32c5Start(ESP32C5_DEV0, ESP32C5_ROLE_BLE_PERIPHERAL);
	if (lStatus != ESP32C5_STATUS_OK) {
		LOG_W(WIRELESS_LOG_TAG, "bootstrap start ble failed status=%d", (int)lStatus);
		return lStatus;
	}

	return ESP32C5_STATUS_OK;
}

static void wirelessTryBootstrap(void)
{
	eEsp32c5Status lStatus;
	uint32_t lNowTickMs;

	if (gWirelessStarted != 0u) {
		return;
	}

	lNowTickMs = repRtosGetTickMs();
	if (lNowTickMs < gWirelessRetryTickMs) {
		return;
	}

	lStatus = wirelessBootstrapEsp32c5();

	if (lStatus != ESP32C5_STATUS_OK) {
		esp32c5Stop(ESP32C5_DEV0);
		gWirelessBleState = BLE_STATE_ERROR;
		gWirelessRetryTickMs = lNowTickMs + WIRELESS_RETRY_INTERVAL_MS;
		LOG_W(WIRELESS_LOG_TAG, "esp32c5 bootstrap failed status=%d", (int)lStatus);
		return;
	}

	gWirelessStarted = 1u;
	gWirelessBleState = BLE_STATE_INITIALIZING;
	wirelessProtoReset(1U);
	LOG_I(WIRELESS_LOG_TAG, "esp32c5 ble bootstrap complete");
}

static void wirelessPump(void)
{
	wirelessTryBootstrap();
	if (gWirelessStarted != 0u) {
		(void)esp32c5Process(ESP32C5_DEV0, repRtosGetTickMs());
		wirelessProtoProcess();
	}
	wirelessUpdateBleState();
}

static bool wirelessIsBleInitDone(void)
{
	return (gWirelessBleState == BLE_WAITING_CONNECTION) ||
	       (gWirelessBleState == BLE_STATE_CONNECTED);
}

enum bleState wirelessGetBleState(void)
{
	return gWirelessBleState;
}

bool wirelessIsReady(void)
{
	return wirelessIsBleInitDone();
}

bool wirelessInit(void)
{
	wirelessPump();
	return wirelessIsBleInitDone();
}

void wirelessProcess(void)
{
	wirelessPump();
}

/**************************End of file********************************/
