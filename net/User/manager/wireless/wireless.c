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

#include <stdio.h>
#include <string.h>

#include "../iotmanager/cprsensor_protocol.h"
#include "../iotmanager/iotmanager.h"
#include "../iotmanager/protcolmgr.h"
#include "../../../rep/module/esp32c5/esp32c5.h"
#include "../../../rep/module/esp32c5/esp32c5_http.h"
#include "../../../rep/module/esp32c5/esp32c5_mqtt.h"
#include "../../../rep/module/esp32c5/esp32c5_wifi.h"
#include "../../../rep/service/log/log.h"
#include "../../../rep/service/rtos/rtos.h"
#include "../../../rep/service/vfs/vfs.h"
#include "../../../rep/tools/aes/aes.h"
#include "../../../rep/tools/jsonparser/jsonparser.h"
#include "../../../rep/tools/md5/md5.h"

#define WIRELESS_LOG_TAG                 "wireless"
#define WIRELESS_DEVICE                  ESP32C5_DEV0
#define WIRELESS_RETRY_LOG_MS            1000U
#define WIRELESS_BLE_NAME                "Primedic-VENT-0001"
#define WIRELESS_BLE_ECHO_MAX_LEN        512U
#define WIRELESS_TX_BUFFER_SIZE          640U
#define WIRELESS_AES_KEY_SIZE            MD5_DIGEST_SIZE
#define WIRELESS_STORAGE_MOUNT_PATH      "/mem"

static bool gWirelessConfigured = false;
static bool gWirelessStarted = false;
static bool gWirelessBleEnabled = false;
static bool gWirelessBleAdvertisingEnabled = false;
static bool gWirelessBleCommandPending = false;
static bool gWirelessWifiEnabled = true;
static bool gWirelessWifiOffRequested = false;
static bool gWirelessWifiOffMqttDone = false;
static bool gWirelessWifiOffDisconnectDone = false;
static bool gWirelessWifiOffModeDone = false;
static bool gWirelessWifiAutoConnectGuardPending = false;
static bool gWirelessWifiAutoConnectGuardDone = false;
static bool gWirelessMqttEnabled = false;
static bool gWirelessReadyLogged = false;
static bool gWirelessMacLogged = false;
static bool gWirelessBleConnected = false;
static bool gWirelessWifiConfigValid = false;
static bool gWirelessWifiModeApplied = false;
static bool gWirelessWifiModePending = false;
static bool gWirelessWifiJoinPending = false;
static bool gWirelessWifiConnected = false;
static bool gWirelessWifiGotIp = false;
static bool gWirelessStorageLoaded = false;
static bool gWirelessIotHttpPending = false;
static bool gWirelessIotMqttUserCfgPending = false;
static bool gWirelessIotMqttConnPending = false;
static bool gWirelessIotMqttQueryPending = false;
static bool gWirelessIotMqttSubPending = false;
static bool gWirelessIotMqttUserCfgReady = false;
static bool gWirelessIotKeyReady = false;
static bool gWirelessIotMqttReady = false;
static bool gWirelessIotMqttSubReady = false;
static bool gWirelessIotMqttConnectedUrcSeen = false;
static bool gWirelessIotManualRetryNeeded = false;
static uint32_t gWirelessLastWarnTick = 0U;
static uint32_t gWirelessStorageWaitLogTick = 0U;
static uint32_t gWirelessIotNextRetryTick = 0U;
static eBleState gWirelessBleState = BLE_STATE_IDLE;
static eWifiState gWirelessWifiState = WIFI_STATE_IDLE;
static eWirelessIotState gWirelessIotState = WIRELESS_IOT_STATE_IDLE;
static uint8_t gWirelessBleRxServiceIndex = 0U;
static uint8_t gWirelessBleRxCharIndex = 0U;
static uint8_t gWirelessBleTxServiceIndex = 0U;
static uint8_t gWirelessBleTxCharIndex = 0U;
static bool gWirelessTxPending = false;
static uint16_t gWirelessTxLen = 0U;
static uint8_t gWirelessTxBuffer[WIRELESS_TX_BUFFER_SIZE];
static bool gWirelessMqttTxInFlight = false;
static uint16_t gWirelessMqttTxLen = 0U;
static uint8_t gWirelessMqttTxBuffer[WIRELESS_TX_BUFFER_SIZE];
static bool gWirelessProtocolHandshakeDone = false;
static bool gWirelessCipherReady = false;
static uint8_t gWirelessAesKey[WIRELESS_AES_KEY_SIZE];
static char gWirelessWifiSsid[WIRELESS_WIFI_SSID_MAX_LEN + 1U];
static char gWirelessWifiPassword[WIRELESS_WIFI_PASSWORD_MAX_LEN + 1U];
static char gWirelessIotSn[WIRELESS_IOT_SN_MAX_LEN + 1U];
static char gWirelessIotKey[WIRELESS_IOT_KEY_MAX_LEN + 1U];
static char gWirelessIotHttpUrl[WIRELESS_IOT_URL_MAX_LEN + 1U];
static char gWirelessIotMqttHost[WIRELESS_IOT_HOST_MAX_LEN + 1U];
static char gWirelessIotMqttUser[WIRELESS_IOT_USER_MAX_LEN + 1U];
static char gWirelessIotMqttClientId[WIRELESS_IOT_USER_MAX_LEN + 1U];
static char gWirelessIotMqttTopic[WIRELESS_IOT_TOPIC_MAX_LEN + 1U];
static char gWirelessIotMqttSubTopic[WIRELESS_IOT_TOPIC_MAX_LEN + 1U];
static char gWirelessIotHttpPayload[WIRELESS_IOT_HTTP_PAYLOAD_LEN];
static char gWirelessIotHttpResponse[WIRELESS_IOT_HTTP_RESPONSE_LEN + 1U];
static uint16_t gWirelessIotHttpResponseLen = 0U;
static uint16_t gWirelessIotMqttPort = 1883U;
static uint8_t gWirelessIotMqttState = 0U;

static bool wirelessCopyText(char *buffer, uint16_t bufferSize, const char *text);
static bool wirelessCopyBytesAsText(char *buffer, uint16_t bufferSize, const uint8_t *text, uint16_t textLen);
static void wirelessTrimText(char *text);
static bool wirelessReadTextFile(const char *path, char *buffer, uint16_t bufferSize);
static bool wirelessWriteTextFile(const char *path, const char *text);
static bool wirelessReadExistingTextFile(const char *path, char *buffer, uint16_t bufferSize);
static bool wirelessTryParseU16Text(const char *text, uint16_t *value);
static bool wirelessTryParseMqttState(const uint8_t *lineBuf, uint16_t lineLen, uint8_t *state);
static bool wirelessCopyMacCompact(char *buffer, uint16_t bufferSize, const char *macText);
static bool wirelessBuildDefaultSn(char *buffer, uint16_t bufferSize);
static void wirelessFillDefaultStorageConfig(void);
static void wirelessBackupStorageValueIfNeeded(const char *backupPath, const char *currentValue, const char *replacementValue);
static void wirelessApplyLocalIotConfig(void);
static bool wirelessBuildHttpAuthPayload(char *buffer, uint16_t bufferSize);
static bool wirelessBuildMqttLogin(char *username, uint16_t usernameSize, char *password, uint16_t passwordSize);
static bool wirelessLoadStorageConfig(void);
static bool wirelessMatchPrefix(const uint8_t *buffer, uint16_t length, const char *text);
static bool wirelessTryParseUint(const uint8_t *buffer, uint16_t length, uint16_t *value);
static void wirelessFillRawCodec(stCprsensorProtocolCodecCfg *codecCfg);
static void wirelessFillCipherCodec(stCprsensorProtocolCodecCfg *codecCfg);
static bool wirelessAesTransform(void *userData, uint8_t *buffer, uint16_t length, bool encryptMode);
static bool wirelessAesEncrypt(void *userData, uint8_t *buffer, uint16_t length);
static bool wirelessAesDecrypt(void *userData, uint8_t *buffer, uint16_t length);
static bool wirelessParseMacString(const char *text, uint8_t *mac, uint16_t macSize);
static bool wirelessTryInitCipherKey(void);
static bool wirelessParseIncomingFrame(const uint8_t *buffer,
					   uint16_t length,
					   stCprsensorProtocolFrameView *frameView,
					   uint8_t *payloadBuffer,
					   uint16_t payloadBufferSize);
static bool wirelessValidateHandshakePayload(const stCprsensorProtocolFrameView *frameView);
static bool wirelessForwardProtocolFrame(const uint8_t *buffer, uint16_t length);
static bool wirelessCanSendPendingFrame(const uint8_t *buffer, uint16_t length);
static void wirelessResetProtocolState(void);
static void wirelessLogTxCccdWrite(uint16_t serviceIndex,
				   uint16_t charIndex,
				   uint16_t descIndex,
				   const uint8_t *dataBuf,
				   uint16_t dataLen);
