/***********************************************************************************
* @file     : iotmanager.c
* @brief    : CPR sensor IoT manager.
* @details  : Receives packets from transport managers, decodes the CPR sensor
*             protocol, and routes replies through the selected transport.
* @author   : GitHub Copilot
* @date     : 2026-04-24
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "iotmanager.h"

#include <string.h>

#include "cprsensor_protocol.h"
#include "../cellular/cellular.h"
#include "../ethernet/ethernet.h"
#include "../wireless/wireless.h"
#include "../../../rep/service/log/log.h"
#include "../../../rep/service/rtos/rtos.h"
#include "../../../rep/tools/aes/aes.h"
#include "../../../rep/tools/md5/md5.h"

#define IOT_MANAGER_LOG_TAG                    "iotmgr"
#define IOT_MANAGER_RX_BUFFER_SIZE             640U
#define IOT_MANAGER_TX_FRAME_BUFFER_SIZE       128U
#define IOT_MANAGER_DEVICE_TYPE                0x01U
#define IOT_MANAGER_PROTOCOL_FLAG              0x01U
#define IOT_MANAGER_SW_VERSION                 0x01U
#define IOT_MANAGER_SW_SUB_VERSION             0x00U
#define IOT_MANAGER_SW_BUILD_VERSION           0x00U
#define IOT_MANAGER_DEFAULT_BATTERY_PERCENT    100U
#define IOT_MANAGER_DEFAULT_BATTERY_MV         3700U
#define IOT_MANAGER_DEFAULT_CHARGE_STATE       0U

typedef struct stIotManagerContext {
	stIotManagerState publicState;
	bool initialized;
	bool rxPending;
	uint16_t rxLength;
	eIotManagerLinkId rxLink;
	uint8_t rxBuffer[IOT_MANAGER_RX_BUFFER_SIZE];
	eIotManagerLinkId txLink;
	eCprsensorProtocolTxFlag txFlags;
	bool cipherReady;
	uint8_t aesKey[MD5_DIGEST_SIZE];
	stCprsensorProtocolHandshakePayload handshakePayload;
	stCprsensorProtocolLanguagePayload languagePayload;
	stCprsensorProtocolVolumePayload volumePayload;
	stCprsensorProtocolMetronomePayload metronomePayload;
	stCprsensorProtocolUtcSettingPayload utcSettingPayload;
	stCprsensorProtocolCommSettingPayload commSettingPayload;
	stCprsensorProtocolTimeSyncPayload timeSyncPayload;
} stIotManagerContext;

static stIotManagerContext gIotManagerState;

static const stIotManagerLinkCaps gIotManagerDefaultLinkCaps[IOT_MANAGER_LINK_MAX] = {
	{ false, false, false, false },
	{ true, false, false, false },
	{ false, true, true, true },
	{ false, true, true, false },
	{ false, true, true, true },
};

static void iotManagerFillRawCodec(stCprsensorProtocolCodecCfg *codecCfg);
static void iotManagerFillCipherCodec(stCprsensorProtocolCodecCfg *codecCfg);
static eIotManagerLinkId iotManagerInterfaceToLink(eIotManagerInterface interfaceType);
static bool iotManagerIsValidLink(eIotManagerLinkId linkId);
static bool iotManagerIsValidInterface(eIotManagerInterface interfaceType);
static bool iotManagerLinkCapsIsEmpty(const stIotManagerLinkCaps *caps);
static stIotManagerServiceRoute *iotManagerGetRouteByServiceId(eIotManagerServiceId serviceId);
static bool iotManagerLinkSupportsService(eIotManagerLinkId linkId, eIotManagerServiceId serviceId);
static bool iotManagerLinkCanRunService(const stIotManagerLinkRuntime *runtime, eIotManagerServiceId serviceId);
static void iotManagerInitRoute(stIotManagerServiceRoute *route, eIotManagerServiceId serviceId, eIotManagerLinkId preferredLink, eIotManagerSendPolicy policy);
static void iotManagerInitLinkRuntime(stIotManagerLinkRuntime *runtime, eIotManagerLinkId linkId);
static void iotManagerEnsureStateInitialized(void);
static eIotManagerLinkId iotManagerPickAutoLink(eIotManagerServiceId serviceId);
static void iotManagerRefreshRouteLocked(stIotManagerServiceRoute *route);
static void iotManagerRefreshStateLocked(void);
static bool iotManagerApplyCompatibilitySelectionLocked(eIotManagerLinkId linkId);
static bool iotManagerAesTransform(void *userData, uint8_t *buffer, uint16_t length, bool encryptMode);
static bool iotManagerAesEncrypt(void *userData, uint8_t *buffer, uint16_t length);
static bool iotManagerAesDecrypt(void *userData, uint8_t *buffer, uint16_t length);
static bool iotManagerParseMacString(const char *text, uint8_t *mac, uint16_t macSize);
static bool iotManagerTryInitCipherKey(void);
static bool iotManagerGetExpectedPayloadLen(eCprsensorProtocolCmd cmd, uint16_t *payloadLen);
static bool iotManagerShouldTryCipher(eCprsensorProtocolCmd cmd, uint16_t payloadLen);
static eCprsensorProtocolStatus iotManagerParseIncomingFrame(const uint8_t *buffer,
						      uint16_t length,
						      stCprsensorProtocolFrameView *frameView,
						      uint8_t *payloadBuffer,
						      uint16_t payloadBufferSize);
						static void iotManagerSetTxLink(eIotManagerLinkId linkId);
static void iotManagerSetReplyFlag(eCprsensorProtocolCmd cmd);
						static void iotManagerHandleFrame(eIotManagerLinkId linkId, const stCprsensorProtocolFrameView *frameView);
						static bool iotManagerBuildAndSendReply(eIotManagerLinkId linkId,
						 eCprsensorProtocolCmd cmd,
						 const uint8_t *payload,
						 uint16_t payloadLen);
						static void iotManagerTryFlushReply(eIotManagerLinkId linkId,
				 bool flagPending,
				 eCprsensorProtocolCmd cmd,
				 const uint8_t *payload,
				 uint16_t payloadLen,
				 void (*clearFlag)(void));
static void iotManagerClearFlagHandshake(void);
static void iotManagerClearFlagHeartBeat(void);
static void iotManagerClearFlagDisconnect(void);
static void iotManagerClearFlagDevInfo(void);
static void iotManagerClearFlagBleInfo(void);
static void iotManagerClearFlagBattery(void);
static void iotManagerClearFlagLanguage(void);
static void iotManagerClearFlagVolume(void);
static void iotManagerClearFlagMetronome(void);
static void iotManagerClearFlagUtcSetting(void);
static void iotManagerFlushHandshakeReply(eIotManagerLinkId linkId, bool flagPending);
static void iotManagerFlushHeartbeatReply(eIotManagerLinkId linkId, bool flagPending);
static void iotManagerFlushDisconnectReply(eIotManagerLinkId linkId, bool flagPending);
static void iotManagerFlushDevInfoReply(eIotManagerLinkId linkId, bool flagPending);
static void iotManagerFlushBleInfoReply(eIotManagerLinkId linkId, bool flagPending);
static void iotManagerFlushBatteryReply(eIotManagerLinkId linkId, bool flagPending);
static void iotManagerFlushLanguageReply(eIotManagerLinkId linkId, bool flagPending);
static void iotManagerFlushVolumeReply(eIotManagerLinkId linkId, bool flagPending);
static void iotManagerFlushMetronomeReply(eIotManagerLinkId linkId, bool flagPending);
static void iotManagerFlushUtcSettingReply(eIotManagerLinkId linkId, bool flagPending);
static void iotManagerFlushPendingReplies(void);
static void iotManagerConsumeReceivedData(void);

static eIotManagerLinkId iotManagerInterfaceToLink(eIotManagerInterface interfaceType)
{
	switch (interfaceType) {
	case IOT_MANAGER_INTERFACE_WIRELESS:
		return IOT_MANAGER_LINK_BLE;
	case IOT_MANAGER_INTERFACE_CELLULAR:
		return IOT_MANAGER_LINK_CELLULAR;
	case IOT_MANAGER_INTERFACE_ETHERNET:
		return IOT_MANAGER_LINK_ETHERNET;
	default:
		break;
	}

	return IOT_MANAGER_LINK_NONE;
}

static bool iotManagerIsValidLink(eIotManagerLinkId linkId)
{
	return (linkId > IOT_MANAGER_LINK_NONE) && (linkId < IOT_MANAGER_LINK_MAX);
}

static bool iotManagerIsValidInterface(eIotManagerInterface interfaceType)
{
	return iotManagerIsValidLink(iotManagerInterfaceToLink(interfaceType));
}

static bool iotManagerLinkCapsIsEmpty(const stIotManagerLinkCaps *caps)
{
	if (caps == NULL) {
		return true;
	}

	return !caps->supportBleLocal && !caps->supportMqttAuthHttp && !caps->supportMqtt && !caps->supportTcpServer;
}

static stIotManagerServiceRoute *iotManagerGetRouteByServiceId(eIotManagerServiceId serviceId)
{
	switch (serviceId) {
	case IOT_MANAGER_SERVICE_BLE_LOCAL:
		return &gIotManagerState.publicState.bleLocalRoute;
	case IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP:
		return &gIotManagerState.publicState.mqttAuthRoute;
	case IOT_MANAGER_SERVICE_MQTT:
		return &gIotManagerState.publicState.mqttRoute;
	case IOT_MANAGER_SERVICE_TCP_SERVER:
		return &gIotManagerState.publicState.tcpServerRoute;
	default:
		break;
	}

	return NULL;
}

static bool iotManagerLinkSupportsService(eIotManagerLinkId linkId, eIotManagerServiceId serviceId)
{
	const stIotManagerLinkCaps *lCaps;

	if (!iotManagerIsValidLink(linkId)) {
		return false;
	}

	lCaps = &gIotManagerState.publicState.links[linkId].caps;
	switch (serviceId) {
	case IOT_MANAGER_SERVICE_BLE_LOCAL:
		return lCaps->supportBleLocal;
	case IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP:
		return lCaps->supportMqttAuthHttp;
	case IOT_MANAGER_SERVICE_MQTT:
		return lCaps->supportMqtt;
	case IOT_MANAGER_SERVICE_TCP_SERVER:
		return lCaps->supportTcpServer;
	default:
		break;
	}

	return false;
}

static bool iotManagerLinkCanRunService(const stIotManagerLinkRuntime *runtime, eIotManagerServiceId serviceId)
{
	if ((runtime == NULL) || !runtime->installed || !runtime->enabled) {
		return false;
	}

	switch (serviceId) {
	case IOT_MANAGER_SERVICE_BLE_LOCAL:
		return runtime->caps.supportBleLocal && (runtime->peerConnected || runtime->moduleReady);
	case IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP:
		return runtime->caps.supportMqttAuthHttp && (runtime->mqttAuthReady || runtime->netReady);
	case IOT_MANAGER_SERVICE_MQTT:
		return runtime->caps.supportMqtt && (runtime->mqttReady || runtime->netReady);
	case IOT_MANAGER_SERVICE_TCP_SERVER:
		return runtime->caps.supportTcpServer && (runtime->tcpServerListening || runtime->netReady);
	default:
		break;
	}

	return false;
}

static void iotManagerInitRoute(stIotManagerServiceRoute *route, eIotManagerServiceId serviceId, eIotManagerLinkId preferredLink, eIotManagerSendPolicy policy)
{
	if (route == NULL) {
		return;
	}

	(void)memset(route, 0, sizeof(*route));
	route->serviceId = serviceId;
	route->preferredLink = preferredLink;
	route->policy = policy;
	route->state = IOT_MANAGER_SERVICE_STATE_WAIT_LINK;
}

static void iotManagerInitLinkRuntime(stIotManagerLinkRuntime *runtime, eIotManagerLinkId linkId)
{
	if (runtime == NULL) {
		return;
	}

	(void)memset(runtime, 0, sizeof(*runtime));
	runtime->linkId = linkId;
	runtime->state = IOT_MANAGER_LINK_STATE_DISABLED;
	runtime->caps = gIotManagerDefaultLinkCaps[linkId];
	runtime->installed = (linkId != IOT_MANAGER_LINK_WIFI);
	runtime->enabled = runtime->installed;
	if (linkId == IOT_MANAGER_LINK_CELLULAR) {
		runtime->cellularType = IOT_MANAGER_CELLULAR_NONE;
	}
}

static void iotManagerEnsureStateInitialized(void)
{
	uint32_t lIndex;

	if (gIotManagerState.initialized) {
		return;
	}

	(void)memset(&gIotManagerState.publicState, 0, sizeof(gIotManagerState.publicState));
	for (lIndex = 0U; lIndex < (uint32_t)IOT_MANAGER_LINK_MAX; ++lIndex) {
		iotManagerInitLinkRuntime(&gIotManagerState.publicState.links[lIndex], (eIotManagerLinkId)lIndex);
	}
	gIotManagerState.publicState.links[IOT_MANAGER_LINK_NONE].state = IOT_MANAGER_LINK_STATE_ABSENT;
	iotManagerInitRoute(&gIotManagerState.publicState.bleLocalRoute,
			     IOT_MANAGER_SERVICE_BLE_LOCAL,
			     IOT_MANAGER_LINK_BLE,
			     IOT_MANAGER_SEND_POLICY_FIXED);
	iotManagerInitRoute(&gIotManagerState.publicState.mqttAuthRoute,
			     IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP,
			     IOT_MANAGER_LINK_ETHERNET,
			     IOT_MANAGER_SEND_POLICY_AUTO);
	iotManagerInitRoute(&gIotManagerState.publicState.mqttRoute,
			     IOT_MANAGER_SERVICE_MQTT,
			     IOT_MANAGER_LINK_ETHERNET,
			     IOT_MANAGER_SEND_POLICY_AUTO);
	iotManagerInitRoute(&gIotManagerState.publicState.tcpServerRoute,
			     IOT_MANAGER_SERVICE_TCP_SERVER,
			     IOT_MANAGER_LINK_ETHERNET,
			     IOT_MANAGER_SEND_POLICY_AUTO);
	gIotManagerState.txLink = IOT_MANAGER_LINK_BLE;
	gIotManagerState.rxLink = IOT_MANAGER_LINK_NONE;
	gIotManagerState.initialized = true;
	iotManagerRefreshStateLocked();
}

static eIotManagerLinkId iotManagerPickAutoLink(eIotManagerServiceId serviceId)
{
	static const eIotManagerLinkId lCloudPriority[] = {
		IOT_MANAGER_LINK_ETHERNET,
		IOT_MANAGER_LINK_WIFI,
		IOT_MANAGER_LINK_CELLULAR,
	};
	uint32_t lIndex;

	if (serviceId == IOT_MANAGER_SERVICE_BLE_LOCAL) {
		return iotManagerLinkCanRunService(&gIotManagerState.publicState.links[IOT_MANAGER_LINK_BLE], serviceId) ?
			IOT_MANAGER_LINK_BLE : IOT_MANAGER_LINK_NONE;
	}

	for (lIndex = 0U; lIndex < (uint32_t)(sizeof(lCloudPriority) / sizeof(lCloudPriority[0])); ++lIndex) {
		eIotManagerLinkId lLinkId;

		lLinkId = lCloudPriority[lIndex];
		if (iotManagerLinkCanRunService(&gIotManagerState.publicState.links[lLinkId], serviceId)) {
			return lLinkId;
		}
	}

	return IOT_MANAGER_LINK_NONE;
}

static void iotManagerRefreshRouteLocked(stIotManagerServiceRoute *route)
{
	eIotManagerLinkId lCandidate;

	if (route == NULL) {
		return;
	}

	lCandidate = IOT_MANAGER_LINK_NONE;
	if ((route->preferredLink != IOT_MANAGER_LINK_NONE) && iotManagerIsValidLink(route->preferredLink) &&
		iotManagerLinkSupportsService(route->preferredLink, route->serviceId) &&
		iotManagerLinkCanRunService(&gIotManagerState.publicState.links[route->preferredLink], route->serviceId)) {
		lCandidate = route->preferredLink;
	} else if (route->policy == IOT_MANAGER_SEND_POLICY_AUTO) {
		lCandidate = iotManagerPickAutoLink(route->serviceId);
	}

	route->activeLink = lCandidate;
	if (lCandidate != IOT_MANAGER_LINK_NONE) {
		route->state = IOT_MANAGER_SERVICE_STATE_READY;
	} else if ((route->preferredLink != IOT_MANAGER_LINK_NONE) && (route->policy != IOT_MANAGER_SEND_POLICY_AUTO)) {
		route->state = IOT_MANAGER_SERVICE_STATE_ERROR;
	} else {
		route->state = IOT_MANAGER_SERVICE_STATE_WAIT_LINK;
	}
}

static void iotManagerRefreshStateLocked(void)
{
	uint32_t lIndex;
	stIotManagerServiceRoute *lRoutes[4];

	lRoutes[0] = &gIotManagerState.publicState.bleLocalRoute;
	lRoutes[1] = &gIotManagerState.publicState.mqttAuthRoute;
	lRoutes[2] = &gIotManagerState.publicState.mqttRoute;
	lRoutes[3] = &gIotManagerState.publicState.tcpServerRoute;

	for (lIndex = 0U; lIndex < (uint32_t)IOT_MANAGER_LINK_MAX; ++lIndex) {
		gIotManagerState.publicState.links[lIndex].selected = false;
	}

	for (lIndex = 0U; lIndex < 4U; ++lIndex) {
		stIotManagerServiceRoute *lRoute;

		lRoute = lRoutes[lIndex];
		iotManagerRefreshRouteLocked(lRoute);
		if (iotManagerIsValidLink(lRoute->activeLink)) {
			gIotManagerState.publicState.links[lRoute->activeLink].selected = true;
		}
	}

	gIotManagerState.publicState.installedCellularType =
		gIotManagerState.publicState.links[IOT_MANAGER_LINK_CELLULAR].cellularType;
	gIotManagerState.publicState.localBleReady =
		iotManagerLinkCanRunService(&gIotManagerState.publicState.links[IOT_MANAGER_LINK_BLE],
					   IOT_MANAGER_SERVICE_BLE_LOCAL);
	gIotManagerState.publicState.cloudAnyReady =
		iotManagerPickAutoLink(IOT_MANAGER_SERVICE_MQTT) != IOT_MANAGER_LINK_NONE;
	gIotManagerState.publicState.mqttAuthDone =
		gIotManagerState.publicState.links[IOT_MANAGER_LINK_WIFI].mqttAuthReady ||
		gIotManagerState.publicState.links[IOT_MANAGER_LINK_CELLULAR].mqttAuthReady ||
		gIotManagerState.publicState.links[IOT_MANAGER_LINK_ETHERNET].mqttAuthReady;
}

static bool iotManagerApplyCompatibilitySelectionLocked(eIotManagerLinkId linkId)
{
	stIotManagerServiceRoute *lBleRoute;
	stIotManagerServiceRoute *lMqttAuthRoute;
	stIotManagerServiceRoute *lMqttRoute;
	stIotManagerServiceRoute *lTcpRoute;

	lBleRoute = &gIotManagerState.publicState.bleLocalRoute;
	lMqttAuthRoute = &gIotManagerState.publicState.mqttAuthRoute;
	lMqttRoute = &gIotManagerState.publicState.mqttRoute;
	lTcpRoute = &gIotManagerState.publicState.tcpServerRoute;

	if (linkId == IOT_MANAGER_LINK_NONE) {
		lBleRoute->preferredLink = IOT_MANAGER_LINK_NONE;
		lBleRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
		lMqttAuthRoute->preferredLink = IOT_MANAGER_LINK_NONE;
		lMqttAuthRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
		lMqttRoute->preferredLink = IOT_MANAGER_LINK_NONE;
		lMqttRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
		lTcpRoute->preferredLink = IOT_MANAGER_LINK_NONE;
		lTcpRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
		iotManagerRefreshStateLocked();
		return true;
	}

	if (!iotManagerIsValidLink(linkId)) {
		return false;
	}

	if (linkId == IOT_MANAGER_LINK_BLE) {
		lBleRoute->preferredLink = linkId;
		lBleRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
		iotManagerRefreshStateLocked();
		return true;
	}

	lMqttAuthRoute->preferredLink = linkId;
	lMqttAuthRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
	lMqttRoute->preferredLink = linkId;
	lMqttRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
	if (iotManagerLinkSupportsService(linkId, IOT_MANAGER_SERVICE_TCP_SERVER)) {
		lTcpRoute->preferredLink = linkId;
		lTcpRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
	}
	iotManagerRefreshStateLocked();

	return true;
}

static void iotManagerFillRawCodec(stCprsensorProtocolCodecCfg *codecCfg)
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

static void iotManagerFillCipherCodec(stCprsensorProtocolCodecCfg *codecCfg)
{
	iotManagerFillRawCodec(codecCfg);
	if (codecCfg == NULL) {
		return;
	}

	codecCfg->cipher.enabled = gIotManagerState.cipherReady;
	codecCfg->cipher.blockSize = CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE;
	codecCfg->cipher.encrypt = iotManagerAesEncrypt;
	codecCfg->cipher.decrypt = iotManagerAesDecrypt;
	codecCfg->cipher.userData = gIotManagerState.aesKey;
}

static bool iotManagerAesTransform(void *userData, uint8_t *buffer, uint16_t length, bool encryptMode)
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

static bool iotManagerAesEncrypt(void *userData, uint8_t *buffer, uint16_t length)
{
	return iotManagerAesTransform(userData, buffer, length, true);
}

static bool iotManagerAesDecrypt(void *userData, uint8_t *buffer, uint16_t length)
{
	return iotManagerAesTransform(userData, buffer, length, false);
}

static bool iotManagerParseMacString(const char *text, uint8_t *mac, uint16_t macSize)
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

static bool iotManagerTryInitCipherKey(void)
{
	char macText[32];
	uint8_t macBytes[CPRSENSOR_PROTOCOL_MAC_LEN];

	if (gIotManagerState.cipherReady) {
		return true;
	}

	if (!wirelessGetMacAddress(macText, (uint16_t)sizeof(macText))) {
		return false;
	}

	if (!iotManagerParseMacString(macText, macBytes, (uint16_t)sizeof(macBytes))) {
		return false;
	}

	if (md5CalcData(macBytes, (uint32_t)sizeof(macBytes), gIotManagerState.aesKey) != MD5_STATUS_OK) {
		return false;
	}

	gIotManagerState.cipherReady = true;
	return true;
}

static bool iotManagerGetExpectedPayloadLen(eCprsensorProtocolCmd cmd, uint16_t *payloadLen)
{
	if (payloadLen == NULL) {
		return false;
	}

	switch (cmd) {
	case CPRSENSOR_PROTOCOL_CMD_HANDSHAKE:
		*payloadLen = CPRSENSOR_PROTOCOL_MAC_LEN;
		return true;
	case CPRSENSOR_PROTOCOL_CMD_HEARTBEAT:
	case CPRSENSOR_PROTOCOL_CMD_DISCONNECT:
	case CPRSENSOR_PROTOCOL_CMD_DEV_INFO:
	case CPRSENSOR_PROTOCOL_CMD_BLE_INFO:
	case CPRSENSOR_PROTOCOL_CMD_BATTERY:
	case CPRSENSOR_PROTOCOL_CMD_CLEAR_MEMORY:
		*payloadLen = 0U;
		return true;
	case CPRSENSOR_PROTOCOL_CMD_COMM_SETTING:
		*payloadLen = (uint16_t)sizeof(stCprsensorProtocolCommSettingPayload);
		return true;
	case CPRSENSOR_PROTOCOL_CMD_TIME_SYNC:
		*payloadLen = (uint16_t)sizeof(stCprsensorProtocolTimeSyncPayload);
		return true;
	case CPRSENSOR_PROTOCOL_CMD_LANGUAGE:
		*payloadLen = (uint16_t)sizeof(stCprsensorProtocolLanguagePayload);
		return true;
	case CPRSENSOR_PROTOCOL_CMD_VOLUME:
		*payloadLen = (uint16_t)sizeof(stCprsensorProtocolVolumePayload);
		return true;
	case CPRSENSOR_PROTOCOL_CMD_METRONOME:
		*payloadLen = (uint16_t)sizeof(stCprsensorProtocolMetronomePayload);
		return true;
	case CPRSENSOR_PROTOCOL_CMD_UTC_SETTING:
		*payloadLen = (uint16_t)sizeof(stCprsensorProtocolUtcSettingPayload);
		return true;
	default:
		break;
	}

	return false;
}

static bool iotManagerShouldTryCipher(eCprsensorProtocolCmd cmd, uint16_t payloadLen)
{
	uint16_t expectedLen;

	if ((!gIotManagerState.cipherReady) || (payloadLen == 0U) ||
		((payloadLen % CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE) != 0U)) {
		return false;
	}

	if (!iotManagerGetExpectedPayloadLen(cmd, &expectedLen)) {
		return false;
	}

	return payloadLen != expectedLen;
}

static eCprsensorProtocolStatus iotManagerParseIncomingFrame(const uint8_t *buffer,
						      uint16_t length,
						      stCprsensorProtocolFrameView *frameView,
						      uint8_t *payloadBuffer,
						      uint16_t payloadBufferSize)
{
	stCprsensorProtocolCodecCfg rawCodec;
	stCprsensorProtocolCodecCfg cipherCodec;
	eCprsensorProtocolStatus status;
	stCprsensorProtocolFrameView cipherView;

	iotManagerFillRawCodec(&rawCodec);
	status = cprsensorProtocolParseFrame(buffer,
					     length,
					     &rawCodec,
					     payloadBuffer,
					     payloadBufferSize,
					     frameView);
	if (status != CPRSENSOR_PROTOCOL_STATUS_OK) {
		return status;
	}

	if (!iotManagerShouldTryCipher(frameView->cmd, frameView->payloadLen)) {
		return CPRSENSOR_PROTOCOL_STATUS_OK;
	}

	iotManagerFillCipherCodec(&cipherCodec);
	status = cprsensorProtocolParseFrame(buffer,
					     length,
					     &cipherCodec,
					     payloadBuffer,
					     payloadBufferSize,
					     &cipherView);
	if (status == CPRSENSOR_PROTOCOL_STATUS_OK) {
		*frameView = cipherView;
		return CPRSENSOR_PROTOCOL_STATUS_OK;
	}

	return CPRSENSOR_PROTOCOL_STATUS_OK;
}

static void iotManagerSetTxLink(eIotManagerLinkId linkId)
{
	if (iotManagerIsValidLink(linkId)) {
		gIotManagerState.txLink = linkId;
		if (linkId == IOT_MANAGER_LINK_BLE) {
			repRtosEnterCritical();
			gIotManagerState.publicState.bleLocalRoute.preferredLink = linkId;
			gIotManagerState.publicState.bleLocalRoute.policy = IOT_MANAGER_SEND_POLICY_FIXED;
			iotManagerRefreshStateLocked();
			repRtosExitCritical();
		}
	}
}

static void iotManagerSetReplyFlag(eCprsensorProtocolCmd cmd)
{
	switch (cmd) {
	case CPRSENSOR_PROTOCOL_CMD_HANDSHAKE:
		gIotManagerState.txFlags.bits.HandShake = 1U;
		break;
	case CPRSENSOR_PROTOCOL_CMD_HEARTBEAT:
		gIotManagerState.txFlags.bits.HeartBeat = 1U;
		break;
	case CPRSENSOR_PROTOCOL_CMD_DISCONNECT:
		gIotManagerState.txFlags.bits.Disconnect = 1U;
		break;
	case CPRSENSOR_PROTOCOL_CMD_DEV_INFO:
		gIotManagerState.txFlags.bits.DevInfo = 1U;
		break;
	case CPRSENSOR_PROTOCOL_CMD_BLE_INFO:
		gIotManagerState.txFlags.bits.BleInfo = 1U;
		break;
	case CPRSENSOR_PROTOCOL_CMD_BATTERY:
		gIotManagerState.txFlags.bits.Battery = 1U;
		break;
	case CPRSENSOR_PROTOCOL_CMD_LANGUAGE:
		gIotManagerState.txFlags.bits.Language = 1U;
		break;
	case CPRSENSOR_PROTOCOL_CMD_VOLUME:
		gIotManagerState.txFlags.bits.Volume = 1U;
		break;
	case CPRSENSOR_PROTOCOL_CMD_METRONOME:
		gIotManagerState.txFlags.bits.Metronome = 1U;
		break;
	case CPRSENSOR_PROTOCOL_CMD_UTC_SETTING:
		gIotManagerState.txFlags.bits.UtcSetting = 1U;
		break;
	default:
		break;
	}
}

static void iotManagerHandleFrame(eIotManagerLinkId linkId, const stCprsensorProtocolFrameView *frameView)
{
	if (frameView == NULL) {
		return;
	}

	iotManagerSetTxLink(linkId);
	switch (frameView->cmd) {
	case CPRSENSOR_PROTOCOL_CMD_HANDSHAKE:
		if ((frameView->payload == NULL) || (frameView->payloadLen < CPRSENSOR_PROTOCOL_MAC_LEN)) {
			LOG_W(IOT_MANAGER_LOG_TAG, "invalid handshake len=%u", (unsigned int)frameView->payloadLen);
			return;
		}
		(void)memcpy(gIotManagerState.handshakePayload.mac,
			     frameView->payload,
			     CPRSENSOR_PROTOCOL_MAC_LEN);
		iotManagerSetReplyFlag(frameView->cmd);
		break;
	case CPRSENSOR_PROTOCOL_CMD_HEARTBEAT:
	case CPRSENSOR_PROTOCOL_CMD_DISCONNECT:
	case CPRSENSOR_PROTOCOL_CMD_DEV_INFO:
	case CPRSENSOR_PROTOCOL_CMD_BLE_INFO:
	case CPRSENSOR_PROTOCOL_CMD_BATTERY:
		iotManagerSetReplyFlag(frameView->cmd);
		break;
	case CPRSENSOR_PROTOCOL_CMD_COMM_SETTING:
		if ((frameView->payload == NULL) || (frameView->payloadLen < sizeof(gIotManagerState.commSettingPayload))) {
			LOG_W(IOT_MANAGER_LOG_TAG, "invalid comm setting len=%u", (unsigned int)frameView->payloadLen);
			return;
		}
		(void)memcpy(&gIotManagerState.commSettingPayload,
			     frameView->payload,
			     sizeof(gIotManagerState.commSettingPayload));
		break;
	case CPRSENSOR_PROTOCOL_CMD_TIME_SYNC:
		if ((frameView->payload == NULL) || (frameView->payloadLen < sizeof(gIotManagerState.timeSyncPayload))) {
			LOG_W(IOT_MANAGER_LOG_TAG, "invalid time sync len=%u", (unsigned int)frameView->payloadLen);
			return;
		}
		(void)memcpy(&gIotManagerState.timeSyncPayload,
			     frameView->payload,
			     sizeof(gIotManagerState.timeSyncPayload));
		break;
	case CPRSENSOR_PROTOCOL_CMD_LANGUAGE:
		if ((frameView->payload == NULL) || (frameView->payloadLen < sizeof(gIotManagerState.languagePayload))) {
			LOG_W(IOT_MANAGER_LOG_TAG, "invalid language len=%u", (unsigned int)frameView->payloadLen);
			return;
		}
		(void)memcpy(&gIotManagerState.languagePayload,
			     frameView->payload,
			     sizeof(gIotManagerState.languagePayload));
		iotManagerSetReplyFlag(frameView->cmd);
		break;
	case CPRSENSOR_PROTOCOL_CMD_VOLUME:
		if ((frameView->payload == NULL) || (frameView->payloadLen < sizeof(gIotManagerState.volumePayload))) {
			LOG_W(IOT_MANAGER_LOG_TAG, "invalid volume len=%u", (unsigned int)frameView->payloadLen);
			return;
		}
		(void)memcpy(&gIotManagerState.volumePayload,
			     frameView->payload,
			     sizeof(gIotManagerState.volumePayload));
		iotManagerSetReplyFlag(frameView->cmd);
		break;
	case CPRSENSOR_PROTOCOL_CMD_METRONOME:
		if ((frameView->payload == NULL) || (frameView->payloadLen < sizeof(gIotManagerState.metronomePayload))) {
			LOG_W(IOT_MANAGER_LOG_TAG, "invalid metronome len=%u", (unsigned int)frameView->payloadLen);
			return;
		}
		(void)memcpy(&gIotManagerState.metronomePayload,
			     frameView->payload,
			     sizeof(gIotManagerState.metronomePayload));
		iotManagerSetReplyFlag(frameView->cmd);
		break;
	case CPRSENSOR_PROTOCOL_CMD_UTC_SETTING:
		if ((frameView->payload == NULL) || (frameView->payloadLen < sizeof(gIotManagerState.utcSettingPayload))) {
			LOG_W(IOT_MANAGER_LOG_TAG, "invalid utc len=%u", (unsigned int)frameView->payloadLen);
			return;
		}
		(void)memcpy(&gIotManagerState.utcSettingPayload,
			     frameView->payload,
			     sizeof(gIotManagerState.utcSettingPayload));
		iotManagerSetReplyFlag(frameView->cmd);
		break;
	default:
		LOG_W(IOT_MANAGER_LOG_TAG,
		      "ignore cmd=0x%02X payloadLen=%u",
		      (unsigned int)frameView->cmd,
		      (unsigned int)frameView->payloadLen);
		break;
	}
}

bool iotManagerPushReceivedData(eIotManagerInterface interfaceType, const uint8_t *buffer, uint16_t length)
{
	eIotManagerLinkId lLinkId;

	iotManagerEnsureStateInitialized();
	lLinkId = iotManagerInterfaceToLink(interfaceType);
	if (!iotManagerIsValidInterface(interfaceType) ||
		(buffer == NULL) || (length == 0U) || (length > IOT_MANAGER_RX_BUFFER_SIZE)) {
		return false;
	}

	repRtosEnterCritical();
	(void)memcpy(gIotManagerState.rxBuffer, buffer, length);
	gIotManagerState.rxLength = length;
	gIotManagerState.rxLink = lLinkId;
	gIotManagerState.rxPending = true;
	repRtosExitCritical();
	return true;
}

bool iotManagerSendByLink(eIotManagerLinkId linkId, const uint8_t *buffer, uint16_t length)
{
	if (!iotManagerIsValidLink(linkId) || (buffer == NULL) || (length == 0U)) {
		return false;
	}

	switch (linkId) {
	case IOT_MANAGER_LINK_BLE:
	case IOT_MANAGER_LINK_WIFI:
		return wirelessSendData(buffer, length);
	case IOT_MANAGER_LINK_CELLULAR:
		return cellularSendData(buffer, length);
	case IOT_MANAGER_LINK_ETHERNET:
		return ethernetSendData(buffer, length);
	default:
		break;
	}

	return false;
}

bool iotManagerSend(eIotManagerServiceId serviceId, const uint8_t *buffer, uint16_t length)
{
	stIotManagerServiceRoute *lRoute;
	eIotManagerLinkId lLinkId;

	iotManagerEnsureStateInitialized();
	if ((buffer == NULL) || (length == 0U)) {
		return false;
	}

	repRtosEnterCritical();
	lRoute = iotManagerGetRouteByServiceId(serviceId);
	if (lRoute == NULL) {
		repRtosExitCritical();
		return false;
	}
	iotManagerRefreshStateLocked();
	lLinkId = lRoute->activeLink;
	repRtosExitCritical();

	return iotManagerSendByLink(lLinkId, buffer, length);
}

bool iotManagerSendByInterface(eIotManagerInterface interfaceType, const uint8_t *buffer, uint16_t length)
{
	return iotManagerSendByLink(iotManagerInterfaceToLink(interfaceType), buffer, length);
}

bool iotManagerUpdateLinkState(eIotManagerLinkId linkId, const stIotManagerLinkRuntime *runtime)
{
	stIotManagerLinkRuntime *lRuntime;

	iotManagerEnsureStateInitialized();
	if (!iotManagerIsValidLink(linkId) || (runtime == NULL)) {
		return false;
	}

	repRtosEnterCritical();
	lRuntime = &gIotManagerState.publicState.links[linkId];
	*lRuntime = *runtime;
	lRuntime->linkId = linkId;
	if (iotManagerLinkCapsIsEmpty(&lRuntime->caps)) {
		lRuntime->caps = gIotManagerDefaultLinkCaps[linkId];
	}
	iotManagerRefreshStateLocked();
	repRtosExitCritical();
	return true;
}

bool iotManagerSelectRoute(eIotManagerServiceId serviceId, eIotManagerLinkId linkId)
{
	stIotManagerServiceRoute *lRoute;

	iotManagerEnsureStateInitialized();
	if ((linkId != IOT_MANAGER_LINK_NONE) &&
		(!iotManagerIsValidLink(linkId) || !iotManagerLinkSupportsService(linkId, serviceId))) {
		return false;
	}

	repRtosEnterCritical();
	lRoute = iotManagerGetRouteByServiceId(serviceId);
	if (lRoute == NULL) {
		repRtosExitCritical();
		return false;
	}
	lRoute->preferredLink = linkId;
	lRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
	iotManagerRefreshStateLocked();
	repRtosExitCritical();
	return true;
}

bool iotManagerSetActiveInterface(eIotManagerInterface interfaceType)
{
	eIotManagerLinkId lLinkId;

	iotManagerEnsureStateInitialized();
	if ((interfaceType != IOT_MANAGER_INTERFACE_NONE) && !iotManagerIsValidInterface(interfaceType)) {
		return false;
	}

	lLinkId = iotManagerInterfaceToLink(interfaceType);
	repRtosEnterCritical();
	if (!iotManagerApplyCompatibilitySelectionLocked(lLinkId)) {
		repRtosExitCritical();
		return false;
	}
	repRtosExitCritical();
	return true;
}

bool iotManagerSetTargetInterface(eIotManagerInterface interfaceType)
{
	eIotManagerLinkId lLinkId;

	iotManagerEnsureStateInitialized();
	if ((interfaceType != IOT_MANAGER_INTERFACE_NONE) && !iotManagerIsValidInterface(interfaceType)) {
		return false;
	}

	lLinkId = iotManagerInterfaceToLink(interfaceType);
	repRtosEnterCritical();
	if (!iotManagerApplyCompatibilitySelectionLocked(lLinkId)) {
		repRtosExitCritical();
		return false;
	}
	repRtosExitCritical();
	return true;
}

bool iotManagerSetInterfaceReady(eIotManagerInterface interfaceType, bool ready)
{
	stIotManagerLinkRuntime *lRuntime;
	eIotManagerLinkId lLinkId;
	uint32_t lTick;

	iotManagerEnsureStateInitialized();
	if (!iotManagerIsValidInterface(interfaceType)) {
		return false;
	}

	lLinkId = iotManagerInterfaceToLink(interfaceType);
	repRtosEnterCritical();
	lRuntime = &gIotManagerState.publicState.links[lLinkId];
	lTick = repRtosGetTickMs();
	lRuntime->installed = true;
	lRuntime->enabled = ready;
	lRuntime->moduleReady = ready;
	lRuntime->busy = false;
	if (!ready) {
		lRuntime->state = IOT_MANAGER_LINK_STATE_DISABLED;
		lRuntime->netReady = false;
		lRuntime->peerConnected = false;
		lRuntime->mqttAuthReady = false;
		lRuntime->mqttReady = false;
		lRuntime->tcpServerListening = false;
		lRuntime->tcpClientConnected = false;
		lRuntime->lastFailTick = lTick;
	} else {
		lRuntime->state = (lLinkId == IOT_MANAGER_LINK_BLE) ? IOT_MANAGER_LINK_STATE_READY : IOT_MANAGER_LINK_STATE_NET_READY;
		lRuntime->peerConnected = (lLinkId == IOT_MANAGER_LINK_BLE);
		lRuntime->netReady = (lLinkId != IOT_MANAGER_LINK_BLE);
		lRuntime->mqttAuthReady = lRuntime->netReady && lRuntime->caps.supportMqttAuthHttp;
		lRuntime->mqttReady = false;
		lRuntime->tcpServerListening = false;
		lRuntime->tcpClientConnected = false;
		lRuntime->lastOkTick = lTick;
	}

	iotManagerRefreshStateLocked();
	repRtosExitCritical();
	return true;
}

bool iotManagerSetInterfaceStatus(eIotManagerInterface interfaceType, eIotManagerNetStatus status)
{
	stIotManagerLinkRuntime *lRuntime;
	eIotManagerLinkId lLinkId;
	uint32_t lTick;

	iotManagerEnsureStateInitialized();
	if (!iotManagerIsValidInterface(interfaceType) || (status >= IOT_MANAGER_NET_STATUS_MAX)) {
		return false;
	}

	lLinkId = iotManagerInterfaceToLink(interfaceType);
	repRtosEnterCritical();
	lRuntime = &gIotManagerState.publicState.links[lLinkId];
	lTick = repRtosGetTickMs();
	lRuntime->installed = true;
	lRuntime->enabled = true;
	lRuntime->busy = false;
	switch (status) {
	case IOT_MANAGER_NET_STATUS_UNKNOWN:
		lRuntime->state = IOT_MANAGER_LINK_STATE_INITING;
		lRuntime->moduleReady = false;
		lRuntime->netReady = false;
		lRuntime->peerConnected = false;
		lRuntime->mqttAuthReady = false;
		lRuntime->mqttReady = false;
		lRuntime->tcpServerListening = false;
		lRuntime->tcpClientConnected = false;
		break;
	case IOT_MANAGER_NET_STATUS_IDLE:
		lRuntime->state = IOT_MANAGER_LINK_STATE_DISABLED;
		lRuntime->moduleReady = false;
		lRuntime->netReady = false;
		lRuntime->peerConnected = false;
		lRuntime->mqttAuthReady = false;
		lRuntime->mqttReady = false;
		lRuntime->tcpServerListening = false;
		lRuntime->tcpClientConnected = false;
		lRuntime->lastFailTick = lTick;
		break;
	case IOT_MANAGER_NET_STATUS_READY:
		lRuntime->state = (lLinkId == IOT_MANAGER_LINK_BLE) ? IOT_MANAGER_LINK_STATE_READY : IOT_MANAGER_LINK_STATE_NET_READY;
		lRuntime->moduleReady = true;
		lRuntime->netReady = (lLinkId != IOT_MANAGER_LINK_BLE);
		lRuntime->peerConnected = (lLinkId == IOT_MANAGER_LINK_BLE);
		lRuntime->mqttAuthReady = lRuntime->netReady && lRuntime->caps.supportMqttAuthHttp;
		lRuntime->mqttReady = false;
		lRuntime->tcpServerListening = false;
		lRuntime->tcpClientConnected = false;
		lRuntime->lastOkTick = lTick;
		break;
	case IOT_MANAGER_NET_STATUS_SELECTED:
		lRuntime->state = IOT_MANAGER_LINK_STATE_SERVICE_CONNECTING;
		lRuntime->moduleReady = true;
		lRuntime->netReady = (lLinkId != IOT_MANAGER_LINK_BLE);
		lRuntime->peerConnected = (lLinkId == IOT_MANAGER_LINK_BLE);
		lRuntime->mqttAuthReady = lRuntime->netReady && lRuntime->caps.supportMqttAuthHttp;
		lRuntime->lastOkTick = lTick;
		break;
	case IOT_MANAGER_NET_STATUS_ACTIVE:
		lRuntime->state = IOT_MANAGER_LINK_STATE_SERVICE_READY;
		lRuntime->moduleReady = true;
		lRuntime->netReady = (lLinkId != IOT_MANAGER_LINK_BLE);
		lRuntime->peerConnected = (lLinkId == IOT_MANAGER_LINK_BLE);
		lRuntime->mqttAuthReady = lRuntime->netReady && lRuntime->caps.supportMqttAuthHttp;
		lRuntime->mqttReady = lRuntime->netReady && lRuntime->caps.supportMqtt;
		lRuntime->tcpServerListening = lRuntime->caps.supportTcpServer;
		lRuntime->lastOkTick = lTick;
		break;
	case IOT_MANAGER_NET_STATUS_ERROR:
		lRuntime->state = IOT_MANAGER_LINK_STATE_ERROR;
		lRuntime->mqttReady = false;
		lRuntime->tcpServerListening = false;
		lRuntime->tcpClientConnected = false;
		lRuntime->lastFailTick = lTick;
		break;
	default:
		break;
	}
	iotManagerRefreshStateLocked();
	repRtosExitCritical();
	return true;
}

const stIotManagerState *iotManagerGetState(void)
{
	iotManagerEnsureStateInitialized();
	return &gIotManagerState.publicState;
}

static bool iotManagerBuildAndSendReply(eIotManagerLinkId linkId,
						 eCprsensorProtocolCmd cmd,
						 const uint8_t *payload,
						 uint16_t payloadLen)
{
	stCprsensorProtocolCodecCfg codecCfg;
	eCprsensorProtocolStatus status;
	uint8_t frameBuffer[IOT_MANAGER_TX_FRAME_BUFFER_SIZE];
	uint16_t frameLen;

	iotManagerFillRawCodec(&codecCfg);
	frameLen = 0U;
	status = cprsensorProtocolPackFrame(cmd,
					 payload,
					 payloadLen,
					 &codecCfg,
					 frameBuffer,
					 (uint16_t)sizeof(frameBuffer),
					 &frameLen);
	if (status != CPRSENSOR_PROTOCOL_STATUS_OK) {
		LOG_W(IOT_MANAGER_LOG_TAG,
		      "pack reply failed cmd=0x%02X status=%d",
		      (unsigned int)cmd,
		      (int)status);
		return false;
	}

	return iotManagerSendByLink(linkId, frameBuffer, frameLen);
}

static void iotManagerTryFlushReply(eIotManagerLinkId linkId,
				 bool flagPending,
				 eCprsensorProtocolCmd cmd,
				 const uint8_t *payload,
				 uint16_t payloadLen,
				 void (*clearFlag)(void))
{
	if (!flagPending || (clearFlag == NULL)) {
		return;
	}

	if (iotManagerBuildAndSendReply(linkId, cmd, payload, payloadLen)) {
		clearFlag();
	}
}

static void iotManagerClearFlagHandshake(void)
{
	repRtosEnterCritical();
	gIotManagerState.txFlags.bits.HandShake = 0U;
	repRtosExitCritical();
}

static void iotManagerClearFlagHeartBeat(void)
{
	repRtosEnterCritical();
	gIotManagerState.txFlags.bits.HeartBeat = 0U;
	repRtosExitCritical();
}

static void iotManagerClearFlagDisconnect(void)
{
	repRtosEnterCritical();
	gIotManagerState.txFlags.bits.Disconnect = 0U;
	repRtosExitCritical();
}

static void iotManagerClearFlagDevInfo(void)
{
	repRtosEnterCritical();
	gIotManagerState.txFlags.bits.DevInfo = 0U;
	repRtosExitCritical();
}

static void iotManagerClearFlagBleInfo(void)
{
	repRtosEnterCritical();
	gIotManagerState.txFlags.bits.BleInfo = 0U;
	repRtosExitCritical();
}

static void iotManagerClearFlagBattery(void)
{
	repRtosEnterCritical();
	gIotManagerState.txFlags.bits.Battery = 0U;
	repRtosExitCritical();
}

static void iotManagerClearFlagLanguage(void)
{
	repRtosEnterCritical();
	gIotManagerState.txFlags.bits.Language = 0U;
	repRtosExitCritical();
}

static void iotManagerClearFlagVolume(void)
{
	repRtosEnterCritical();
	gIotManagerState.txFlags.bits.Volume = 0U;
	repRtosExitCritical();
}

static void iotManagerClearFlagMetronome(void)
{
	repRtosEnterCritical();
	gIotManagerState.txFlags.bits.Metronome = 0U;
	repRtosExitCritical();
}

static void iotManagerClearFlagUtcSetting(void)
{
	repRtosEnterCritical();
	gIotManagerState.txFlags.bits.UtcSetting = 0U;
	repRtosExitCritical();
}

static void iotManagerFlushHandshakeReply(eIotManagerLinkId linkId, bool flagPending)
{
	char macText[32];

	if (!flagPending) {
		return;
	}

	if (wirelessGetMacAddress(macText, (uint16_t)sizeof(macText)) &&
		iotManagerParseMacString(macText, gIotManagerState.handshakePayload.mac, CPRSENSOR_PROTOCOL_MAC_LEN) &&
		iotManagerBuildAndSendReply(linkId,
					 CPRSENSOR_PROTOCOL_CMD_HANDSHAKE,
					 gIotManagerState.handshakePayload.mac,
					 CPRSENSOR_PROTOCOL_MAC_LEN)) {
		iotManagerClearFlagHandshake();
	}
}

static void iotManagerFlushHeartbeatReply(eIotManagerLinkId linkId, bool flagPending)
{
	iotManagerTryFlushReply(linkId,
				 flagPending,
				 CPRSENSOR_PROTOCOL_CMD_HEARTBEAT,
				 NULL,
				 0U,
				 iotManagerClearFlagHeartBeat);
}

static void iotManagerFlushDisconnectReply(eIotManagerLinkId linkId, bool flagPending)
{
	iotManagerTryFlushReply(linkId,
				 flagPending,
				 CPRSENSOR_PROTOCOL_CMD_DISCONNECT,
				 NULL,
				 0U,
				 iotManagerClearFlagDisconnect);
}

static void iotManagerFlushDevInfoReply(eIotManagerLinkId linkId, bool flagPending)
{
	stCprsensorProtocolDevInfoReplyPayload devInfoPayload;

	if (!flagPending) {
		return;
	}

	(void)memset(&devInfoPayload, 0, sizeof(devInfoPayload));
	devInfoPayload.deviceType = IOT_MANAGER_DEVICE_TYPE;
	(void)memcpy(devInfoPayload.deviceSn,
		     "CPRSENSOR0001",
		     CPRSENSOR_PROTOCOL_DEVICE_SN_LEN);
	devInfoPayload.protocolOrFlag = IOT_MANAGER_PROTOCOL_FLAG;
	devInfoPayload.swVersion = IOT_MANAGER_SW_VERSION;
	devInfoPayload.swSubVersion = IOT_MANAGER_SW_SUB_VERSION;
	devInfoPayload.swBuildVersion = IOT_MANAGER_SW_BUILD_VERSION;
	iotManagerTryFlushReply(linkId,
				 true,
				 CPRSENSOR_PROTOCOL_CMD_DEV_INFO,
				 (const uint8_t *)&devInfoPayload,
				 (uint16_t)sizeof(devInfoPayload),
				 iotManagerClearFlagDevInfo);
}

static void iotManagerFlushBleInfoReply(eIotManagerLinkId linkId, bool flagPending)
{
	stCprsensorProtocolBleInfoReplyPayload bleInfoPayload;

	if (!flagPending) {
		return;
	}

	(void)memset(&bleInfoPayload, 0, sizeof(bleInfoPayload));
	(void)memcpy(bleInfoPayload.bleVersion,
		     "CprSensorTest-BLE-Bridge",
		     (uint16_t)strlen("CprSensorTest-BLE-Bridge"));
	iotManagerTryFlushReply(linkId,
				 true,
				 CPRSENSOR_PROTOCOL_CMD_BLE_INFO,
				 (const uint8_t *)&bleInfoPayload,
				 (uint16_t)sizeof(bleInfoPayload),
				 iotManagerClearFlagBleInfo);
}

static void iotManagerFlushBatteryReply(eIotManagerLinkId linkId, bool flagPending)
{
	stCprsensorProtocolBatteryReplyPayload batteryPayload;

	if (!flagPending) {
		return;
	}

	(void)memset(&batteryPayload, 0, sizeof(batteryPayload));
	batteryPayload.batPercent = IOT_MANAGER_DEFAULT_BATTERY_PERCENT;
	cprsensorProtocolWriteU16Be(batteryPayload.batMvBe, IOT_MANAGER_DEFAULT_BATTERY_MV);
	batteryPayload.chargeState = IOT_MANAGER_DEFAULT_CHARGE_STATE;
	iotManagerTryFlushReply(linkId,
				 true,
				 CPRSENSOR_PROTOCOL_CMD_BATTERY,
				 (const uint8_t *)&batteryPayload,
				 (uint16_t)sizeof(batteryPayload),
				 iotManagerClearFlagBattery);
}

static void iotManagerFlushLanguageReply(eIotManagerLinkId linkId, bool flagPending)
{
	iotManagerTryFlushReply(linkId,
				 flagPending,
				 CPRSENSOR_PROTOCOL_CMD_LANGUAGE,
				 (const uint8_t *)&gIotManagerState.languagePayload,
				 (uint16_t)sizeof(gIotManagerState.languagePayload),
				 iotManagerClearFlagLanguage);
}

static void iotManagerFlushVolumeReply(eIotManagerLinkId linkId, bool flagPending)
{
	iotManagerTryFlushReply(linkId,
				 flagPending,
				 CPRSENSOR_PROTOCOL_CMD_VOLUME,
				 (const uint8_t *)&gIotManagerState.volumePayload,
				 (uint16_t)sizeof(gIotManagerState.volumePayload),
				 iotManagerClearFlagVolume);
}

static void iotManagerFlushMetronomeReply(eIotManagerLinkId linkId, bool flagPending)
{
	iotManagerTryFlushReply(linkId,
				 flagPending,
				 CPRSENSOR_PROTOCOL_CMD_METRONOME,
				 (const uint8_t *)&gIotManagerState.metronomePayload,
				 (uint16_t)sizeof(gIotManagerState.metronomePayload),
				 iotManagerClearFlagMetronome);
}

static void iotManagerFlushUtcSettingReply(eIotManagerLinkId linkId, bool flagPending)
{
	iotManagerTryFlushReply(linkId,
				 flagPending,
				 CPRSENSOR_PROTOCOL_CMD_UTC_SETTING,
				 (const uint8_t *)&gIotManagerState.utcSettingPayload,
				 (uint16_t)sizeof(gIotManagerState.utcSettingPayload),
				 iotManagerClearFlagUtcSetting);
}

static void iotManagerFlushPendingReplies(void)
{
	eIotManagerLinkId linkId;
	eCprsensorProtocolTxFlag flags;

	iotManagerEnsureStateInitialized();

	repRtosEnterCritical();
	linkId = gIotManagerState.txLink;
	flags = gIotManagerState.txFlags;
	repRtosExitCritical();

	if (!iotManagerIsValidLink(linkId)) {
		return;
	}

	iotManagerFlushHandshakeReply(linkId, flags.bits.HandShake != 0U);
	iotManagerFlushHeartbeatReply(linkId, flags.bits.HeartBeat != 0U);
	iotManagerFlushDisconnectReply(linkId, flags.bits.Disconnect != 0U);
	iotManagerFlushDevInfoReply(linkId, flags.bits.DevInfo != 0U);
	iotManagerFlushBleInfoReply(linkId, flags.bits.BleInfo != 0U);
	iotManagerFlushBatteryReply(linkId, flags.bits.Battery != 0U);
	iotManagerFlushLanguageReply(linkId, flags.bits.Language != 0U);
	iotManagerFlushVolumeReply(linkId, flags.bits.Volume != 0U);
	iotManagerFlushMetronomeReply(linkId, flags.bits.Metronome != 0U);
	iotManagerFlushUtcSettingReply(linkId, flags.bits.UtcSetting != 0U);
}

static void iotManagerConsumeReceivedData(void)
{
	uint8_t rxBuffer[IOT_MANAGER_RX_BUFFER_SIZE];
	uint8_t payloadBuffer[IOT_MANAGER_RX_BUFFER_SIZE];
	uint16_t rxLength;
	eIotManagerLinkId linkId;
	stCprsensorProtocolFrameView frameView;
	eCprsensorProtocolStatus status;

	iotManagerEnsureStateInitialized();

	repRtosEnterCritical();
	if (!gIotManagerState.rxPending) {
		repRtosExitCritical();
		return;
	}

	rxLength = gIotManagerState.rxLength;
	linkId = gIotManagerState.rxLink;
	(void)memcpy(rxBuffer, gIotManagerState.rxBuffer, rxLength);
	gIotManagerState.rxPending = false;
	repRtosExitCritical();

	status = iotManagerParseIncomingFrame(rxBuffer,
					      rxLength,
					      &frameView,
					      payloadBuffer,
					      (uint16_t)sizeof(payloadBuffer));
	if (status != CPRSENSOR_PROTOCOL_STATUS_OK) {
		LOG_W(IOT_MANAGER_LOG_TAG,
		      "parse failed status=%d len=%u",
		      (int)status,
		      (unsigned int)rxLength);
		return;
	}

	iotManagerHandleFrame(linkId, &frameView);
}

void iotManagerProcess(void)
{
	iotManagerEnsureStateInitialized();
	(void)iotManagerTryInitCipherKey();
	iotManagerConsumeReceivedData();
	iotManagerFlushPendingReplies();
}

/**************************End of file********************************/
