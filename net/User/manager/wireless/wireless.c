/***********************************************************************************
* @file     : wireless.c
* @brief    : Project-side ESP32-C5 wireless manager.
* @details  : Provides non-blocking BLE bring-up and status mapping for the
*             current system startup flow.
* @author   : GitHub Copilot
* @date     : 2026-04-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "wireless.h"

#include <string.h>

#include "../../../rep/module/esp32c5/esp32c5.h"
#include "../../../rep/service/log/log.h"
#include "../../../rep/service/rtos/rtos.h"

#define WIRELESS_LOG_TAG                 "wireless"
#define WIRELESS_DEVICE                  ESP32C5_DEV0
#define WIRELESS_RETRY_LOG_MS            1000U
#define WIRELESS_BLE_NAME                "Primedic-VENT-0001"
#define WIRELESS_BLE_ECHO_MAX_LEN        512U

static bool gWirelessConfigured = false;
static bool gWirelessStarted = false;
static bool gWirelessReadyLogged = false;
static bool gWirelessMacLogged = false;
static bool gWirelessBleConnected = false;
static uint32_t gWirelessLastWarnTick = 0U;
static eBleState gWirelessBleState = BLE_STATE_IDLE;
static uint8_t gWirelessBleRxServiceIndex = 0U;
static uint8_t gWirelessBleRxCharIndex = 0U;
static uint8_t gWirelessBleTxServiceIndex = 0U;
static uint8_t gWirelessBleTxCharIndex = 0U;

static bool wirelessCopyText(char *buffer, uint16_t bufferSize, const char *text);
static bool wirelessMatchPrefix(const uint8_t *buffer, uint16_t length, const char *text);
static bool wirelessTryParseUint(const uint8_t *buffer, uint16_t length, uint16_t *value);
static void wirelessLogTxCccdWrite(uint16_t serviceIndex,
				   uint16_t charIndex,
				   uint16_t descIndex,
				   const uint8_t *dataBuf,
				   uint16_t dataLen);
static bool wirelessHandleWriteUrc(const uint8_t *lineBuf, uint16_t lineLen);
static void wirelessEsp32c5UrcHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static bool wirelessConfigureIfNeeded(void);
static void wirelessService(void);
static void wirelessUpdateState(void);

static bool wirelessCopyText(char *buffer, uint16_t bufferSize, const char *text)
{
	uint16_t length;

	if ((buffer == NULL) || (bufferSize == 0U) || (text == NULL)) {
		return false;
	}

	length = (uint16_t)strlen(text);
	if (length >= bufferSize) {
		return false;
	}

	(void)memcpy(buffer, text, length + 1U);
	return true;
}

static bool wirelessMatchPrefix(const uint8_t *buffer, uint16_t length, const char *text)
{
	uint16_t index;
	uint16_t textLen;

	if ((buffer == NULL) || (text == NULL)) {
		return false;
	}

	textLen = (uint16_t)strlen(text);
	if (length < textLen) {
		return false;
	}

	for (index = 0U; index < textLen; index++) {
		if ((char)buffer[index] != text[index]) {
			return false;
		}
	}

	return true;
}

static bool wirelessTryParseUint(const uint8_t *buffer, uint16_t length, uint16_t *value)
{
	uint32_t parsed;
	uint16_t index;

	if ((buffer == NULL) || (length == 0U) || (value == NULL)) {
		return false;
	}

	parsed = 0U;
	for (index = 0U; index < length; index++) {
		if ((buffer[index] < '0') || (buffer[index] > '9')) {
			return false;
		}

		parsed = (parsed * 10U) + (uint32_t)(buffer[index] - '0');
		if (parsed > 65535UL) {
			return false;
		}
	}

	*value = (uint16_t)parsed;
	return true;
}

static void wirelessLogTxCccdWrite(uint16_t serviceIndex,
				   uint16_t charIndex,
				   uint16_t descIndex,
				   const uint8_t *dataBuf,
				   uint16_t dataLen)
{
	if ((serviceIndex != (uint16_t)gWirelessBleTxServiceIndex) ||
		(charIndex != (uint16_t)gWirelessBleTxCharIndex) ||
		(descIndex != 1U) ||
		(dataBuf == NULL) ||
		(dataLen == 0U)) {
		return;
	}

	LOG_I(WIRELESS_LOG_TAG,
		  "ble tx cccd write svc=%u char=%u desc=%u len=%u data=%.*s",
		  (unsigned int)serviceIndex,
		  (unsigned int)charIndex,
		  (unsigned int)descIndex,
		  (unsigned int)dataLen,
		  (int)dataLen,
		  (const char *)dataBuf);
}

static bool wirelessHandleWriteUrc(const uint8_t *lineBuf, uint16_t lineLen)
{
	uint16_t commaPos[5];
	uint16_t connIndex;
	uint16_t serviceIndex;
	uint16_t charIndex;
	uint16_t descIndex;
	uint16_t valueLen;
	uint16_t commaCount;
	uint16_t index;
	uint16_t dataLen;
	eEsp32c5Status status;

	if (!wirelessMatchPrefix(lineBuf, lineLen, "+WRITE:")) {
		return false;
	}

	commaCount = 0U;
	for (index = 7U; index < lineLen; index++) {
		if (lineBuf[index] == ',') {
			if (commaCount >= (uint16_t)(sizeof(commaPos) / sizeof(commaPos[0]))) {
				return false;
			}
			commaPos[commaCount++] = index;
		}
	}

	if ((commaCount != 5U) || (commaPos[commaCount - 1U] >= lineLen)) {
		return false;
	}

	if ((commaPos[0] <= 7U) ||
		!wirelessTryParseUint(&lineBuf[7], (uint16_t)(commaPos[0] - 7U), &connIndex)) {
		return false;
	}

	if ((commaPos[1] <= (uint16_t)(commaPos[0] + 1U)) ||
		!wirelessTryParseUint(&lineBuf[commaPos[0] + 1U],
					 (uint16_t)(commaPos[1] - (uint16_t)(commaPos[0] + 1U)),
					 &serviceIndex)) {
		return false;
	}

	if ((commaPos[2] <= (uint16_t)(commaPos[1] + 1U)) ||
		!wirelessTryParseUint(&lineBuf[commaPos[1] + 1U],
					 (uint16_t)(commaPos[2] - (uint16_t)(commaPos[1] + 1U)),
					 &charIndex)) {
		return false;
	}

	descIndex = 0U;
	if (commaPos[3] > (uint16_t)(commaPos[2] + 1U) &&
		!wirelessTryParseUint(&lineBuf[commaPos[2] + 1U],
					 (uint16_t)(commaPos[3] - (uint16_t)(commaPos[2] + 1U)),
					 &descIndex)) {
		return false;
	}

	if ((commaPos[4] <= (uint16_t)(commaPos[3] + 1U)) ||
		!wirelessTryParseUint(&lineBuf[commaPos[3] + 1U],
					 (uint16_t)(commaPos[4] - (uint16_t)(commaPos[3] + 1U)),
					 &valueLen)) {
		return false;
	}

	dataLen = (uint16_t)(lineLen - (uint16_t)(commaPos[commaCount - 1U] + 1U));
	if ((dataLen == 0U) || (dataLen > WIRELESS_BLE_ECHO_MAX_LEN) || (dataLen != valueLen)) {
		LOG_W(WIRELESS_LOG_TAG, "ignore ble write len=%u", (unsigned int)dataLen);
		return true;
	}

	wirelessLogTxCccdWrite(serviceIndex,
				  charIndex,
				  descIndex,
				  &lineBuf[commaPos[commaCount - 1U] + 1U],
				  dataLen);

	if ((serviceIndex != (uint16_t)gWirelessBleRxServiceIndex) ||
		(charIndex != (uint16_t)gWirelessBleRxCharIndex) ||
		(descIndex != 0U)) {
		return true;
	}

	status = esp32c5WriteData(WIRELESS_DEVICE,
							  &lineBuf[commaPos[commaCount - 1U] + 1U],
							  dataLen);
		if (status != ESP32C5_STATUS_OK) {
			LOG_W(WIRELESS_LOG_TAG,
				  "ble echo failed status=%d len=%u",
				  (int)status,
				  (unsigned int)dataLen);
			return true;
		}

	LOG_I(WIRELESS_LOG_TAG, "ble echo len=%u", (unsigned int)dataLen);
	return true;
}

static void wirelessEsp32c5UrcHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
	(void)userData;

	if ((lineBuf == NULL) || (lineLen == 0U)) {
		return;
	}

	(void)wirelessHandleWriteUrc(lineBuf, lineLen);
}

static bool wirelessConfigureIfNeeded(void)
{
	stEsp32c5Cfg cfg;
	stEsp32c5BleCfg bleCfg;
	eEsp32c5Status status;

	if (gWirelessConfigured) {
		return true;
	}

	status = esp32c5GetDefCfg(WIRELESS_DEVICE, &cfg);
	if (status != ESP32C5_STATUS_OK) {
		gWirelessBleState = BLE_STATE_ERROR;
		return false;
	}

	status = esp32c5GetDefBleCfg(WIRELESS_DEVICE, &bleCfg);
	if (status != ESP32C5_STATUS_OK) {
		gWirelessBleState = BLE_STATE_ERROR;
		return false;
	}

	bleCfg.initMode = 2U;
	bleCfg.advIntervalMin = 200U;
	bleCfg.advIntervalMax = 200U;
	bleCfg.rxServiceIndex = 1U;
	bleCfg.rxCharIndex = 1U;
	bleCfg.txServiceIndex = 1U;
	bleCfg.txCharIndex = 2U;
	if (!wirelessCopyText(bleCfg.name, (uint16_t)sizeof(bleCfg.name), WIRELESS_BLE_NAME)) {
		gWirelessBleState = BLE_STATE_ERROR;
		return false;
	}

	status = esp32c5SetCfg(WIRELESS_DEVICE, &cfg);
	if (status != ESP32C5_STATUS_OK) {
		gWirelessBleState = BLE_STATE_ERROR;
		return false;
	}

	status = esp32c5SetBleCfg(WIRELESS_DEVICE, &bleCfg);
	if (status != ESP32C5_STATUS_OK) {
		gWirelessBleState = BLE_STATE_ERROR;
		return false;
	}

	status = esp32c5Init(WIRELESS_DEVICE);
	if (status != ESP32C5_STATUS_OK) {
		gWirelessBleState = BLE_STATE_ERROR;
		return false;
	}

	status = esp32c5SetUrcHandler(WIRELESS_DEVICE, wirelessEsp32c5UrcHandler, NULL);
	if (status != ESP32C5_STATUS_OK) {
		gWirelessBleState = BLE_STATE_ERROR;
		return false;
	}

	gWirelessConfigured = true;
	gWirelessBleRxServiceIndex = bleCfg.rxServiceIndex;
	gWirelessBleRxCharIndex = bleCfg.rxCharIndex;
	gWirelessBleTxServiceIndex = bleCfg.txServiceIndex;
	gWirelessBleTxCharIndex = bleCfg.txCharIndex;
	gWirelessBleState = BLE_STATE_INITIALIZING;
	return true;
}

static void wirelessService(void)
{
	const stEsp32c5State *state;
	eEsp32c5Status status;
	uint32_t nowTickMs;

	if (!wirelessConfigureIfNeeded()) {
		return;
	}

	if (!gWirelessStarted) {
		status = esp32c5Start(WIRELESS_DEVICE, ESP32C5_ROLE_BLE_PERIPHERAL);
		if (status != ESP32C5_STATUS_OK) {
			gWirelessBleState = BLE_STATE_ERROR;
			return;
		}

		gWirelessStarted = true;
		gWirelessReadyLogged = false;
		gWirelessMacLogged = false;
		gWirelessBleConnected = false;
	}

	nowTickMs = repRtosGetTickMs();
	status = esp32c5Process(WIRELESS_DEVICE, nowTickMs);
	state = esp32c5GetState(WIRELESS_DEVICE);
	if ((status != ESP32C5_STATUS_OK) && ((nowTickMs - gWirelessLastWarnTick) >= WIRELESS_RETRY_LOG_MS)) {
		gWirelessLastWarnTick = nowTickMs;
		LOG_W(WIRELESS_LOG_TAG,
			  "esp32c5 process failed status=%d run=%d",
			  (int)status,
			  (state == NULL) ? -1 : (int)state->runState);
	}

	wirelessUpdateState();
}

static void wirelessUpdateState(void)
{
	const stEsp32c5State *state;
	char macAddress[ESP32C5_MAC_ADDRESS_TEXT_MAX_LENGTH];

	state = esp32c5GetState(WIRELESS_DEVICE);
	if (state == NULL) {
		if (gWirelessStarted) {
			gWirelessBleState = BLE_STATE_ERROR;
		}
		return;
	}

	if (!gWirelessStarted) {
		gWirelessBleState = BLE_STATE_IDLE;
		return;
	}

	if (state->runState == ESP32C5_RUN_ERROR) {
		gWirelessBleState = BLE_STATE_ERROR;
		return;
	}

	if (!state->isReady) {
		gWirelessBleState = BLE_STATE_INITIALIZING;
		return;
	}

	if (state->isBleConnected) {
		gWirelessBleState = BLE_STATE_CONNECTED;
	} else if (gWirelessBleConnected) {
		gWirelessBleState = BLE_STATE_DISCONNECTED;
	} else {
		gWirelessBleState = BLE_WAITING_CONNECTION;
	}

	if (!gWirelessReadyLogged) {
		LOG_I(WIRELESS_LOG_TAG, "esp32c5 ready ble=%s", WIRELESS_BLE_NAME);
		gWirelessReadyLogged = true;
	}

	if (!gWirelessMacLogged && state->hasMacAddress &&
		esp32c5GetCachedMac(WIRELESS_DEVICE, macAddress, (uint16_t)sizeof(macAddress))) {
		LOG_I(WIRELESS_LOG_TAG, "ble mac: %s", macAddress);
		gWirelessMacLogged = true;
	}

	if (state->isBleConnected != gWirelessBleConnected) {
		gWirelessBleConnected = state->isBleConnected;
		LOG_I(WIRELESS_LOG_TAG, "ble %s", gWirelessBleConnected ? "connected" : "disconnected");
	}
}

enum bleState wirelessGetBleState(void)
{
	wirelessUpdateState();
	return gWirelessBleState;
}

bool wirelessIsReady(void)
{
	wirelessUpdateState();
	return esp32c5IsReady(WIRELESS_DEVICE);
}

bool wirelessInit(void)
{
	wirelessService();
	return gWirelessBleState != BLE_STATE_ERROR;
}

void wirelessProcess(void)
{
	wirelessService();
}

/**************************End of file********************************/