static bool wirelessHandleWriteUrc(const uint8_t *lineBuf, uint16_t lineLen);
static bool wirelessIsWifiUrc(const uint8_t *lineBuf, uint16_t lineLen);
static void wirelessHandleWifiUrc(const uint8_t *lineBuf, uint16_t lineLen);
static bool wirelessIsMqttUrc(const uint8_t *lineBuf, uint16_t lineLen);
static void wirelessHandleMqttUrc(const uint8_t *lineBuf, uint16_t lineLen);
static bool wirelessEsp32c5UrcMatcher(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static void wirelessEsp32c5UrcHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static bool wirelessConfigureIfNeeded(void);
static void wirelessUpdateIotState(void);
static void wirelessResetMqttRuntime(void);
static bool wirelessSubmitSimpleCommand(const char *cmdText);
static void wirelessServiceBleSwitch(void);
static bool wirelessServiceWifiOff(const stEsp32c5Info *info);
static void wirelessServiceWifi(void);
static void wirelessIotHttpLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static void wirelessIotMqttQueryLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static void wirelessIotMqttSubscribeLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static bool wirelessTryParseHttpKey(void);
static bool wirelessSubmitHttpAuth(void);
static bool wirelessSubmitMqttUserCfg(void);
static bool wirelessSubmitMqttConnect(void);
static bool wirelessSubmitMqttQuery(void);
static bool wirelessSubmitMqttSubscribe(void);
static void wirelessEnterIotError(uint32_t nowTick, const char *logText);
static void wirelessScheduleIotRetry(uint32_t nowTick, const char *logText);
static void wirelessServiceIot(void);
static void wirelessFlushTx(void);
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

static bool wirelessCopyBytesAsText(char *buffer, uint16_t bufferSize, const uint8_t *text, uint16_t textLen)
{
	if ((buffer == NULL) || (bufferSize == 0U) || (text == NULL) || (textLen >= bufferSize)) {
		return false;
	}

	if (textLen > 0U) {
		(void)memcpy(buffer, text, textLen);
	}
	buffer[textLen] = '\0';
	return true;
}

static void wirelessTrimText(char *text)
{
	uint16_t length;

	if (text == NULL) {
		return;
	}

	while ((*text == ' ') || (*text == '\t') || (*text == '\r') || (*text == '\n')) {
		(void)memmove(text, &text[1], strlen(text));
	}

	length = (uint16_t)strlen(text);
	while ((length > 0U) &&
		((text[length - 1U] == ' ') || (text[length - 1U] == '\t') ||
		 (text[length - 1U] == '\r') || (text[length - 1U] == '\n'))) {
		text[--length] = '\0';
	}
}

static bool wirelessReadTextFile(const char *path, char *buffer, uint16_t bufferSize)
{
	uint32_t actualSize;

	if ((path == NULL) || (buffer == NULL) || (bufferSize == 0U)) {
		return false;
	}

	buffer[0] = '\0';
	actualSize = 0U;
	if (!vfsReadFile(path, buffer, (uint32_t)(bufferSize - 1U), &actualSize)) {
		return false;
	}

	if (actualSize >= bufferSize) {
		return false;
	}

	buffer[actualSize] = '\0';
	wirelessTrimText(buffer);
	return buffer[0] != '\0';
}

static bool wirelessWriteTextFile(const char *path, const char *text)
{
	if ((path == NULL) || (text == NULL) || (text[0] == '\0')) {
		return false;
	}

	return vfsWriteFile(path, text, (uint32_t)strlen(text));
}

static bool wirelessReadExistingTextFile(const char *path, char *buffer, uint16_t bufferSize)
{
	if ((path == NULL) || !vfsExists(path)) {
		if ((buffer != NULL) && (bufferSize > 0U)) {
			buffer[0] = '\0';
		}
		return false;
	}

	return wirelessReadTextFile(path, buffer, bufferSize);
}

static bool wirelessTryParseU16Text(const char *text, uint16_t *value)
{
	uint32_t parsed;

	if ((text == NULL) || (value == NULL) || (text[0] == '\0')) {
		return false;
	}

	parsed = 0U;
	while (*text != '\0') {
		if ((*text < '0') || (*text > '9')) {
			return false;
		}
		parsed = (parsed * 10U) + (uint32_t)(*text - '0');
		if (parsed > 65535UL) {
			return false;
		}
		text++;
	}

	*value = (uint16_t)parsed;
	return true;
}

static bool wirelessTryParseMqttState(const uint8_t *lineBuf, uint16_t lineLen, uint8_t *state)
{
	return esp32c5MqttTryParseConnState(lineBuf, lineLen, state);
}

static bool wirelessCopyMacCompact(char *buffer, uint16_t bufferSize, const char *macText)
{
	uint16_t outIndex;
	char ch;

	if ((buffer == NULL) || (bufferSize < 13U) || (macText == NULL)) {
		return false;
	}

	outIndex = 0U;
	while (*macText != '\0') {
		ch = *macText++;
		if ((ch == ':') || (ch == '-') || (ch == ' ')) {
			continue;
		}
		if ((ch >= 'a') && (ch <= 'f')) {
			ch = (char)(ch - ('a' - 'A'));
		}
		if (!(((ch >= '0') && (ch <= '9')) || ((ch >= 'A') && (ch <= 'F')))) {
			return false;
		}
		if (outIndex >= 12U) {
			return false;
		}
		buffer[outIndex++] = ch;
	}

	if (outIndex != 12U) {
		return false;
	}
	buffer[outIndex] = '\0';
	return true;
}

static bool wirelessBuildDefaultSn(char *buffer, uint16_t bufferSize)
{
	char macText[ESP32C5_MAC_ADDRESS_TEXT_MAX_LENGTH];
	char macCompact[13];
	int length;

	if ((buffer == NULL) || (bufferSize == 0U)) {
		return false;
	}

	if (!wirelessGetMacAddress(macText, (uint16_t)sizeof(macText)) ||
		!wirelessCopyMacCompact(macCompact, (uint16_t)sizeof(macCompact), macText)) {
		return false;
	}

	length = snprintf(buffer, bufferSize, "CPR%s", macCompact);
	return (length > 0) && ((uint16_t)length < bufferSize);
}

static void wirelessFillDefaultStorageConfig(void)
{
	char defaultSn[WIRELESS_IOT_SN_MAX_LEN + 1U];

	(void)vfsMkdir("/mem/setting");
	(void)vfsMkdir("/mem/dev");

	if (gWirelessIotSn[0] == '\0') {
		if (wirelessBuildDefaultSn(defaultSn, (uint16_t)sizeof(defaultSn))) {
			(void)wirelessCopyText(gWirelessIotSn, (uint16_t)sizeof(gWirelessIotSn), defaultSn);
			(void)wirelessWriteTextFile(WIRELESS_IOT_SN_PATH, gWirelessIotSn);
			LOG_I(WIRELESS_LOG_TAG, "storage default sn=%s", gWirelessIotSn);
		}
	}
	if (gWirelessIotHttpUrl[0] == '\0') {
		(void)wirelessCopyText(gWirelessIotHttpUrl, (uint16_t)sizeof(gWirelessIotHttpUrl), WIRELESS_IOT_DEFAULT_HTTP_URL);
		(void)wirelessWriteTextFile(WIRELESS_IOT_HTTP_URL_PATH, gWirelessIotHttpUrl);
		LOG_I(WIRELESS_LOG_TAG, "storage default http url");
	}
	if (gWirelessIotMqttHost[0] == '\0') {
		(void)wirelessCopyText(gWirelessIotMqttHost, (uint16_t)sizeof(gWirelessIotMqttHost), WIRELESS_IOT_DEFAULT_MQTT_HOST);
		(void)wirelessWriteTextFile(WIRELESS_IOT_MQTT_HOST_PATH, gWirelessIotMqttHost);
		LOG_I(WIRELESS_LOG_TAG, "storage default mqtt host");
	}
	if (!vfsExists(WIRELESS_IOT_MQTT_PORT_PATH)) {
		(void)wirelessWriteTextFile(WIRELESS_IOT_MQTT_PORT_PATH, WIRELESS_IOT_DEFAULT_MQTT_PORT);
	}
}

static void wirelessBackupStorageValueIfNeeded(const char *backupPath, const char *currentValue, const char *replacementValue)
{
	if ((backupPath == NULL) || (currentValue == NULL) || (currentValue[0] == '\0') || vfsExists(backupPath)) {
		return;
	}

	if ((replacementValue != NULL) && (strcmp(currentValue, replacementValue) == 0)) {
		return;
	}

	(void)wirelessWriteTextFile(backupPath, currentValue);
}

static void wirelessApplyLocalIotConfig(void)
{
#if (WIRELESS_IOT_LOCAL_TEST_MODE == 1)
	char portText[8];
	uint16_t localPort;

	wirelessBackupStorageValueIfNeeded(WIRELESS_IOT_HTTP_URL_BACKUP_PATH,
					      gWirelessIotHttpUrl,
					      WIRELESS_IOT_LOCAL_HTTP_URL);
	wirelessBackupStorageValueIfNeeded(WIRELESS_IOT_MQTT_HOST_BACKUP_PATH,
					      gWirelessIotMqttHost,
					      WIRELESS_IOT_LOCAL_MQTT_HOST);
	if (snprintf(portText, sizeof(portText), "%u", (unsigned int)gWirelessIotMqttPort) > 0) {
		wirelessBackupStorageValueIfNeeded(WIRELESS_IOT_MQTT_PORT_BACKUP_PATH,
					      portText,
					      WIRELESS_IOT_LOCAL_MQTT_PORT);
	}
	wirelessBackupStorageValueIfNeeded(WIRELESS_IOT_MQTT_KEY_BACKUP_PATH, gWirelessIotKey, NULL);

	if ((gWirelessIotHttpUrl[0] == '\0') || (strcmp(gWirelessIotHttpUrl, WIRELESS_IOT_LOCAL_HTTP_URL) != 0)) {
		(void)wirelessCopyText(gWirelessIotHttpUrl, (uint16_t)sizeof(gWirelessIotHttpUrl), WIRELESS_IOT_LOCAL_HTTP_URL);
		(void)wirelessWriteTextFile(WIRELESS_IOT_HTTP_URL_PATH, gWirelessIotHttpUrl);
		LOG_I(WIRELESS_LOG_TAG, "local test http url active");
	}

	if ((gWirelessIotMqttHost[0] == '\0') || (strcmp(gWirelessIotMqttHost, WIRELESS_IOT_LOCAL_MQTT_HOST) != 0)) {
		(void)wirelessCopyText(gWirelessIotMqttHost, (uint16_t)sizeof(gWirelessIotMqttHost), WIRELESS_IOT_LOCAL_MQTT_HOST);
		(void)wirelessWriteTextFile(WIRELESS_IOT_MQTT_HOST_PATH, gWirelessIotMqttHost);
		LOG_I(WIRELESS_LOG_TAG, "local test mqtt host active");
	}

	localPort = gWirelessIotMqttPort;
	if (wirelessTryParseU16Text(WIRELESS_IOT_LOCAL_MQTT_PORT, &localPort) &&
		((gWirelessIotMqttPort == 0U) || (gWirelessIotMqttPort != localPort))) {
		gWirelessIotMqttPort = localPort;
		(void)wirelessWriteTextFile(WIRELESS_IOT_MQTT_PORT_PATH, WIRELESS_IOT_LOCAL_MQTT_PORT);
		LOG_I(WIRELESS_LOG_TAG, "local test mqtt port active=%u", (unsigned int)gWirelessIotMqttPort);
	}

	if (vfsExists(WIRELESS_IOT_MQTT_KEY_PATH)) {
		(void)vfsDelete(WIRELESS_IOT_MQTT_KEY_PATH);
	}
	gWirelessIotKey[0] = '\0';
	gWirelessIotKeyReady = false;
#endif
}

static bool wirelessBuildHttpAuthPayload(char *buffer, uint16_t bufferSize)
{
	char macText[ESP32C5_MAC_ADDRESS_TEXT_MAX_LENGTH];
	char macCompact[13];
	char signSource[128];
	char signHex[WIRELESS_IOT_MD5_HEX_LEN];
	int length;

	if ((buffer == NULL) || (bufferSize == 0U) || (gWirelessIotSn[0] == '\0')) {
		return false;
	}

	if (!wirelessGetMacAddress(macText, (uint16_t)sizeof(macText)) ||
		!wirelessCopyMacCompact(macCompact, (uint16_t)sizeof(macCompact), macText)) {
		return false;
	}

	length = snprintf(signSource,
				 sizeof(signSource),
				 "%s%s%s%s",
				 gWirelessIotSn,
				 macCompact,
				 WIRELESS_IOT_HTTP_RANDOM,
				 WIRELESS_IOT_PRODUCT_SECRET);
	if ((length <= 0) || ((uint16_t)length >= sizeof(signSource)) ||
		(md5StringToHex32(signSource, signHex, 1U) != MD5_STATUS_OK)) {
		return false;
	}

	length = snprintf(buffer,
				 bufferSize,
				 "{\"deviceId\":\"%s\",\"moduleId\":\"%s\",\"random\":\"%s\",\"sign\":\"%s\"}",
				 gWirelessIotSn,
				 macCompact,
				 WIRELESS_IOT_HTTP_RANDOM,
				 signHex);
	return (length > 0) && ((uint16_t)length < bufferSize);
}

static bool wirelessBuildMqttLogin(char *username, uint16_t usernameSize, char *password, uint16_t passwordSize)
{
	char signSource[160];
	uint32_t timestampMs;
	int length;

	if ((username == NULL) || (password == NULL) || (usernameSize == 0U) || (passwordSize < WIRELESS_IOT_MD5_HEX_LEN) ||
		(gWirelessIotSn[0] == '\0') || (gWirelessIotKey[0] == '\0')) {
		return false;
	}

	timestampMs = repRtosGetTickMs();
	length = snprintf(username, usernameSize, "%s|%lu", gWirelessIotSn, (unsigned long)timestampMs);
	if ((length <= 0) || ((uint16_t)length >= usernameSize)) {
		return false;
	}

	length = snprintf(signSource, sizeof(signSource), "%s|%s", username, gWirelessIotKey);
	if ((length <= 0) || ((uint16_t)length >= sizeof(signSource))) {
		return false;
	}

	return md5StringToHex32(signSource, password, 0U) == MD5_STATUS_OK;
}

static bool wirelessLoadStorageConfig(void)
{
	char portText[8];
	bool hasSn;
	bool hasHttpUrl;
	bool hasMqttHost;

	if (gWirelessStorageLoaded) {
		return true;
	}

	if (!vfsIsMounted(WIRELESS_STORAGE_MOUNT_PATH) && !vfsMount(WIRELESS_STORAGE_MOUNT_PATH)) {
		return false;
	}

	gWirelessIotSn[0] = '\0';
	gWirelessIotHttpUrl[0] = '\0';
	gWirelessIotKey[0] = '\0';
	gWirelessIotMqttHost[0] = '\0';
	gWirelessIotMqttUser[0] = '\0';
	gWirelessIotMqttClientId[0] = '\0';
	gWirelessIotMqttTopic[0] = '\0';
	gWirelessIotMqttSubTopic[0] = '\0';

	if (!wirelessReadExistingTextFile(WIRELESS_IOT_SN_PATH, gWirelessIotSn, (uint16_t)sizeof(gWirelessIotSn))) {
		(void)wirelessReadExistingTextFile(WIRELESS_IOT_SN_LEGACY_PATH, gWirelessIotSn, (uint16_t)sizeof(gWirelessIotSn));
	}
	(void)wirelessReadExistingTextFile(WIRELESS_IOT_HTTP_URL_PATH, gWirelessIotHttpUrl, (uint16_t)sizeof(gWirelessIotHttpUrl));
	(void)wirelessReadExistingTextFile(WIRELESS_IOT_MQTT_KEY_PATH, gWirelessIotKey, (uint16_t)sizeof(gWirelessIotKey));
	(void)wirelessReadExistingTextFile(WIRELESS_IOT_MQTT_HOST_PATH, gWirelessIotMqttHost, (uint16_t)sizeof(gWirelessIotMqttHost));
	(void)wirelessReadExistingTextFile(WIRELESS_IOT_MQTT_USER_PATH, gWirelessIotMqttUser, (uint16_t)sizeof(gWirelessIotMqttUser));
	(void)wirelessReadExistingTextFile(WIRELESS_IOT_MQTT_CLIENT_ID_PATH,
				      gWirelessIotMqttClientId,
				      (uint16_t)sizeof(gWirelessIotMqttClientId));
	(void)wirelessReadExistingTextFile(WIRELESS_IOT_MQTT_TOPIC_PATH,
				      gWirelessIotMqttTopic,
				      (uint16_t)sizeof(gWirelessIotMqttTopic));
	(void)wirelessReadExistingTextFile(WIRELESS_IOT_MQTT_SUB_TOPIC_PATH,
				      gWirelessIotMqttSubTopic,
				      (uint16_t)sizeof(gWirelessIotMqttSubTopic));
	if (wirelessReadExistingTextFile(WIRELESS_IOT_MQTT_PORT_PATH, portText, (uint16_t)sizeof(portText))) {
		(void)wirelessTryParseU16Text(portText, &gWirelessIotMqttPort);
	}

	wirelessApplyLocalIotConfig();

	wirelessFillDefaultStorageConfig();

	if (gWirelessIotMqttClientId[0] == '\0') {
		(void)wirelessCopyText(gWirelessIotMqttClientId,
				    (uint16_t)sizeof(gWirelessIotMqttClientId),
				    (gWirelessIotSn[0] != '\0') ? gWirelessIotSn : "cprsensor");
	}
	if (gWirelessIotMqttUser[0] == '\0') {
		(void)wirelessCopyText(gWirelessIotMqttUser,
				    (uint16_t)sizeof(gWirelessIotMqttUser),
				    gWirelessIotMqttClientId);
	}
	if (gWirelessIotMqttTopic[0] == '\0') {
		int topicLen;

		topicLen = -1;
		if ((gWirelessIotSn[0] == '\0') ||
			((topicLen = snprintf(gWirelessIotMqttTopic,
					     sizeof(gWirelessIotMqttTopic),
					     "CPR/%s/event/transfer",
					     gWirelessIotSn)) <= 0) ||
			((uint16_t)topicLen >= sizeof(gWirelessIotMqttTopic))) {
			(void)wirelessCopyText(gWirelessIotMqttTopic,
					    (uint16_t)sizeof(gWirelessIotMqttTopic),
					    "CPR/cprsensor/event/transfer");
		}
	}
	if (gWirelessIotMqttSubTopic[0] == '\0') {
		int topicLen;

		topicLen = -1;
		if ((gWirelessIotSn[0] == '\0') ||
			((topicLen = snprintf(gWirelessIotMqttSubTopic,
					     sizeof(gWirelessIotMqttSubTopic),
					     "CPR/%s/cmd/transfer",
					     gWirelessIotSn)) <= 0) ||
			((uint16_t)topicLen >= sizeof(gWirelessIotMqttSubTopic))) {
			(void)wirelessCopyText(gWirelessIotMqttSubTopic,
					    (uint16_t)sizeof(gWirelessIotMqttSubTopic),
					    "CPR/cprsensor/cmd/transfer");
		}
	}

	gWirelessIotKeyReady = gWirelessIotKey[0] != '\0';
	hasSn = gWirelessIotSn[0] != '\0';
	hasHttpUrl = gWirelessIotHttpUrl[0] != '\0';
	hasMqttHost = gWirelessIotMqttHost[0] != '\0';
	if (!hasSn || !hasMqttHost || (!gWirelessIotKeyReady && !hasHttpUrl)) {
		uint32_t nowTickMs;

		nowTickMs = repRtosGetTickMs();
		if ((nowTickMs - gWirelessStorageWaitLogTick) >= WIRELESS_RETRY_LOG_MS) {
			gWirelessStorageWaitLogTick = nowTickMs;
			LOG_W(WIRELESS_LOG_TAG,
			      "storage wait wifi=%u sn=%u key=%u http=%u mqttHost=%u",
			      gWirelessWifiConfigValid ? 1U : 0U,
			      hasSn ? 1U : 0U,
			      gWirelessIotKeyReady ? 1U : 0U,
			      hasHttpUrl ? 1U : 0U,
			      hasMqttHost ? 1U : 0U);
		}
		return false;
	}

	gWirelessStorageLoaded = true;
	LOG_I(WIRELESS_LOG_TAG,
	      "storage cfg wifi=%u sn=%u key=%u http=%u mqttHost=%u id=%s",
	      gWirelessWifiConfigValid ? 1U : 0U,
	      (gWirelessIotSn[0] != '\0') ? 1U : 0U,
	      gWirelessIotKeyReady ? 1U : 0U,
	      (gWirelessIotHttpUrl[0] != '\0') ? 1U : 0U,
	      (gWirelessIotMqttHost[0] != '\0') ? 1U : 0U,
	      gWirelessIotSn);
	return true;
}

static void wirelessResetMqttRuntime(void)
{
	gWirelessIotHttpPending = false;
	gWirelessIotMqttUserCfgPending = false;
	gWirelessIotMqttConnPending = false;
	gWirelessIotMqttQueryPending = false;
	gWirelessIotMqttSubPending = false;
	gWirelessIotMqttUserCfgReady = false;
	gWirelessIotMqttReady = false;
	gWirelessIotMqttSubReady = false;
	gWirelessIotMqttConnectedUrcSeen = false;
	gWirelessIotManualRetryNeeded = false;
	gWirelessIotNextRetryTick = 0U;
	gWirelessIotMqttState = 0U;
	gWirelessMqttTxInFlight = false;
	gWirelessMqttTxLen = 0U;
	gWirelessIotState = WIRELESS_IOT_STATE_IDLE;
}

static bool wirelessSubmitSimpleCommand(const char *cmdText)
{
	eEsp32c5Status status;

	if (cmdText == NULL) {
		return false;
	}

	status = esp32c5SubmitTextCommand(WIRELESS_DEVICE, cmdText);
	return status == ESP32C5_STATUS_OK;
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

static void wirelessFillRawCodec(stCprsensorProtocolCodecCfg *codecCfg)
{
	if (codecCfg == NULL) {
		return;
	}

	(void)memset(codecCfg, 0, sizeof(*codecCfg));
	codecCfg->crc.polynomial = 0x1021U;
	codecCfg->crc.initValue = 0xFFFFU;
	codecCfg->crc.xorOut = 0U;
	codecCfg->crc.reflectInput = false;
	codecCfg->crc.reflectOutput = false;
	codecCfg->cipher.enabled = false;
	codecCfg->cipher.blockSize = CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE;
}

static void wirelessFillCipherCodec(stCprsensorProtocolCodecCfg *codecCfg)
{
	wirelessFillRawCodec(codecCfg);
	if (codecCfg == NULL) {
		return;
	}

	codecCfg->cipher.enabled = gWirelessCipherReady;
	codecCfg->cipher.blockSize = CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE;
	codecCfg->cipher.encrypt = wirelessAesEncrypt;
	codecCfg->cipher.decrypt = wirelessAesDecrypt;
	codecCfg->cipher.userData = gWirelessAesKey;
}

static bool wirelessAesTransform(void *userData, uint8_t *buffer, uint16_t length, bool encryptMode)
{
	stAesContext context;

	if ((userData == NULL) || (buffer == NULL) || (length == 0U) ||
		((length % CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE) != 0U)) {
		return false;
	}

	if (aesInit(&context, AES_TYPE_128, AES_MODE_ECB, (const uint8_t *)userData, NULL) != AES_STATUS_OK) {
		return false;
	}

	if (encryptMode) {
		return aesEncrypt(&context, buffer, buffer, (uint32_t)length) == AES_STATUS_OK;
	}

	return aesDecrypt(&context, buffer, buffer, (uint32_t)length) == AES_STATUS_OK;
}

static bool wirelessAesEncrypt(void *userData, uint8_t *buffer, uint16_t length)
{
	return wirelessAesTransform(userData, buffer, length, true);
}

static bool wirelessAesDecrypt(void *userData, uint8_t *buffer, uint16_t length)
{
	return wirelessAesTransform(userData, buffer, length, false);
}

static bool wirelessParseMacString(const char *text, uint8_t *mac, uint16_t macSize)
{
	uint16_t index;
	uint8_t highNibble;
	uint8_t value;
	char ch;
	bool highReady;

	if ((text == NULL) || (mac == NULL) || (macSize < CPRSENSOR_PROTOCOL_MAC_LEN)) {
		return false;
	}

	index = 0U;
	highNibble = 0U;
	highReady = false;
	while (*text != '\0') {
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

		if (!highReady) {
			highNibble = (uint8_t)(value << 4U);
			highReady = true;
		} else {
			if (index >= CPRSENSOR_PROTOCOL_MAC_LEN) {
				return false;
			}
			mac[index++] = (uint8_t)(highNibble | value);
			highReady = false;
		}
	}

	return (!highReady) && (index == CPRSENSOR_PROTOCOL_MAC_LEN);
}

static bool wirelessTryInitCipherKey(void)
{
	char macText[32];
	uint8_t macBytes[CPRSENSOR_PROTOCOL_MAC_LEN];

	if (gWirelessCipherReady) {
		return true;
	}

	if (!wirelessGetMacAddress(macText, (uint16_t)sizeof(macText))) {
		return false;
	}

	if (!wirelessParseMacString(macText, macBytes, (uint16_t)sizeof(macBytes))) {
		return false;
	}

	if (md5CalcData(macBytes, (uint32_t)sizeof(macBytes), gWirelessAesKey) != MD5_STATUS_OK) {
		return false;
	}

	gWirelessCipherReady = true;
	return true;
}

static bool wirelessParseIncomingFrame(const uint8_t *buffer,
					   uint16_t length,
					   stCprsensorProtocolFrameView *frameView,
					   uint8_t *payloadBuffer,
					   uint16_t payloadBufferSize)
{
	stCprsensorProtocolCodecCfg rawCodec;
	stCprsensorProtocolCodecCfg cipherCodec;
	stCprsensorProtocolFrameView cipherView;

	if ((buffer == NULL) || (frameView == NULL) || (payloadBuffer == NULL) || (payloadBufferSize == 0U)) {
		return false;
	}

	wirelessFillRawCodec(&rawCodec);
	if (cprsensorProtocolParseFrame(buffer,
					 length,
					 &rawCodec,
					 payloadBuffer,
					 payloadBufferSize,
					 frameView) != CPRSENSOR_PROTOCOL_STATUS_OK) {
		return false;
	}

	if (!wirelessTryInitCipherKey()) {
		return frameView->cmd == CPRSENSOR_PROTOCOL_CMD_HANDSHAKE;
	}

	if ((frameView->encodedPayloadLen == 0U) ||
		((frameView->encodedPayloadLen % CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE) != 0U)) {
		return true;
	}

	wirelessFillCipherCodec(&cipherCodec);
	if (cprsensorProtocolParseFrame(buffer,
					 length,
					 &cipherCodec,
					 payloadBuffer,
					 payloadBufferSize,
					 &cipherView) == CPRSENSOR_PROTOCOL_STATUS_OK) {
		*frameView = cipherView;
	}

	return true;
}

static bool wirelessValidateHandshakePayload(const stCprsensorProtocolFrameView *frameView)
{
	char macText[32];
	uint8_t macBytes[CPRSENSOR_PROTOCOL_MAC_LEN];

	if ((frameView == NULL) || (frameView->cmd != CPRSENSOR_PROTOCOL_CMD_HANDSHAKE) ||
		(frameView->payload == NULL) || (frameView->payloadLen < CPRSENSOR_PROTOCOL_MAC_LEN)) {
		return false;
	}

	if (!wirelessGetMacAddress(macText, (uint16_t)sizeof(macText))) {
		return false;
	}

	if (!wirelessParseMacString(macText, macBytes, (uint16_t)sizeof(macBytes))) {
		return false;
	}

	return memcmp(frameView->payload, macBytes, CPRSENSOR_PROTOCOL_MAC_LEN) == 0;
}

static bool wirelessForwardProtocolFrame(const uint8_t *buffer, uint16_t length)
{
	stCprsensorProtocolFrameView frameView;
	uint8_t payloadBuffer[WIRELESS_TX_BUFFER_SIZE];

	if (!wirelessParseIncomingFrame(buffer,
					 length,
					 &frameView,
					 payloadBuffer,
					 (uint16_t)sizeof(payloadBuffer))) {
		LOG_W(WIRELESS_LOG_TAG, "ignore invalid protocol frame len=%u", (unsigned int)length);
		return false;
	}

	if (!gWirelessProtocolHandshakeDone) {
		if (frameView.cmd != CPRSENSOR_PROTOCOL_CMD_HANDSHAKE) {
			LOG_W(WIRELESS_LOG_TAG,
			      "ignore cmd=0x%02X before handshake",
			      (unsigned int)frameView.cmd);
			return false;
		}

		if (!wirelessValidateHandshakePayload(&frameView)) {
			LOG_W(WIRELESS_LOG_TAG, "ignore invalid handshake payload");
			return false;
		}

		gWirelessProtocolHandshakeDone = true;
		LOG_I(WIRELESS_LOG_TAG, "ble protocol handshake ok");
	}

	if (!protcolMgrPushReceivedData(IOT_MANAGER_LINK_BLE, buffer, length)) {
		LOG_W(WIRELESS_LOG_TAG, "ble rx forward failed len=%u", (unsigned int)length);
		return false;
	}

	return true;
}

static bool wirelessCanSendPendingFrame(const uint8_t *buffer, uint16_t length)
{
	if (gWirelessProtocolHandshakeDone) {
		return true;
	}

	if ((buffer == NULL) || (length < CPRSENSOR_PROTOCOL_FRAME_HEAD_SIZE) ||
		(buffer[0] != CPRSENSOR_PROTOCOL_FRAME_HEAD0) ||
		(buffer[1] != CPRSENSOR_PROTOCOL_FRAME_HEAD1)) {
		return false;
	}

	return buffer[3] == CPRSENSOR_PROTOCOL_CMD_HANDSHAKE;
}

static void wirelessResetProtocolState(void)
{
	repRtosEnterCritical();
	gWirelessProtocolHandshakeDone = false;
	gWirelessTxPending = false;
	gWirelessTxLen = 0U;
	repRtosExitCritical();
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

	status = ESP32C5_STATUS_OK;
	if (!wirelessForwardProtocolFrame(&lineBuf[commaPos[commaCount - 1U] + 1U], dataLen)) {
		status = ESP32C5_STATUS_ERROR;
	}
	if (status != ESP32C5_STATUS_OK) {
		LOG_W(WIRELESS_LOG_TAG,
		      "ble rx forward failed status=%d len=%u",
		      (int)status,
		      (unsigned int)dataLen);
		return true;
	}
	return true;
}

static bool wirelessIsWifiUrc(const uint8_t *lineBuf, uint16_t lineLen)
{
	return esp32c5WifiIsUrc(lineBuf, lineLen);
}

static void wirelessHandleWifiUrc(const uint8_t *lineBuf, uint16_t lineLen)
{
	if (wirelessMatchPrefix(lineBuf, lineLen, "WIFI CONNECTED")) {
		gWirelessWifiConnected = true;
		gWirelessWifiGotIp = false;
		gWirelessWifiState = WIFI_STATE_WAITING_CONNECTION;
		LOG_I(WIRELESS_LOG_TAG, "wifi connected");
		return;
	}

	if (wirelessMatchPrefix(lineBuf, lineLen, "WIFI GOT IP")) {
		gWirelessWifiConnected = true;
		gWirelessWifiGotIp = true;
		gWirelessWifiModeApplied = true;
		gWirelessWifiJoinPending = false;
		gWirelessWifiState = WIFI_STATE_CONNECTED;
		LOG_I(WIRELESS_LOG_TAG, "wifi got ip");
		return;
	}

	if (wirelessMatchPrefix(lineBuf, lineLen, "WIFI DISCONNECT") ||
		wirelessMatchPrefix(lineBuf, lineLen, "WIFI DISCONNECTED")) {
		gWirelessWifiConnected = false;
		gWirelessWifiGotIp = false;
		gWirelessWifiJoinPending = false;
		gWirelessIotMqttReady = false;
		gWirelessIotMqttSubReady = false;
		gWirelessIotMqttConnectedUrcSeen = false;
		gWirelessIotMqttConnPending = false;
		gWirelessIotMqttQueryPending = false;
		gWirelessIotMqttSubPending = false;
		gWirelessWifiState = gWirelessWifiConfigValid ? WIFI_STATE_DISCONNECTED : WIFI_STATE_IDLE;
		LOG_I(WIRELESS_LOG_TAG, "wifi disconnected");
	}
}

static bool wirelessIsMqttUrc(const uint8_t *lineBuf, uint16_t lineLen)
{
	return esp32c5MqttIsUrc(lineBuf, lineLen);
}

static void wirelessHandleMqttUrc(const uint8_t *lineBuf, uint16_t lineLen)
{
	const uint8_t *payload;
	uint16_t payloadLen;

	if (wirelessMatchPrefix(lineBuf, lineLen, "+MQTTCONNECTED:")) {
		gWirelessIotMqttUserCfgReady = true;
		gWirelessIotMqttConnectedUrcSeen = true;
		gWirelessIotMqttReady = true;
		gWirelessIotMqttState = 4U;
		gWirelessIotMqttUserCfgPending = false;
		gWirelessIotMqttConnPending = false;
		gWirelessIotMqttQueryPending = false;
		gWirelessIotManualRetryNeeded = false;
		gWirelessIotNextRetryTick = 0U;
		gWirelessIotState = WIRELESS_IOT_STATE_MQTT_READY;
		LOG_I(WIRELESS_LOG_TAG, "mqtt broker connected");
		return;
	}

	if (wirelessMatchPrefix(lineBuf, lineLen, "+MQTTDISCONNECTED:")) {
		gWirelessIotMqttConnectedUrcSeen = false;
		gWirelessIotMqttReady = false;
		gWirelessIotMqttSubReady = false;
		gWirelessIotMqttSubPending = false;
		gWirelessIotMqttState = 3U;
		gWirelessIotMqttConnPending = false;
		gWirelessIotMqttQueryPending = false;
		gWirelessIotState = gWirelessMqttEnabled ? WIRELESS_IOT_STATE_AUTH_READY : WIRELESS_IOT_STATE_IDLE;
		LOG_W(WIRELESS_LOG_TAG, "mqtt broker disconnected");
		return;
	}

	if (esp32c5MqttTryParseSubRecv(lineBuf, lineLen, &payload, &payloadLen)) {
		if (!protcolMgrPushReceivedData(IOT_MANAGER_LINK_WIFI, payload, payloadLen)) {
			LOG_W(WIRELESS_LOG_TAG, "mqtt rx forward failed len=%u", (unsigned int)payloadLen);
		}
		return;
	}

	if (wirelessMatchPrefix(lineBuf, lineLen, "+MQTTSUB:")) {
		gWirelessIotMqttSubPending = false;
		gWirelessIotMqttSubReady = true;
		LOG_I(WIRELESS_LOG_TAG, "mqtt subscribed topic=%s", gWirelessIotMqttSubTopic);
	}
}

static bool wirelessEsp32c5UrcMatcher(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
	(void)userData;
	return wirelessIsWifiUrc(lineBuf, lineLen) || wirelessIsMqttUrc(lineBuf, lineLen);
}

static void wirelessEsp32c5UrcHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
	(void)userData;

	if ((lineBuf == NULL) || (lineLen == 0U)) {
		return;
	}

	(void)wirelessHandleWriteUrc(lineBuf, lineLen);
	if (wirelessIsWifiUrc(lineBuf, lineLen)) {
		wirelessHandleWifiUrc(lineBuf, lineLen);
	}
	if (wirelessIsMqttUrc(lineBuf, lineLen)) {
		wirelessHandleMqttUrc(lineBuf, lineLen);
	}
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
	bleCfg.autoStartAdvertising = false;
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

	status = esp32c5SetUrcMatcher(WIRELESS_DEVICE, wirelessEsp32c5UrcMatcher, NULL);
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

static void wirelessUpdateIotState(void)
{
	stIotManagerLinkRuntime runtime;

	(void)memset(&runtime, 0, sizeof(runtime));
	runtime.linkId = IOT_MANAGER_LINK_BLE;
	runtime.installed = true;
	runtime.enabled = gWirelessBleEnabled;
	runtime.state = (gWirelessBleState == BLE_STATE_ERROR) ? IOT_MANAGER_LINK_STATE_ERROR :
			(!gWirelessBleEnabled ? IOT_MANAGER_LINK_STATE_DISABLED :
			((gWirelessBleState == BLE_STATE_CONNECTED) ? IOT_MANAGER_LINK_STATE_READY : IOT_MANAGER_LINK_STATE_INITING));
	runtime.moduleReady = (gWirelessBleState != BLE_STATE_IDLE) && (gWirelessBleState != BLE_STATE_INITIALIZING) &&
					 (gWirelessBleState != BLE_STATE_ERROR);
	runtime.peerConnected = (gWirelessBleState == BLE_STATE_CONNECTED);
	runtime.caps.supportBleLocal = true;
	(void)iotManagerUpdateLinkState(IOT_MANAGER_LINK_BLE, &runtime);

	(void)memset(&runtime, 0, sizeof(runtime));
	runtime.linkId = IOT_MANAGER_LINK_WIFI;
	runtime.installed = true;
	runtime.enabled = gWirelessWifiEnabled;
	runtime.moduleReady = (gWirelessBleState != BLE_STATE_IDLE) && (gWirelessBleState != BLE_STATE_INITIALIZING) &&
					 (gWirelessBleState != BLE_STATE_ERROR);
	runtime.netReady = (gWirelessWifiState == WIFI_STATE_CONNECTED);
	runtime.mqttAuthReady = runtime.netReady && gWirelessMqttEnabled && gWirelessIotKeyReady;
	runtime.mqttReady = runtime.netReady && gWirelessMqttEnabled && gWirelessIotMqttReady;
	runtime.tcpServerListening = false;
	runtime.caps.supportMqttAuthHttp = true;
	runtime.caps.supportMqtt = true;
	if (gWirelessWifiState == WIFI_STATE_ERROR) {
		runtime.state = IOT_MANAGER_LINK_STATE_ERROR;
	} else if (gWirelessWifiState == WIFI_STATE_CONNECTED) {
		runtime.state = IOT_MANAGER_LINK_STATE_NET_READY;
	} else if (gWirelessWifiState == WIFI_STATE_WAITING_CONNECTION) {
		runtime.state = IOT_MANAGER_LINK_STATE_NET_CONNECTING;
	} else if (runtime.enabled) {
		runtime.state = IOT_MANAGER_LINK_STATE_READY;
	} else {
		runtime.state = IOT_MANAGER_LINK_STATE_DISABLED;
	}
	(void)iotManagerUpdateLinkState(IOT_MANAGER_LINK_WIFI, &runtime);
}

static void wirelessServiceBleSwitch(void)
{
	const stEsp32c5Info *info;
	const stEsp32c5State *state;

	info = esp32c5GetInfo(WIRELESS_DEVICE);
	state = esp32c5GetState(WIRELESS_DEVICE);
	if ((info == NULL) || (state == NULL) || !state->isReady) {
		return;
	}

	if (info->isBusy) {
		return;
	}
	if (gWirelessBleCommandPending) {
		gWirelessBleCommandPending = false;
	}

	if (!gWirelessBleEnabled) {
		if (state->isBleConnected) {
			if (esp32c5DisconnectBle(WIRELESS_DEVICE) == ESP32C5_STATUS_OK) {
				gWirelessBleCommandPending = true;
				gWirelessBleAdvertisingEnabled = false;
			}
			return;
		}
		if (gWirelessBleAdvertisingEnabled || state->isBleAdvertising) {
			if (wirelessSubmitSimpleCommand("AT+BLEADVSTOP\r\n")) {
				gWirelessBleCommandPending = true;
				gWirelessBleAdvertisingEnabled = false;
			}
		}
		return;
	}

	if (!state->isBleConnected && !gWirelessBleAdvertisingEnabled && !state->isBleAdvertising) {
		if (wirelessSubmitSimpleCommand("AT+BLEADVSTART\r\n")) {
			gWirelessBleCommandPending = true;
			gWirelessBleAdvertisingEnabled = true;
		}
	}
}

static bool wirelessServiceWifiOff(const stEsp32c5Info *info)
{
	if (!gWirelessWifiOffRequested) {
		return false;
	}

	if ((info == NULL) || info->isBusy) {
		return true;
	}

	if (!gWirelessWifiOffMqttDone) {
		if (gWirelessIotMqttReady || gWirelessIotMqttConnPending || gWirelessIotMqttQueryPending || gWirelessIotMqttSubPending) {
			if (wirelessSubmitSimpleCommand("AT+MQTTCLEAN=0\r\n")) {
				gWirelessWifiOffMqttDone = true;
				wirelessResetMqttRuntime();
			}
			return true;
		}
		gWirelessWifiOffMqttDone = true;
		wirelessResetMqttRuntime();
	}

	if (!gWirelessWifiOffDisconnectDone) {
		if (gWirelessWifiConnected || gWirelessWifiGotIp || gWirelessWifiJoinPending) {
			if (wirelessSubmitSimpleCommand("AT+CWQAP\r\n")) {
				gWirelessWifiOffDisconnectDone = true;
				gWirelessWifiConnected = false;
				gWirelessWifiGotIp = false;
				gWirelessWifiJoinPending = false;
			}
			return true;
		}
		gWirelessWifiOffDisconnectDone = true;
	}

	if (!gWirelessWifiOffModeDone) {
		if (wirelessSubmitSimpleCommand("AT+CWMODE=0\r\n")) {
			gWirelessWifiOffModeDone = true;
			gWirelessWifiModeApplied = false;
			gWirelessWifiModePending = false;
		}
		return true;
	}

	gWirelessWifiOffRequested = false;
	gWirelessWifiConfigValid = false;
	gWirelessWifiState = WIFI_STATE_IDLE;
	return true;
}

static void wirelessServiceWifi(void)
{
	const stEsp32c5Info *info;
	char cmdText[160];
	eEsp32c5Status status;

	if (gWirelessBleState == BLE_STATE_ERROR) {
		gWirelessWifiState = WIFI_STATE_ERROR;
		return;
	}

	info = esp32c5GetInfo(WIRELESS_DEVICE);
	if (wirelessServiceWifiOff(info)) {
		return;
	}

	if (info == NULL) {
		return;
	}

	if (!gWirelessWifiAutoConnectGuardDone) {
		if (!gWirelessWifiAutoConnectGuardPending && !info->isBusy) {
			if (wirelessSubmitSimpleCommand("AT+CWAUTOCONN=0\r\n")) {
				gWirelessWifiAutoConnectGuardPending = true;
			}
		}
		if (gWirelessWifiAutoConnectGuardPending && !info->isBusy) {
			gWirelessWifiAutoConnectGuardPending = false;
			gWirelessWifiAutoConnectGuardDone = true;
		}
		if (!gWirelessWifiAutoConnectGuardDone) {
			return;
		}
	}

	if (!gWirelessWifiConfigValid && (gWirelessWifiConnected || gWirelessWifiGotIp) && !info->isBusy) {
		if (wirelessSubmitSimpleCommand("AT+CWQAP\r\n")) {
			gWirelessWifiConnected = false;
			gWirelessWifiGotIp = false;
			gWirelessWifiJoinPending = false;
			gWirelessWifiState = WIFI_STATE_READY;
		}
		return;
	}

	if (!gWirelessWifiEnabled) {
		gWirelessWifiState = WIFI_STATE_IDLE;
		return;
	}

	if (!gWirelessWifiConfigValid) {
		gWirelessWifiState = gWirelessStarted ? WIFI_STATE_READY : WIFI_STATE_IDLE;
		return;
	}

	if (gWirelessWifiModePending && !info->isBusy) {
		gWirelessWifiModePending = false;
		if (!info->hasLastResult || (info->lastResult != FLOWPARSER_RESULT_OK)) {
			gWirelessWifiState = WIFI_STATE_ERROR;
			return;
		}
		gWirelessWifiModeApplied = true;
		gWirelessWifiState = WIFI_STATE_WAITING_CONNECTION;
	}

	if (gWirelessWifiJoinPending && !info->isBusy) {
		gWirelessWifiJoinPending = false;
		if (!info->hasLastResult || (info->lastResult != FLOWPARSER_RESULT_OK)) {
			gWirelessWifiConnected = false;
			gWirelessWifiGotIp = false;
			gWirelessWifiState = WIFI_STATE_DISCONNECTED;
			return;
		}
		gWirelessWifiConnected = true;
		gWirelessWifiGotIp = true;
		gWirelessWifiState = WIFI_STATE_CONNECTED;
		return;
	}

	if (gWirelessWifiGotIp || info->isBusy || (gWirelessBleState == BLE_STATE_INITIALIZING)) {
		return;
	}

	if (!gWirelessWifiModeApplied) {
		status = esp32c5WifiBuildStationModeCommand(cmdText, (uint16_t)sizeof(cmdText));
		if (status == ESP32C5_STATUS_OK) {
			status = esp32c5SubmitTextCommand(WIRELESS_DEVICE, cmdText);
		}
		if (status == ESP32C5_STATUS_OK) {
			gWirelessWifiModePending = true;
			gWirelessWifiState = WIFI_STATE_INITIALIZING;
		} else if ((status != ESP32C5_STATUS_BUSY) && (status != ESP32C5_STATUS_NOT_READY)) {
			gWirelessWifiState = WIFI_STATE_ERROR;
		}
		return;
	}

	status = esp32c5WifiBuildJoinCommand(gWirelessWifiSsid,
					      gWirelessWifiPassword,
					      cmdText,
					      (uint16_t)sizeof(cmdText));
	if (status != ESP32C5_STATUS_OK) {
		gWirelessWifiState = WIFI_STATE_ERROR;
		return;
	}

	status = esp32c5SubmitTextCommand(WIRELESS_DEVICE, cmdText);
	if (status == ESP32C5_STATUS_OK) {
		gWirelessWifiJoinPending = true;
		gWirelessWifiState = WIFI_STATE_WAITING_CONNECTION;
	} else if ((status != ESP32C5_STATUS_BUSY) && (status != ESP32C5_STATUS_NOT_READY)) {
		gWirelessWifiState = WIFI_STATE_ERROR;
	}
}

static void wirelessIotHttpLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
	uint16_t copyLen;

	(void)userData;
	if ((lineBuf == NULL) || (lineLen == 0U) ||
		wirelessMatchPrefix(lineBuf, lineLen, "OK") ||
		wirelessMatchPrefix(lineBuf, lineLen, "ERROR")) {
		return;
	}

	if (gWirelessIotHttpResponseLen >= WIRELESS_IOT_HTTP_RESPONSE_LEN) {
		return;
	}

	copyLen = lineLen;
	if (copyLen > (uint16_t)(WIRELESS_IOT_HTTP_RESPONSE_LEN - gWirelessIotHttpResponseLen)) {
		copyLen = (uint16_t)(WIRELESS_IOT_HTTP_RESPONSE_LEN - gWirelessIotHttpResponseLen);
	}

	(void)memcpy(&gWirelessIotHttpResponse[gWirelessIotHttpResponseLen], lineBuf, copyLen);
	gWirelessIotHttpResponseLen = (uint16_t)(gWirelessIotHttpResponseLen + copyLen);
	gWirelessIotHttpResponse[gWirelessIotHttpResponseLen] = '\0';
}

static void wirelessIotMqttQueryLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
	uint8_t mqttState;

	(void)userData;
	if (wirelessTryParseMqttState(lineBuf, lineLen, &mqttState)) {
		gWirelessIotMqttState = mqttState;
		gWirelessIotMqttUserCfgReady = mqttState >= 1U;
		gWirelessIotMqttReady = (mqttState >= 4U);
		if (gWirelessIotMqttReady) {
			gWirelessIotMqttConnectedUrcSeen = true;
			gWirelessIotState = WIRELESS_IOT_STATE_MQTT_READY;
			LOG_I(WIRELESS_LOG_TAG, "mqtt query ready state=%u", (unsigned int)mqttState);
		} else {
			LOG_W(WIRELESS_LOG_TAG, "mqtt query state=%u", (unsigned int)mqttState);
		}
	}
}

static void wirelessIotMqttSubscribeLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
	(void)userData;
	if (wirelessMatchPrefix(lineBuf, lineLen, "+MQTTSUB:")) {
		gWirelessIotMqttSubReady = true;
	}
}

static bool wirelessTryParseHttpKey(void)
{
	static const char *const keyNames[] = {"result", "deviceSecret", "device_secret", "mqtt_key", "key", "token", "password", "secret"};
	char message[96];
	int32_t code;
	uint32_t index;
	uint16_t responseLen;

	responseLen = (uint16_t)strlen(gWirelessIotHttpResponse);
	if ((jsonParserFindInt(gWirelessIotHttpResponse, responseLen, "code", &code) == JSON_PARSER_STATUS_OK) &&
		(code != 200)) {
		LOG_W(WIRELESS_LOG_TAG, "iot http code=%ld", (long)code);
		if ((jsonParserFindString(gWirelessIotHttpResponse, responseLen, "message", message, (uint16_t)sizeof(message), NULL) == JSON_PARSER_STATUS_OK) ||
			(jsonParserFindString(gWirelessIotHttpResponse, responseLen, "msg", message, (uint16_t)sizeof(message), NULL) == JSON_PARSER_STATUS_OK) ||
			(jsonParserFindString(gWirelessIotHttpResponse, responseLen, "error", message, (uint16_t)sizeof(message), NULL) == JSON_PARSER_STATUS_OK)) {
			LOG_W(WIRELESS_LOG_TAG, "iot http msg=%s", message);
		}
		return false;
	}

	for (index = 0U; index < (uint32_t)(sizeof(keyNames) / sizeof(keyNames[0])); index++) {
		if (jsonParserFindString(gWirelessIotHttpResponse,
					 responseLen,
					 keyNames[index],
					 gWirelessIotKey,
					 (uint16_t)sizeof(gWirelessIotKey),
					 NULL) == JSON_PARSER_STATUS_OK) {
			gWirelessIotKeyReady = true;
			gWirelessIotMqttUserCfgReady = false;
			gWirelessIotMqttReady = false;
			gWirelessIotMqttConnectedUrcSeen = false;
			gWirelessIotMqttState = 0U;
			(void)wirelessWriteTextFile(WIRELESS_IOT_MQTT_KEY_PATH, gWirelessIotKey);
			LOG_I(WIRELESS_LOG_TAG, "iot key cached from http field=%s", keyNames[index]);
			return true;
		}
	}

	return false;
}

static bool wirelessSubmitHttpAuth(void)
{
	char cmdText[WIRELESS_IOT_CMD_MAX_LEN];
	uint16_t payloadLen;
	eEsp32c5Status status;

	if (gWirelessIotHttpUrl[0] == '\0') {
		return false;
	}
	if (!wirelessBuildHttpAuthPayload(gWirelessIotHttpPayload, (uint16_t)sizeof(gWirelessIotHttpPayload))) {
		return false;
	}
	payloadLen = (uint16_t)strlen(gWirelessIotHttpPayload);

	if (esp32c5HttpBuildPostJsonCommand(gWirelessIotHttpUrl,
						   payloadLen,
						   cmdText,
						   (uint16_t)sizeof(cmdText)) != ESP32C5_STATUS_OK) {
		return false;
	}

	gWirelessIotHttpResponseLen = 0U;
	gWirelessIotHttpResponse[0] = '\0';
	status = esp32c5SubmitPromptCommandEx(WIRELESS_DEVICE,
					      cmdText,
					      (const uint8_t *)gWirelessIotHttpPayload,
					      payloadLen,
					      wirelessIotHttpLineHandler,
					      NULL);
	if (status == ESP32C5_STATUS_OK) {
		gWirelessIotHttpPending = true;
		gWirelessIotState = WIRELESS_IOT_STATE_WAIT_AUTH;
		LOG_I(WIRELESS_LOG_TAG, "iot http auth start");
		return true;
	}

	return (status == ESP32C5_STATUS_BUSY) || (status == ESP32C5_STATUS_NOT_READY);
}

static bool wirelessSubmitMqttUserCfg(void)
{
	char cmdText[WIRELESS_IOT_CMD_MAX_LEN];
	char username[WIRELESS_IOT_USER_MAX_LEN + 1U];
	char password[WIRELESS_IOT_MD5_HEX_LEN];
	eEsp32c5Status status;

	if ((gWirelessIotMqttClientId[0] == '\0') || !wirelessBuildMqttLogin(username, (uint16_t)sizeof(username), password, (uint16_t)sizeof(password))) {
		return false;
	}

	if (esp32c5MqttBuildUserCfgCommand(gWirelessIotMqttClientId,
						 username,
						 password,
						 cmdText,
						 (uint16_t)sizeof(cmdText)) != ESP32C5_STATUS_OK) {
		return false;
	}

	status = esp32c5SubmitTextCommandEx(WIRELESS_DEVICE,
						   cmdText,
						   wirelessIotMqttSubscribeLineHandler,
						   NULL);
	if (status == ESP32C5_STATUS_OK) {
		gWirelessIotMqttUserCfgPending = true;
		gWirelessIotMqttUserCfgReady = false;
		gWirelessIotState = WIRELESS_IOT_STATE_MQTT_CONNECTING;
		return true;
	}

	return (status == ESP32C5_STATUS_BUSY) || (status == ESP32C5_STATUS_NOT_READY);
}

static bool wirelessSubmitMqttConnect(void)
{
	char cmdText[WIRELESS_IOT_CMD_MAX_LEN];
	eEsp32c5Status status;

	if (gWirelessIotMqttHost[0] == '\0') {
		return false;
	}

	if (esp32c5MqttBuildConnectCommand(gWirelessIotMqttHost,
						  gWirelessIotMqttPort,
						  cmdText,
						  (uint16_t)sizeof(cmdText)) != ESP32C5_STATUS_OK) {
		return false;
	}

	status = esp32c5SubmitTextCommand(WIRELESS_DEVICE, cmdText);
	if (status == ESP32C5_STATUS_OK) {
		gWirelessIotMqttConnPending = true;
		gWirelessIotMqttConnectedUrcSeen = false;
		gWirelessIotMqttReady = false;
		gWirelessIotState = WIRELESS_IOT_STATE_MQTT_CONNECTING;
		return true;
	}

	return (status == ESP32C5_STATUS_BUSY) || (status == ESP32C5_STATUS_NOT_READY);
}

static bool wirelessSubmitMqttQuery(void)
{
	char cmdText[32];
	eEsp32c5Status status;

	if (esp32c5MqttBuildQueryCommand(cmdText, (uint16_t)sizeof(cmdText)) != ESP32C5_STATUS_OK) {
		return false;
	}

	status = esp32c5SubmitTextCommandEx(WIRELESS_DEVICE,
					    cmdText,
					    wirelessIotMqttQueryLineHandler,
					    NULL);
	if (status == ESP32C5_STATUS_OK) {
		gWirelessIotMqttQueryPending = true;
		return true;
	}

	return (status == ESP32C5_STATUS_BUSY) || (status == ESP32C5_STATUS_NOT_READY);
}

static bool wirelessSubmitMqttSubscribe(void)
{
	char cmdText[WIRELESS_IOT_CMD_MAX_LEN];
	eEsp32c5Status status;

	if (gWirelessIotMqttSubTopic[0] == '\0') {
		return false;
	}

	if (esp32c5MqttBuildSubscribeCommand(gWirelessIotMqttSubTopic,
						    1U,
						    cmdText,
						    (uint16_t)sizeof(cmdText)) != ESP32C5_STATUS_OK) {
		return false;
	}

	status = esp32c5SubmitTextCommand(WIRELESS_DEVICE, cmdText);
	if (status == ESP32C5_STATUS_OK) {
		gWirelessIotMqttSubPending = true;
		gWirelessIotMqttSubReady = false;
		return true;
	}

	return (status == ESP32C5_STATUS_BUSY) || (status == ESP32C5_STATUS_NOT_READY);
}

static void wirelessEnterIotError(uint32_t nowTick, const char *logText)
{
	gWirelessIotState = WIRELESS_IOT_STATE_ERROR;
	gWirelessIotManualRetryNeeded = true;
	gWirelessIotNextRetryTick = nowTick;
	if (logText != NULL) {
		LOG_W(WIRELESS_LOG_TAG, "%s", logText);
	}
}

static void wirelessScheduleIotRetry(uint32_t nowTick, const char *logText)
{
	gWirelessIotState = gWirelessIotKeyReady ? WIRELESS_IOT_STATE_AUTH_READY : WIRELESS_IOT_STATE_WAIT_AUTH;
	gWirelessIotManualRetryNeeded = false;
	gWirelessIotNextRetryTick = nowTick + WIRELESS_IOT_RETRY_MS;
	if (logText != NULL) {
		LOG_W(WIRELESS_LOG_TAG, "%s", logText);
	}
}

static void wirelessServiceIot(void)
{
	const stEsp32c5Info *info;
	uint32_t nowTick;

	if (!gWirelessStorageLoaded) {
		(void)wirelessLoadStorageConfig();
	}

	if (!gWirelessMqttEnabled) {
		wirelessResetMqttRuntime();
		return;
	}

	if (gWirelessWifiState != WIFI_STATE_CONNECTED) {
		gWirelessIotState = WIRELESS_IOT_STATE_WAIT_WIFI;
		gWirelessIotMqttReady = false;
		return;
	}

	info = esp32c5GetInfo(WIRELESS_DEVICE);
	if (info == NULL) {
		return;
	}
	nowTick = repRtosGetTickMs();

	if (gWirelessIotHttpPending && !info->isBusy) {
		gWirelessIotHttpPending = false;
		if (info->hasLastResult && (info->lastResult == FLOWPARSER_RESULT_OK) && wirelessTryParseHttpKey()) {
			gWirelessIotState = WIRELESS_IOT_STATE_AUTH_READY;
		} else {
			wirelessEnterIotError(nowTick, "iot http auth failed");
		}
		return;
	}

	if (gWirelessIotMqttUserCfgPending && !info->isBusy) {
		gWirelessIotMqttUserCfgPending = false;
		if ((!info->hasLastResult || (info->lastResult != FLOWPARSER_RESULT_OK)) &&
			!gWirelessIotMqttConnectedUrcSeen && !gWirelessIotMqttReady) {
			gWirelessIotMqttUserCfgReady = false;
			wirelessScheduleIotRetry(nowTick, "mqtt user cfg failed");
			return;
		}
		gWirelessIotMqttUserCfgReady = true;
		if (gWirelessIotMqttConnectedUrcSeen || gWirelessIotMqttReady) {
			gWirelessIotMqttReady = true;
			gWirelessIotMqttState = 4U;
			gWirelessIotState = WIRELESS_IOT_STATE_MQTT_READY;
		} else {
			gWirelessIotMqttState = 1U;
			(void)wirelessSubmitMqttConnect();
		}
		return;
	}

	if (gWirelessIotMqttConnPending && !info->isBusy) {
		gWirelessIotMqttConnPending = false;
		if ((info->hasLastResult && (info->lastResult == FLOWPARSER_RESULT_OK)) || gWirelessIotMqttConnectedUrcSeen) {
			gWirelessIotState = WIRELESS_IOT_STATE_MQTT_CONNECTING;
			if (!gWirelessIotMqttConnectedUrcSeen) {
				(void)wirelessSubmitMqttQuery();
			} else {
				gWirelessIotMqttReady = true;
				gWirelessIotState = WIRELESS_IOT_STATE_MQTT_READY;
			}
		} else {
			gWirelessIotMqttReady = false;
			wirelessScheduleIotRetry(nowTick, "mqtt connect failed");
		}
		return;
	}

	if (gWirelessIotMqttQueryPending && !info->isBusy) {
		gWirelessIotMqttQueryPending = false;
		if (!info->hasLastResult || (info->lastResult != FLOWPARSER_RESULT_OK)) {
			gWirelessIotMqttReady = false;
			wirelessScheduleIotRetry(nowTick, "mqtt query failed");
			return;
		}
		if (!gWirelessIotMqttReady) {
			gWirelessIotState = (gWirelessIotMqttState == 3U) ? WIRELESS_IOT_STATE_AUTH_READY : WIRELESS_IOT_STATE_MQTT_CONNECTING;
			LOG_W(WIRELESS_LOG_TAG, "mqtt query not ready state=%u", (unsigned int)gWirelessIotMqttState);
		}
		return;
	}

	if (gWirelessIotMqttSubPending && !info->isBusy) {
		gWirelessIotMqttSubPending = false;
		if ((!info->hasLastResult || (info->lastResult != FLOWPARSER_RESULT_OK)) && !gWirelessIotMqttSubReady) {
			gWirelessIotMqttSubReady = false;
			wirelessScheduleIotRetry(nowTick, "mqtt subscribe failed");
			return;
		}
		gWirelessIotMqttSubReady = true;
		LOG_I(WIRELESS_LOG_TAG, "mqtt subscribed topic=%s", gWirelessIotMqttSubTopic);
		return;
	}

	if (info->isBusy) {
		return;
	}

	if (gWirelessIotManualRetryNeeded) {
		return;
	}

	if (gWirelessIotMqttReady && !gWirelessIotMqttSubReady) {
		if ((int32_t)(nowTick - gWirelessIotNextRetryTick) < 0) {
			return;
		}
		if (!wirelessSubmitMqttSubscribe()) {
			gWirelessIotNextRetryTick = nowTick + WIRELESS_IOT_RETRY_MS;
		}
		return;
	}

	if (gWirelessIotMqttReady) {
		return;
	}

	if (gWirelessIotState == WIRELESS_IOT_STATE_MQTT_CONNECTING) {
		return;
	}

	if (!gWirelessIotKeyReady) {
		if ((int32_t)(nowTick - gWirelessIotNextRetryTick) < 0) {
			return;
		}
		if (!wirelessSubmitHttpAuth()) {
			gWirelessIotState = WIRELESS_IOT_STATE_WAIT_AUTH;
			gWirelessIotNextRetryTick = nowTick + WIRELESS_IOT_RETRY_MS;
		}
		return;
	}

	if (gWirelessIotMqttHost[0] == '\0') {
		gWirelessIotState = WIRELESS_IOT_STATE_AUTH_READY;
		return;
	}

	if ((int32_t)(nowTick - gWirelessIotNextRetryTick) < 0) {
		return;
	}

	if (!gWirelessIotMqttUserCfgReady) {
		if (!wirelessSubmitMqttUserCfg()) {
			gWirelessIotNextRetryTick = nowTick + WIRELESS_IOT_RETRY_MS;
		}
		return;
	}

	if ((gWirelessIotMqttState == 0U) && !wirelessSubmitMqttQuery()) {
		gWirelessIotNextRetryTick = nowTick + WIRELESS_IOT_RETRY_MS;
		return;
	}

	if ((gWirelessIotMqttState < 4U) && !wirelessSubmitMqttConnect()) {
		gWirelessIotNextRetryTick = nowTick + WIRELESS_IOT_RETRY_MS;
	}
}

static void wirelessFlushTx(void)
{
	uint8_t txBuffer[WIRELESS_TX_BUFFER_SIZE];
	uint16_t txLen;
	eEsp32c5Status status;

	if (gWirelessBleState != BLE_STATE_CONNECTED) {
		return;
	}

	repRtosEnterCritical();
	if (!gWirelessTxPending) {
		repRtosExitCritical();
		return;
	}

	txLen = gWirelessTxLen;
	(void)memcpy(txBuffer, gWirelessTxBuffer, txLen);
	repRtosExitCritical();

	if (!wirelessCanSendPendingFrame(txBuffer, txLen)) {
		return;
	}

	status = esp32c5WriteData(WIRELESS_DEVICE, txBuffer, txLen);
	if (status != ESP32C5_STATUS_OK) {
		if ((status != ESP32C5_STATUS_BUSY) && (status != ESP32C5_STATUS_NOT_READY)) {
			LOG_W(WIRELESS_LOG_TAG,
			      "ble tx flush failed status=%d len=%u",
			      (int)status,
			      (unsigned int)txLen);
		}
		return;
	}

	repRtosEnterCritical();
	if (gWirelessTxPending && (gWirelessTxLen == txLen)) {
		gWirelessTxPending = false;
		gWirelessTxLen = 0U;
	}
	repRtosExitCritical();
}

static void wirelessService(void)
{
	const stEsp32c5State *state;
	eEsp32c5Status status;
	uint32_t nowTickMs;

	if (!wirelessConfigureIfNeeded()) {
		return;
	}

	(void)wirelessLoadStorageConfig();

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
		gWirelessBleAdvertisingEnabled = false;
		gWirelessWifiModeApplied = false;
		gWirelessWifiModePending = false;
		gWirelessWifiJoinPending = false;
		gWirelessWifiConnected = false;
		gWirelessWifiGotIp = false;
		gWirelessWifiAutoConnectGuardPending = false;
		gWirelessWifiAutoConnectGuardDone = false;
		gWirelessWifiState = gWirelessWifiEnabled ? WIFI_STATE_READY : WIFI_STATE_IDLE;
		gWirelessIotManualRetryNeeded = false;
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
	wirelessServiceBleSwitch();
	wirelessServiceWifi();
	wirelessServiceIot();
	wirelessFlushTx();
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
		gWirelessWifiState = WIFI_STATE_IDLE;
		wirelessUpdateIotState();
		return;
	}

	if (state->runState == ESP32C5_RUN_ERROR) {
		gWirelessBleState = BLE_STATE_ERROR;
		gWirelessWifiState = WIFI_STATE_ERROR;
		wirelessUpdateIotState();
		return;
	}

	if (!state->isReady) {
		gWirelessBleState = BLE_STATE_INITIALIZING;
		gWirelessWifiState = gWirelessWifiEnabled ? WIFI_STATE_INITIALIZING : WIFI_STATE_IDLE;
		wirelessUpdateIotState();
		return;
	}

	if (!gWirelessBleEnabled) {
		gWirelessBleState = BLE_STATE_READY;
	} else if (state->isBleConnected) {
		gWirelessBleState = gWirelessProtocolHandshakeDone ? BLE_STATE_CONNECTED : BLE_WAITING_CONNECTION;
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
		if (!gWirelessBleConnected) {
			wirelessResetProtocolState();
			gWirelessBleAdvertisingEnabled = false;
		} else {
			gWirelessBleAdvertisingEnabled = false;
		}
		LOG_I(WIRELESS_LOG_TAG, "ble %s", gWirelessBleConnected ? "connected" : "disconnected");
	}

	if (!gWirelessWifiEnabled) {
		gWirelessWifiState = WIFI_STATE_IDLE;
	} else if (!gWirelessWifiConfigValid) {
		gWirelessWifiState = WIFI_STATE_READY;
	} else if (gWirelessWifiGotIp) {
		gWirelessWifiState = WIFI_STATE_CONNECTED;
	} else if (gWirelessWifiJoinPending) {
		gWirelessWifiState = WIFI_STATE_WAITING_CONNECTION;
	} else if (gWirelessWifiModePending) {
		gWirelessWifiState = WIFI_STATE_INITIALIZING;
	} else if (gWirelessWifiConnected) {
		gWirelessWifiState = WIFI_STATE_WAITING_CONNECTION;
	} else if (gWirelessWifiModeApplied) {
		gWirelessWifiState = WIFI_STATE_DISCONNECTED;
	}

	wirelessUpdateIotState();
}

enum bleState wirelessGetBleState(void)
{
	wirelessUpdateState();
	return gWirelessBleState;
}

enum wifiState wirelessGetWifiState(void)
{
	wirelessUpdateState();
	return gWirelessWifiState;
}

eWirelessIotState wirelessGetIotState(void)
{
	wirelessUpdateState();
	return gWirelessIotState;
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

bool wirelessGetBleEnabled(void)
{
	return gWirelessBleEnabled;
}

bool wirelessSetBleEnabled(bool enabled)
{
	gWirelessBleEnabled = enabled;
	if (!enabled) {
		wirelessResetProtocolState();
		gWirelessTxPending = false;
		gWirelessTxLen = 0U;
	}
	return true;
}

bool wirelessGetWifiEnabled(void)
{
	return gWirelessWifiEnabled;
}

bool wirelessSetWifiEnabled(bool enabled)
{
	gWirelessWifiEnabled = enabled;
	if (enabled) {
		gWirelessWifiOffRequested = false;
		gWirelessWifiOffMqttDone = false;
		gWirelessWifiOffDisconnectDone = false;
		gWirelessWifiOffModeDone = false;
		gWirelessWifiConfigValid = false;
		gWirelessWifiState = gWirelessStarted ? WIFI_STATE_READY : WIFI_STATE_IDLE;
		return true;
	}

	gWirelessMqttEnabled = false;
	gWirelessIotState = WIRELESS_IOT_STATE_IDLE;
	gWirelessWifiOffRequested = true;
	gWirelessWifiOffMqttDone = false;
	gWirelessWifiOffDisconnectDone = false;
	gWirelessWifiOffModeDone = false;
	gWirelessWifiConfigValid = false;
	gWirelessWifiState = WIFI_STATE_DISCONNECTED;
	return true;
}

bool wirelessConnectWifi(const uint8_t *ssid, uint8_t ssidLen, const uint8_t *password, uint8_t passwordLen)
{
	gWirelessWifiEnabled = true;
	gWirelessWifiOffRequested = false;
	return wirelessSetWifiCredentials(ssid, ssidLen, password, passwordLen);
}

bool wirelessGetMqttEnabled(void)
{
	return gWirelessMqttEnabled;
}

bool wirelessSetMqttEnabled(bool enabled)
{
	const stEsp32c5Info *info;

	if (enabled) {
		if (gWirelessWifiState != WIFI_STATE_CONNECTED) {
			return false;
		}
		gWirelessMqttEnabled = true;
		gWirelessIotManualRetryNeeded = false;
		gWirelessIotNextRetryTick = 0U;
		gWirelessIotState = gWirelessIotKeyReady ? WIRELESS_IOT_STATE_AUTH_READY : WIRELESS_IOT_STATE_WAIT_AUTH;
		return true;
	}

	info = esp32c5GetInfo(WIRELESS_DEVICE);
	if ((info != NULL) && info->isBusy &&
		(gWirelessIotMqttReady || gWirelessIotMqttConnPending || gWirelessIotMqttQueryPending || gWirelessIotMqttSubPending)) {
		return false;
	}
	if (gWirelessIotMqttReady || gWirelessIotMqttConnPending || gWirelessIotMqttQueryPending || gWirelessIotMqttSubPending) {
		(void)wirelessSubmitSimpleCommand("AT+MQTTCLEAN=0\r\n");
	}
	gWirelessMqttEnabled = false;
	gWirelessIotState = WIRELESS_IOT_STATE_IDLE;
	wirelessResetMqttRuntime();
	return true;
}

bool wirelessSendData(const uint8_t *buffer, uint16_t length)
{
	return wirelessSendBleData(buffer, length);
}

bool wirelessSendBleData(const uint8_t *buffer, uint16_t length)
{
	if ((buffer == NULL) || (length == 0U) || (length > WIRELESS_TX_BUFFER_SIZE)) {
		return false;
	}

	repRtosEnterCritical();
	(void)memcpy(gWirelessTxBuffer, buffer, length);
	gWirelessTxLen = length;
	gWirelessTxPending = true;
	repRtosExitCritical();
	return true;
}

bool wirelessSendWifiData(const uint8_t *buffer, uint16_t length)
{
	const stEsp32c5State *state;
	char cmdText[WIRELESS_IOT_CMD_MAX_LEN];

	if ((buffer == NULL) || (length == 0U) || (length > WIRELESS_TX_BUFFER_SIZE) ||
		!gWirelessIotMqttReady || (gWirelessIotMqttTopic[0] == '\0')) {
		return false;
	}

	if (gWirelessTxPending) {
		return false;
	}

	state = esp32c5GetState(WIRELESS_DEVICE);
	if ((state == NULL) || state->isBusy) {
		return false;
	}

	if (gWirelessMqttTxInFlight) {
		gWirelessMqttTxInFlight = false;
		gWirelessMqttTxLen = 0U;
	}

	if (esp32c5MqttBuildPublishRawCommand(gWirelessIotMqttTopic,
						     length,
						     0U,
						     0U,
						     cmdText,
						     (uint16_t)sizeof(cmdText)) != ESP32C5_STATUS_OK) {
		LOG_W(WIRELESS_LOG_TAG, "mqtt raw publish command build failed");
		return false;
	}

	(void)memcpy(gWirelessMqttTxBuffer, buffer, length);
	gWirelessMqttTxLen = length;
	if (esp32c5SubmitPromptCommandEx(WIRELESS_DEVICE,
					  cmdText,
					  gWirelessMqttTxBuffer,
					  gWirelessMqttTxLen,
					  NULL,
					  NULL) != ESP32C5_STATUS_OK) {
		gWirelessMqttTxLen = 0U;
		return false;
	}

	gWirelessMqttTxInFlight = true;
	return true;
}

bool wirelessSetWifiCredentials(const uint8_t *ssid, uint8_t ssidLen, const uint8_t *password, uint8_t passwordLen)
{
	if ((ssid == NULL) || (ssidLen > WIRELESS_WIFI_SSID_MAX_LEN) ||
		(passwordLen > WIRELESS_WIFI_PASSWORD_MAX_LEN) || ((password == NULL) && (passwordLen > 0U))) {
		return false;
	}

	if (!wirelessCopyBytesAsText(gWirelessWifiSsid,
				    (uint16_t)sizeof(gWirelessWifiSsid),
				    ssid,
				    ssidLen)) {
		return false;
	}

	if (!wirelessCopyBytesAsText(gWirelessWifiPassword,
				    (uint16_t)sizeof(gWirelessWifiPassword),
				    (passwordLen > 0U) ? password : (const uint8_t *)"",
				    passwordLen)) {
		return false;
	}

	gWirelessWifiConfigValid = ssidLen > 0U;
	gWirelessWifiEnabled = gWirelessWifiConfigValid ? true : gWirelessWifiEnabled;
	gWirelessWifiOffRequested = false;
	gWirelessWifiOffMqttDone = false;
	gWirelessWifiOffDisconnectDone = false;
	gWirelessWifiOffModeDone = false;
	gWirelessWifiModeApplied = false;
	gWirelessWifiModePending = false;
	gWirelessWifiJoinPending = false;
	gWirelessWifiConnected = false;
	gWirelessWifiGotIp = false;
	gWirelessWifiState = gWirelessWifiConfigValid ? WIFI_STATE_INITIALIZING : WIFI_STATE_IDLE;
	gWirelessIotManualRetryNeeded = false;
	return true;
}

bool wirelessRequestIotRetry(void)
{
	const stEsp32c5Info *info;

	info = esp32c5GetInfo(WIRELESS_DEVICE);
	if ((info != NULL) && info->isBusy) {
		return false;
	}

	gWirelessIotHttpPending = false;
	gWirelessIotMqttUserCfgPending = false;
	gWirelessIotMqttConnPending = false;
	gWirelessIotMqttQueryPending = false;
	gWirelessIotMqttSubPending = false;
	gWirelessIotMqttConnectedUrcSeen = false;
	gWirelessIotMqttReady = false;
	gWirelessIotMqttSubReady = false;
	gWirelessIotManualRetryNeeded = false;
	gWirelessIotNextRetryTick = 0U;
	gWirelessIotState = gWirelessIotKeyReady ? WIRELESS_IOT_STATE_AUTH_READY : WIRELESS_IOT_STATE_WAIT_AUTH;
	return true;
}

bool wirelessGetMacAddress(char *buffer, uint16_t bufferSize)
{
	return esp32c5GetCachedMac(WIRELESS_DEVICE, buffer, bufferSize);
}

void wirelessProcess(void)
{
	wirelessService();
}

/**************************End of file********************************/

