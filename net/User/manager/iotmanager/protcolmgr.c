/***********************************************************************************
* @file     : protcolmgr.c
* @brief    : CPR sensor protocol manager.
* @details  : Parses CPR sensor frames, tracks reply state, and sends protocol replies.
* @author   : GitHub Copilot
* @date     : 2026-04-25
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "protcolmgr.h"

#include <string.h>

#include "cprsensor_protocol.h"
#include "../wireless/wireless.h"
#include "../../../rep/service/log/log.h"
#include "../../../rep/service/rtos/rtos.h"
#include "../../../rep/tools/aes/aes.h"
#include "../../../rep/tools/md5/md5.h"

static const char gProtcolMgrLogTag[] = "ptclmgr";
static const uint16_t gProtcolMgrRxBufferSize = 640U;
static const uint8_t gProtcolMgrRxQueueDepth = 32U;
static const uint8_t gProtcolMgrTxQueueDepth = 64U;
static const uint16_t gProtcolMgrTxFrameBufferSize = 128U;
static const uint8_t gProtcolMgrDeviceType = 0x01U;
static const uint8_t gProtcolMgrProtocolFlag = 0x01U;
static const uint8_t gProtcolMgrSwVersion = 0x01U;
static const uint8_t gProtcolMgrSwSubVersion = 0x00U;
static const uint8_t gProtcolMgrSwBuildVersion = 0x00U;
static const uint8_t gProtcolMgrDefaultBatteryPercent = 100U;
static const uint16_t gProtcolMgrDefaultBatteryMv = 3700U;
static const uint8_t gProtcolMgrDefaultChargeState = 0U;

static bool gProtcolMgrInitialized;
static uint8_t gProtcolMgrRxHead;
static uint8_t gProtcolMgrRxCount;
static uint16_t gProtcolMgrRxLength[32];
static eIotManagerLinkId gProtcolMgrRxLink[32];
static uint8_t gProtcolMgrRxBuffer[32][640];
static eIotManagerLinkId gProtcolMgrTxLink;
static uint8_t gProtcolMgrTxHead;
static uint8_t gProtcolMgrTxCount;
static eIotManagerLinkId gProtcolMgrTxLinkQueue[64];
static eCprsensorProtocolCmd gProtcolMgrTxCmdQueue[64];
static bool gProtcolMgrCipherReady;
static uint8_t gProtcolMgrAesKey[MD5_DIGEST_SIZE];
static stCprsensorProtocolHandshakePayload gProtcolMgrHandshakePayload;
static uint8_t gProtcolMgrWifiSsidLen;
static uint8_t gProtcolMgrWifiPwdLen;
static uint8_t gProtcolMgrWifiSsid[WIRELESS_WIFI_SSID_MAX_LEN];
static uint8_t gProtcolMgrWifiPwd[WIRELESS_WIFI_PASSWORD_MAX_LEN];
static stCprsensorProtocolLanguagePayload gProtcolMgrLanguagePayload;
static stCprsensorProtocolVolumePayload gProtcolMgrVolumePayload;
static stCprsensorProtocolMetronomePayload gProtcolMgrMetronomePayload;
static stCprsensorProtocolUtcSettingPayload gProtcolMgrUtcSettingPayload;
static stCprsensorProtocolCommSettingPayload gProtcolMgrCommSettingPayload;
static stCprsensorProtocolTimeSyncPayload gProtcolMgrTimeSyncPayload;

static void protcolMgrEnsureInitialized(void);
static void protcolMgrFillRawCodec(stCprsensorProtocolCodecCfg *codecCfg);
static void protcolMgrFillCipherCodec(stCprsensorProtocolCodecCfg *codecCfg);
static bool protcolMgrAesTransform(void *userData, uint8_t *buffer, uint16_t length, bool encryptMode);
static bool protcolMgrAesEncrypt(void *userData, uint8_t *buffer, uint16_t length);
static bool protcolMgrAesDecrypt(void *userData, uint8_t *buffer, uint16_t length);
static bool protcolMgrParseMacString(const char *text, uint8_t *mac, uint16_t macSize);
static eCprsensorProtocolReplySlot protcolMgrGetReplySlot(eCprsensorProtocolCmd cmd);
static void protcolMgrClearReplyPending(eCprsensorProtocolReplySlot replySlot);
static bool protcolMgrQueueReply(eIotManagerLinkId linkId, eCprsensorProtocolCmd cmd);
static bool protcolMgrPeekReply(eIotManagerLinkId *linkId, eCprsensorProtocolCmd *cmd);
static bool protcolMgrIsReplyHead(eIotManagerLinkId linkId, eCprsensorProtocolCmd cmd);
static bool protcolMgrIsTxQueueFull(void);
static void protcolMgrPromoteReplyLink(eIotManagerLinkId linkId);
static void protcolMgrRotateReply(void);
static bool protcolMgrGetExpectedPayloadLen(eCprsensorProtocolCmd cmd, uint16_t *payloadLen);
static bool protcolMgrShouldTryCipher(eCprsensorProtocolCmd cmd, uint16_t payloadLen);
static eCprsensorProtocolStatus protcolMgrParseIncomingFrame(const uint8_t *buffer,
					      uint16_t length,
					      stCprsensorProtocolFrameView *frameView,
					      uint8_t *payloadBuffer,
					      uint16_t payloadBufferSize);
static void protcolMgrSetTxLink(eIotManagerLinkId linkId);
static void protcolMgrSetReplyFlag(eCprsensorProtocolCmd cmd);
static void protcolMgrHandleFrame(eIotManagerLinkId linkId, const stCprsensorProtocolFrameView *frameView);
static bool protcolMgrBuildAndSendReply(eIotManagerLinkId linkId, eCprsensorProtocolCmd cmd, const uint8_t *payload, uint16_t payloadLen);
static void protcolMgrFlushHandshakeReply(eIotManagerLinkId linkId);
static void protcolMgrFlushHeartbeatReply(eIotManagerLinkId linkId);
static void protcolMgrFlushDisconnectReply(eIotManagerLinkId linkId);
static void protcolMgrFlushDevInfoReply(eIotManagerLinkId linkId);
static void protcolMgrFlushBleInfoReply(eIotManagerLinkId linkId);
static void protcolMgrFlushWifiSettingReply(eIotManagerLinkId linkId);
static void protcolMgrFlushBatteryReply(eIotManagerLinkId linkId);
static void protcolMgrFlushLanguageReply(eIotManagerLinkId linkId);
static void protcolMgrFlushVolumeReply(eIotManagerLinkId linkId);
static void protcolMgrFlushMetronomeReply(eIotManagerLinkId linkId);
static void protcolMgrFlushUtcSettingReply(eIotManagerLinkId linkId);
static void protcolMgrFlushPendingReplies(void);
static void protcolMgrConsumeReceivedData(void);

static const struct {
	eCprsensorProtocolReplySlot replySlot;
	void (*flushReply)(eIotManagerLinkId linkId);
} gProtcolMgrReplyFlushTable[] = {
	{ CPRSENSOR_PROTOCOL_REPLY_SLOT_HANDSHAKE, protcolMgrFlushHandshakeReply },
	{ CPRSENSOR_PROTOCOL_REPLY_SLOT_HEARTBEAT, protcolMgrFlushHeartbeatReply },
	{ CPRSENSOR_PROTOCOL_REPLY_SLOT_DISCONNECT, protcolMgrFlushDisconnectReply },
	{ CPRSENSOR_PROTOCOL_REPLY_SLOT_DEV_INFO, protcolMgrFlushDevInfoReply },
	{ CPRSENSOR_PROTOCOL_REPLY_SLOT_BLE_INFO, protcolMgrFlushBleInfoReply },
	{ CPRSENSOR_PROTOCOL_REPLY_SLOT_WIFI_SETTING, protcolMgrFlushWifiSettingReply },
	{ CPRSENSOR_PROTOCOL_REPLY_SLOT_BATTERY, protcolMgrFlushBatteryReply },
	{ CPRSENSOR_PROTOCOL_REPLY_SLOT_LANGUAGE, protcolMgrFlushLanguageReply },
	{ CPRSENSOR_PROTOCOL_REPLY_SLOT_VOLUME, protcolMgrFlushVolumeReply },
	{ CPRSENSOR_PROTOCOL_REPLY_SLOT_METRONOME, protcolMgrFlushMetronomeReply },
	{ CPRSENSOR_PROTOCOL_REPLY_SLOT_UTC_SETTING, protcolMgrFlushUtcSettingReply },
};

static void protcolMgrEnsureInitialized(void)
{
	if (gProtcolMgrInitialized) {
		return;
	}

	gProtcolMgrTxLink = IOT_MANAGER_LINK_BLE;
	gProtcolMgrRxHead = 0U;
	gProtcolMgrRxCount = 0U;
	gProtcolMgrTxHead = 0U;
	gProtcolMgrTxCount = 0U;
	gProtcolMgrInitialized = true;
}

static void protcolMgrFillRawCodec(stCprsensorProtocolCodecCfg *codecCfg)
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

static void protcolMgrFillCipherCodec(stCprsensorProtocolCodecCfg *codecCfg)
{
	protcolMgrFillRawCodec(codecCfg);
	if (codecCfg == NULL) {
		return;
	}

	codecCfg->cipher.enabled = gProtcolMgrCipherReady;
	codecCfg->cipher.blockSize = CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE;
	codecCfg->cipher.encrypt = protcolMgrAesEncrypt;
	codecCfg->cipher.decrypt = protcolMgrAesDecrypt;
	codecCfg->cipher.userData = gProtcolMgrAesKey;
}

static bool protcolMgrAesTransform(void *userData, uint8_t *buffer, uint16_t length, bool encryptMode)
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

static bool protcolMgrAesEncrypt(void *userData, uint8_t *buffer, uint16_t length)
{
	return protcolMgrAesTransform(userData, buffer, length, true);
}

static bool protcolMgrAesDecrypt(void *userData, uint8_t *buffer, uint16_t length)
{
	return protcolMgrAesTransform(userData, buffer, length, false);
}

static bool protcolMgrParseMacString(const char *text, uint8_t *mac, uint16_t macSize)
{
	uint16_t lIndex;
	uint8_t lHighNibble;
	uint8_t lValue;
	char lCh;
	bool lHighReady;

	if ((text == NULL) || (mac == NULL) || (macSize < CPRSENSOR_PROTOCOL_MAC_LEN)) {
		return false;
	}

	lIndex = 0U;
	lHighNibble = 0U;
	lHighReady = false;
	while (*text != '\0') {
		lCh = *text++;
		if ((lCh == ':') || (lCh == '-') || (lCh == ' ')) {
			continue;
		}

		if ((lCh >= '0') && (lCh <= '9')) {
			lValue = (uint8_t)(lCh - '0');
		} else if ((lCh >= 'a') && (lCh <= 'f')) {
			lValue = (uint8_t)(lCh - 'a' + 10);
		} else if ((lCh >= 'A') && (lCh <= 'F')) {
			lValue = (uint8_t)(lCh - 'A' + 10);
		} else {
			return false;
		}

		if (!lHighReady) {
			lHighNibble = (uint8_t)(lValue << 4U);
			lHighReady = true;
		} else {
			if (lIndex >= CPRSENSOR_PROTOCOL_MAC_LEN) {
				return false;
			}
			mac[lIndex++] = (uint8_t)(lHighNibble | lValue);
			lHighReady = false;
		}
	}

	return (!lHighReady) && (lIndex == CPRSENSOR_PROTOCOL_MAC_LEN);
}

bool protcolMgrTryInitCipherKey(void)
{
	char lMacText[32];
	uint8_t lMacBytes[CPRSENSOR_PROTOCOL_MAC_LEN];

	protcolMgrEnsureInitialized();
	if (gProtcolMgrCipherReady) {
		return true;
	}

	if (!wirelessGetMacAddress(lMacText, (uint16_t)sizeof(lMacText))) {
		return false;
	}

	if (!protcolMgrParseMacString(lMacText, lMacBytes, (uint16_t)sizeof(lMacBytes))) {
		return false;
	}

	if (md5CalcData(lMacBytes, (uint32_t)sizeof(lMacBytes), gProtcolMgrAesKey) != MD5_STATUS_OK) {
		return false;
	}

	gProtcolMgrCipherReady = true;
	return true;
}

static eCprsensorProtocolReplySlot protcolMgrGetReplySlot(eCprsensorProtocolCmd cmd)
{
	switch (cmd) {
	case CPRSENSOR_PROTOCOL_CMD_HANDSHAKE:
		return CPRSENSOR_PROTOCOL_REPLY_SLOT_HANDSHAKE;
	case CPRSENSOR_PROTOCOL_CMD_HEARTBEAT:
		return CPRSENSOR_PROTOCOL_REPLY_SLOT_HEARTBEAT;
	case CPRSENSOR_PROTOCOL_CMD_DISCONNECT:
		return CPRSENSOR_PROTOCOL_REPLY_SLOT_DISCONNECT;
	case CPRSENSOR_PROTOCOL_CMD_DEV_INFO:
		return CPRSENSOR_PROTOCOL_REPLY_SLOT_DEV_INFO;
	case CPRSENSOR_PROTOCOL_CMD_BLE_INFO:
		return CPRSENSOR_PROTOCOL_REPLY_SLOT_BLE_INFO;
	case CPRSENSOR_PROTOCOL_CMD_WIFI_SETTING:
		return CPRSENSOR_PROTOCOL_REPLY_SLOT_WIFI_SETTING;
	case CPRSENSOR_PROTOCOL_CMD_BATTERY:
		return CPRSENSOR_PROTOCOL_REPLY_SLOT_BATTERY;
	case CPRSENSOR_PROTOCOL_CMD_LANGUAGE:
		return CPRSENSOR_PROTOCOL_REPLY_SLOT_LANGUAGE;
	case CPRSENSOR_PROTOCOL_CMD_VOLUME:
		return CPRSENSOR_PROTOCOL_REPLY_SLOT_VOLUME;
	case CPRSENSOR_PROTOCOL_CMD_METRONOME:
		return CPRSENSOR_PROTOCOL_REPLY_SLOT_METRONOME;
	case CPRSENSOR_PROTOCOL_CMD_UTC_SETTING:
		return CPRSENSOR_PROTOCOL_REPLY_SLOT_UTC_SETTING;
	default:
		break;
	}

	return CPRSENSOR_PROTOCOL_REPLY_SLOT_MAX;
}

static void protcolMgrClearReplyPending(eCprsensorProtocolReplySlot replySlot)
{
	(void)replySlot;
	repRtosEnterCritical();
	if (gProtcolMgrTxCount > 0U) {
		gProtcolMgrTxHead++;
		if (gProtcolMgrTxHead >= gProtcolMgrTxQueueDepth) {
			gProtcolMgrTxHead = 0U;
		}
		gProtcolMgrTxCount--;
	}
	repRtosExitCritical();
}

static bool protcolMgrQueueReply(eIotManagerLinkId linkId, eCprsensorProtocolCmd cmd)
{
	uint8_t lTail;

	if ((linkId <= IOT_MANAGER_LINK_NONE) || (linkId >= IOT_MANAGER_LINK_MAX)) {
		return false;
	}

	repRtosEnterCritical();
	if (gProtcolMgrTxCount >= gProtcolMgrTxQueueDepth) {
		repRtosExitCritical();
		LOG_W(gProtcolMgrLogTag, "tx queue full link=%u cmd=0x%02X", (unsigned int)linkId, (unsigned int)cmd);
		return false;
	}

	lTail = (uint8_t)(gProtcolMgrTxHead + gProtcolMgrTxCount);
	if (lTail >= gProtcolMgrTxQueueDepth) {
		lTail = (uint8_t)(lTail - gProtcolMgrTxQueueDepth);
	}
	gProtcolMgrTxLinkQueue[lTail] = linkId;
	gProtcolMgrTxCmdQueue[lTail] = cmd;
	gProtcolMgrTxCount++;
	repRtosExitCritical();
	return true;
}

static bool protcolMgrPeekReply(eIotManagerLinkId *linkId, eCprsensorProtocolCmd *cmd)
{
	bool lHasReply;

	if ((linkId == NULL) || (cmd == NULL)) {
		return false;
	}

	lHasReply = false;
	repRtosEnterCritical();
	if (gProtcolMgrTxCount > 0U) {
		*linkId = gProtcolMgrTxLinkQueue[gProtcolMgrTxHead];
		*cmd = gProtcolMgrTxCmdQueue[gProtcolMgrTxHead];
		lHasReply = true;
	}
	repRtosExitCritical();
	return lHasReply;
}

static bool protcolMgrIsReplyHead(eIotManagerLinkId linkId, eCprsensorProtocolCmd cmd)
{
	bool lIsHead;

	lIsHead = false;
	repRtosEnterCritical();
	if ((gProtcolMgrTxCount > 0U) &&
		(gProtcolMgrTxLinkQueue[gProtcolMgrTxHead] == linkId) &&
		(gProtcolMgrTxCmdQueue[gProtcolMgrTxHead] == cmd)) {
		lIsHead = true;
	}
	repRtosExitCritical();
	return lIsHead;
}

static bool protcolMgrIsTxQueueFull(void)
{
	bool lFull;

	repRtosEnterCritical();
	lFull = gProtcolMgrTxCount >= gProtcolMgrTxQueueDepth;
	repRtosExitCritical();
	return lFull;
}

static void protcolMgrPromoteReplyLink(eIotManagerLinkId linkId)
{
	uint8_t lIndex;
	uint8_t lSlot;
	uint8_t lPrevSlot;
	eIotManagerLinkId lLinkId;
	eCprsensorProtocolCmd lCmd;

	repRtosEnterCritical();
	for (lIndex = 0U; lIndex < gProtcolMgrTxCount; ++lIndex) {
		lSlot = (uint8_t)(gProtcolMgrTxHead + lIndex);
		if (lSlot >= gProtcolMgrTxQueueDepth) {
			lSlot = (uint8_t)(lSlot - gProtcolMgrTxQueueDepth);
		}
		if (gProtcolMgrTxLinkQueue[lSlot] != linkId) {
			continue;
		}
		while (lIndex > 0U) {
			lPrevSlot = (lSlot == 0U) ? (uint8_t)(gProtcolMgrTxQueueDepth - 1U) : (uint8_t)(lSlot - 1U);
			lLinkId = gProtcolMgrTxLinkQueue[lPrevSlot];
			lCmd = gProtcolMgrTxCmdQueue[lPrevSlot];
			gProtcolMgrTxLinkQueue[lPrevSlot] = gProtcolMgrTxLinkQueue[lSlot];
			gProtcolMgrTxCmdQueue[lPrevSlot] = gProtcolMgrTxCmdQueue[lSlot];
			gProtcolMgrTxLinkQueue[lSlot] = lLinkId;
			gProtcolMgrTxCmdQueue[lSlot] = lCmd;
			lSlot = lPrevSlot;
			lIndex--;
		}
		break;
	}
	repRtosExitCritical();
}

static void protcolMgrRotateReply(void)
{
	eIotManagerLinkId lLinkId;
	eCprsensorProtocolCmd lCmd;
	uint8_t lTail;

	repRtosEnterCritical();
	if (gProtcolMgrTxCount > 1U) {
		lLinkId = gProtcolMgrTxLinkQueue[gProtcolMgrTxHead];
		lCmd = gProtcolMgrTxCmdQueue[gProtcolMgrTxHead];
		gProtcolMgrTxHead++;
		if (gProtcolMgrTxHead >= gProtcolMgrTxQueueDepth) {
			gProtcolMgrTxHead = 0U;
		}
		lTail = (uint8_t)(gProtcolMgrTxHead + gProtcolMgrTxCount - 1U);
		if (lTail >= gProtcolMgrTxQueueDepth) {
			lTail = (uint8_t)(lTail - gProtcolMgrTxQueueDepth);
		}
		gProtcolMgrTxLinkQueue[lTail] = lLinkId;
		gProtcolMgrTxCmdQueue[lTail] = lCmd;
	}
	repRtosExitCritical();
}

static bool protcolMgrGetExpectedPayloadLen(eCprsensorProtocolCmd cmd, uint16_t *payloadLen)
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

static bool protcolMgrShouldTryCipher(eCprsensorProtocolCmd cmd, uint16_t payloadLen)
{
	uint16_t lExpectedLen;

	if ((!gProtcolMgrCipherReady) || (payloadLen == 0U) ||
		((payloadLen % CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE) != 0U)) {
		return false;
	}

	if (!protcolMgrGetExpectedPayloadLen(cmd, &lExpectedLen)) {
		return false;
	}

	return payloadLen != lExpectedLen;
}

static eCprsensorProtocolStatus protcolMgrParseIncomingFrame(const uint8_t *buffer,
					      uint16_t length,
					      stCprsensorProtocolFrameView *frameView,
					      uint8_t *payloadBuffer,
					      uint16_t payloadBufferSize)
{
	stCprsensorProtocolCodecCfg lRawCodec;
	stCprsensorProtocolCodecCfg lCipherCodec;
	eCprsensorProtocolStatus lStatus;
	stCprsensorProtocolFrameView lCipherView;

	protcolMgrFillRawCodec(&lRawCodec);
	lStatus = cprsensorProtocolParseFrame(buffer,
					    length,
					    &lRawCodec,
					    payloadBuffer,
					    payloadBufferSize,
					    frameView);
	if (lStatus != CPRSENSOR_PROTOCOL_STATUS_OK) {
		return lStatus;
	}

	if (!protcolMgrShouldTryCipher(frameView->cmd, frameView->payloadLen)) {
		return CPRSENSOR_PROTOCOL_STATUS_OK;
	}

	protcolMgrFillCipherCodec(&lCipherCodec);
	lStatus = cprsensorProtocolParseFrame(buffer,
					    length,
					    &lCipherCodec,
					    payloadBuffer,
					    payloadBufferSize,
					    &lCipherView);
	if (lStatus == CPRSENSOR_PROTOCOL_STATUS_OK) {
		*frameView = lCipherView;
	}

	return CPRSENSOR_PROTOCOL_STATUS_OK;
}

static void protcolMgrSetTxLink(eIotManagerLinkId linkId)
{
	if ((linkId > IOT_MANAGER_LINK_NONE) && (linkId < IOT_MANAGER_LINK_MAX)) {
		gProtcolMgrTxLink = linkId;
	}
}

static void protcolMgrSetReplyFlag(eCprsensorProtocolCmd cmd)
{
	eCprsensorProtocolReplySlot lReplySlot;

	lReplySlot = protcolMgrGetReplySlot(cmd);
	if (lReplySlot < CPRSENSOR_PROTOCOL_REPLY_SLOT_MAX) {
		(void)protcolMgrQueueReply(gProtcolMgrTxLink, cmd);
	}
}

static void protcolMgrHandleFrame(eIotManagerLinkId linkId, const stCprsensorProtocolFrameView *frameView)
{
	if (frameView == NULL) {
		return;
	}

	protcolMgrSetTxLink(linkId);
	switch (frameView->cmd) {
	case CPRSENSOR_PROTOCOL_CMD_HANDSHAKE:
		if ((frameView->payload == NULL) || (frameView->payloadLen < CPRSENSOR_PROTOCOL_MAC_LEN)) {
			LOG_W(gProtcolMgrLogTag, "invalid handshake len=%u", (unsigned int)frameView->payloadLen);
			return;
		}
		(void)memcpy(gProtcolMgrHandshakePayload.mac, frameView->payload, CPRSENSOR_PROTOCOL_MAC_LEN);
		protcolMgrSetReplyFlag(frameView->cmd);
		break;
	case CPRSENSOR_PROTOCOL_CMD_HEARTBEAT:
	case CPRSENSOR_PROTOCOL_CMD_DISCONNECT:
	case CPRSENSOR_PROTOCOL_CMD_DEV_INFO:
	case CPRSENSOR_PROTOCOL_CMD_BLE_INFO:
	case CPRSENSOR_PROTOCOL_CMD_BATTERY:
		protcolMgrSetReplyFlag(frameView->cmd);
		break;
	case CPRSENSOR_PROTOCOL_CMD_WIFI_SETTING: {
		uint8_t lSsidLen;
		uint8_t lPwdLen;
		const uint8_t *lSsid;
		const uint8_t *lPwd;

		if ((frameView->payload == NULL) || (frameView->payloadLen < 2U)) {
			LOG_W(gProtcolMgrLogTag, "invalid wifi setting len=%u", (unsigned int)frameView->payloadLen);
			return;
		}

		lSsidLen = frameView->payload[0];
		if ((lSsidLen > WIRELESS_WIFI_SSID_MAX_LEN) ||
			(frameView->payloadLen < (uint16_t)(2U + lSsidLen))) {
			LOG_W(gProtcolMgrLogTag, "invalid wifi ssid len=%u", (unsigned int)lSsidLen);
			return;
		}

		lSsid = &frameView->payload[1];
		lPwdLen = frameView->payload[1U + lSsidLen];
		if ((lPwdLen > WIRELESS_WIFI_PASSWORD_MAX_LEN) ||
			(frameView->payloadLen != (uint16_t)(2U + lSsidLen + lPwdLen))) {
			LOG_W(gProtcolMgrLogTag, "invalid wifi pwd len=%u", (unsigned int)lPwdLen);
			return;
		}

		lPwd = &frameView->payload[2U + lSsidLen];
		if (!wirelessSetWifiCredentials(lSsid, lSsidLen, lPwd, lPwdLen)) {
			LOG_W(gProtcolMgrLogTag, "apply wifi setting failed");
			return;
		}

		gProtcolMgrWifiSsidLen = lSsidLen;
		gProtcolMgrWifiPwdLen = lPwdLen;
		if (lSsidLen > 0U) {
			(void)memcpy(gProtcolMgrWifiSsid, lSsid, lSsidLen);
		}
		if (lPwdLen > 0U) {
			(void)memcpy(gProtcolMgrWifiPwd, lPwd, lPwdLen);
		}
		protcolMgrSetReplyFlag(frameView->cmd);
		break;
	}
	case CPRSENSOR_PROTOCOL_CMD_COMM_SETTING:
		if ((frameView->payload == NULL) || (frameView->payloadLen < sizeof(gProtcolMgrCommSettingPayload))) {
			LOG_W(gProtcolMgrLogTag, "invalid comm setting len=%u", (unsigned int)frameView->payloadLen);
			return;
		}
		(void)memcpy(&gProtcolMgrCommSettingPayload, frameView->payload, sizeof(gProtcolMgrCommSettingPayload));
		break;
	case CPRSENSOR_PROTOCOL_CMD_TIME_SYNC:
		if ((frameView->payload == NULL) || (frameView->payloadLen < sizeof(gProtcolMgrTimeSyncPayload))) {
			LOG_W(gProtcolMgrLogTag, "invalid time sync len=%u", (unsigned int)frameView->payloadLen);
			return;
		}
		(void)memcpy(&gProtcolMgrTimeSyncPayload, frameView->payload, sizeof(gProtcolMgrTimeSyncPayload));
		break;
	case CPRSENSOR_PROTOCOL_CMD_LANGUAGE:
		if ((frameView->payload == NULL) || (frameView->payloadLen < sizeof(gProtcolMgrLanguagePayload))) {
			LOG_W(gProtcolMgrLogTag, "invalid language len=%u", (unsigned int)frameView->payloadLen);
			return;
		}
		(void)memcpy(&gProtcolMgrLanguagePayload, frameView->payload, sizeof(gProtcolMgrLanguagePayload));
		protcolMgrSetReplyFlag(frameView->cmd);
		break;
	case CPRSENSOR_PROTOCOL_CMD_VOLUME:
		if ((frameView->payload == NULL) || (frameView->payloadLen < sizeof(gProtcolMgrVolumePayload))) {
			LOG_W(gProtcolMgrLogTag, "invalid volume len=%u", (unsigned int)frameView->payloadLen);
			return;
		}
		(void)memcpy(&gProtcolMgrVolumePayload, frameView->payload, sizeof(gProtcolMgrVolumePayload));
		protcolMgrSetReplyFlag(frameView->cmd);
		break;
	case CPRSENSOR_PROTOCOL_CMD_METRONOME:
		if ((frameView->payload == NULL) || (frameView->payloadLen < sizeof(gProtcolMgrMetronomePayload))) {
			LOG_W(gProtcolMgrLogTag, "invalid metronome len=%u", (unsigned int)frameView->payloadLen);
			return;
		}
		(void)memcpy(&gProtcolMgrMetronomePayload, frameView->payload, sizeof(gProtcolMgrMetronomePayload));
		protcolMgrSetReplyFlag(frameView->cmd);
		break;
	case CPRSENSOR_PROTOCOL_CMD_UTC_SETTING:
		if ((frameView->payload == NULL) || (frameView->payloadLen < sizeof(gProtcolMgrUtcSettingPayload))) {
			LOG_W(gProtcolMgrLogTag, "invalid utc len=%u", (unsigned int)frameView->payloadLen);
			return;
		}
		(void)memcpy(&gProtcolMgrUtcSettingPayload, frameView->payload, sizeof(gProtcolMgrUtcSettingPayload));
		protcolMgrSetReplyFlag(frameView->cmd);
		break;
	default:
		LOG_W(gProtcolMgrLogTag,
		      "ignore cmd=0x%02X payloadLen=%u",
		      (unsigned int)frameView->cmd,
		      (unsigned int)frameView->payloadLen);
		break;
	}
}

bool protcolMgrPushReceivedData(eIotManagerLinkId linkId, const uint8_t *buffer, uint16_t length)
{
	uint8_t lSlot;
	uint8_t lTail;

	protcolMgrEnsureInitialized();
	if ((linkId <= IOT_MANAGER_LINK_NONE) || (linkId >= IOT_MANAGER_LINK_MAX) ||
		(buffer == NULL) || (length == 0U) || (length > gProtcolMgrRxBufferSize)) {
		return false;
	}

	repRtosEnterCritical();
	if (gProtcolMgrRxCount >= gProtcolMgrRxQueueDepth) {
		repRtosExitCritical();
		LOG_W(gProtcolMgrLogTag, "rx queue full link=%u len=%u", (unsigned int)linkId, (unsigned int)length);
		return false;
	}

	lTail = (uint8_t)(gProtcolMgrRxHead + gProtcolMgrRxCount);
	if (lTail >= gProtcolMgrRxQueueDepth) {
		lTail = (uint8_t)(lTail - gProtcolMgrRxQueueDepth);
	}
	lSlot = lTail;
	(void)memcpy(gProtcolMgrRxBuffer[lSlot], buffer, length);
	gProtcolMgrRxLength[lSlot] = length;
	gProtcolMgrRxLink[lSlot] = linkId;
	gProtcolMgrRxCount++;
	repRtosExitCritical();
	return true;
}

static bool protcolMgrBuildAndSendReply(eIotManagerLinkId linkId, eCprsensorProtocolCmd cmd, const uint8_t *payload, uint16_t payloadLen)
{
	stCprsensorProtocolCodecCfg lCodecCfg;
	eCprsensorProtocolStatus lStatus;
	uint8_t lFrameBuffer[128];
	uint16_t lFrameLen;

	protcolMgrFillRawCodec(&lCodecCfg);
	lFrameLen = 0U;
	lStatus = cprsensorProtocolPackFrame(cmd,
					   payload,
					   payloadLen,
					   &lCodecCfg,
					   lFrameBuffer,
					   gProtcolMgrTxFrameBufferSize,
					   &lFrameLen);
	if (lStatus != CPRSENSOR_PROTOCOL_STATUS_OK) {
		LOG_W(gProtcolMgrLogTag,
		      "pack reply failed cmd=0x%02X status=%d",
		      (unsigned int)cmd,
		      (int)lStatus);
		return false;
	}

	return iotManagerSendByLink(linkId, lFrameBuffer, lFrameLen);
}

static void protcolMgrFlushHandshakeReply(eIotManagerLinkId linkId)
{
	char lMacText[32];

	if (!wirelessGetMacAddress(lMacText, (uint16_t)sizeof(lMacText))) {
		return;
	}

	if (!protcolMgrParseMacString(lMacText, gProtcolMgrHandshakePayload.mac, CPRSENSOR_PROTOCOL_MAC_LEN)) {
		return;
	}

	if (protcolMgrBuildAndSendReply(linkId,
					 CPRSENSOR_PROTOCOL_CMD_HANDSHAKE,
					 gProtcolMgrHandshakePayload.mac,
					 CPRSENSOR_PROTOCOL_MAC_LEN)) {
		protcolMgrClearReplyPending(CPRSENSOR_PROTOCOL_REPLY_SLOT_HANDSHAKE);
	}
}

static void protcolMgrFlushHeartbeatReply(eIotManagerLinkId linkId)
{
	if (protcolMgrBuildAndSendReply(linkId, CPRSENSOR_PROTOCOL_CMD_HEARTBEAT, NULL, 0U)) {
		protcolMgrClearReplyPending(CPRSENSOR_PROTOCOL_REPLY_SLOT_HEARTBEAT);
	}
}

static void protcolMgrFlushDisconnectReply(eIotManagerLinkId linkId)
{
	if (protcolMgrBuildAndSendReply(linkId, CPRSENSOR_PROTOCOL_CMD_DISCONNECT, NULL, 0U)) {
		protcolMgrClearReplyPending(CPRSENSOR_PROTOCOL_REPLY_SLOT_DISCONNECT);
	}
}

static void protcolMgrFlushDevInfoReply(eIotManagerLinkId linkId)
{
	stCprsensorProtocolDevInfoReplyPayload lDevInfoPayload;

	(void)memset(&lDevInfoPayload, 0, sizeof(lDevInfoPayload));
	lDevInfoPayload.deviceType = gProtcolMgrDeviceType;
	(void)memcpy(lDevInfoPayload.deviceSn, "CPRSENSOR0001", CPRSENSOR_PROTOCOL_DEVICE_SN_LEN);
	lDevInfoPayload.protocolOrFlag = gProtcolMgrProtocolFlag;
	lDevInfoPayload.swVersion = gProtcolMgrSwVersion;
	lDevInfoPayload.swSubVersion = gProtcolMgrSwSubVersion;
	lDevInfoPayload.swBuildVersion = gProtcolMgrSwBuildVersion;
	if (protcolMgrBuildAndSendReply(linkId,
					 CPRSENSOR_PROTOCOL_CMD_DEV_INFO,
					 (const uint8_t *)&lDevInfoPayload,
					 (uint16_t)sizeof(lDevInfoPayload))) {
		protcolMgrClearReplyPending(CPRSENSOR_PROTOCOL_REPLY_SLOT_DEV_INFO);
	}
}

static void protcolMgrFlushBleInfoReply(eIotManagerLinkId linkId)
{
	stCprsensorProtocolBleInfoReplyPayload lBleInfoPayload;
	const char *lBleVersion;

	(void)memset(&lBleInfoPayload, 0, sizeof(lBleInfoPayload));
	lBleVersion = "CprSensorTest-BLE-Bridge";
	(void)memcpy(lBleInfoPayload.bleVersion, lBleVersion, (uint16_t)strlen(lBleVersion));
	if (protcolMgrBuildAndSendReply(linkId,
					 CPRSENSOR_PROTOCOL_CMD_BLE_INFO,
					 (const uint8_t *)&lBleInfoPayload,
					 (uint16_t)sizeof(lBleInfoPayload))) {
		protcolMgrClearReplyPending(CPRSENSOR_PROTOCOL_REPLY_SLOT_BLE_INFO);
	}
}

static void protcolMgrFlushWifiSettingReply(eIotManagerLinkId linkId)
{
	uint8_t lPayload[2U + WIRELESS_WIFI_SSID_MAX_LEN + WIRELESS_WIFI_PASSWORD_MAX_LEN];
	uint16_t lOffset;

	lOffset = 0U;
	lPayload[lOffset++] = gProtcolMgrWifiSsidLen;
	if (gProtcolMgrWifiSsidLen > 0U) {
		(void)memcpy(&lPayload[lOffset], gProtcolMgrWifiSsid, gProtcolMgrWifiSsidLen);
		lOffset = (uint16_t)(lOffset + gProtcolMgrWifiSsidLen);
	}
	lPayload[lOffset++] = gProtcolMgrWifiPwdLen;
	if (gProtcolMgrWifiPwdLen > 0U) {
		(void)memcpy(&lPayload[lOffset], gProtcolMgrWifiPwd, gProtcolMgrWifiPwdLen);
		lOffset = (uint16_t)(lOffset + gProtcolMgrWifiPwdLen);
	}

	if (protcolMgrBuildAndSendReply(linkId,
					 CPRSENSOR_PROTOCOL_CMD_WIFI_SETTING,
					 lPayload,
					 lOffset)) {
		protcolMgrClearReplyPending(CPRSENSOR_PROTOCOL_REPLY_SLOT_WIFI_SETTING);
	}
}

static void protcolMgrFlushBatteryReply(eIotManagerLinkId linkId)
{
	stCprsensorProtocolBatteryReplyPayload lBatteryPayload;

	(void)memset(&lBatteryPayload, 0, sizeof(lBatteryPayload));
	lBatteryPayload.batPercent = gProtcolMgrDefaultBatteryPercent;
	cprsensorProtocolWriteU16Be(lBatteryPayload.batMvBe, gProtcolMgrDefaultBatteryMv);
	lBatteryPayload.chargeState = gProtcolMgrDefaultChargeState;
	if (protcolMgrBuildAndSendReply(linkId,
					 CPRSENSOR_PROTOCOL_CMD_BATTERY,
					 (const uint8_t *)&lBatteryPayload,
					 (uint16_t)sizeof(lBatteryPayload))) {
		protcolMgrClearReplyPending(CPRSENSOR_PROTOCOL_REPLY_SLOT_BATTERY);
	}
}

static void protcolMgrFlushLanguageReply(eIotManagerLinkId linkId)
{
	if (protcolMgrBuildAndSendReply(linkId,
					 CPRSENSOR_PROTOCOL_CMD_LANGUAGE,
					 (const uint8_t *)&gProtcolMgrLanguagePayload,
					 (uint16_t)sizeof(gProtcolMgrLanguagePayload))) {
		protcolMgrClearReplyPending(CPRSENSOR_PROTOCOL_REPLY_SLOT_LANGUAGE);
	}
}

static void protcolMgrFlushVolumeReply(eIotManagerLinkId linkId)
{
	if (protcolMgrBuildAndSendReply(linkId,
					 CPRSENSOR_PROTOCOL_CMD_VOLUME,
					 (const uint8_t *)&gProtcolMgrVolumePayload,
					 (uint16_t)sizeof(gProtcolMgrVolumePayload))) {
		protcolMgrClearReplyPending(CPRSENSOR_PROTOCOL_REPLY_SLOT_VOLUME);
	}
}

static void protcolMgrFlushMetronomeReply(eIotManagerLinkId linkId)
{
	if (protcolMgrBuildAndSendReply(linkId,
					 CPRSENSOR_PROTOCOL_CMD_METRONOME,
					 (const uint8_t *)&gProtcolMgrMetronomePayload,
					 (uint16_t)sizeof(gProtcolMgrMetronomePayload))) {
		protcolMgrClearReplyPending(CPRSENSOR_PROTOCOL_REPLY_SLOT_METRONOME);
	}
}

static void protcolMgrFlushUtcSettingReply(eIotManagerLinkId linkId)
{
	if (protcolMgrBuildAndSendReply(linkId,
					 CPRSENSOR_PROTOCOL_CMD_UTC_SETTING,
					 (const uint8_t *)&gProtcolMgrUtcSettingPayload,
					 (uint16_t)sizeof(gProtcolMgrUtcSettingPayload))) {
		protcolMgrClearReplyPending(CPRSENSOR_PROTOCOL_REPLY_SLOT_UTC_SETTING);
	}
}

static void protcolMgrFlushPendingReplies(void)
{
	eIotManagerLinkId lLinkId;
	eCprsensorProtocolCmd lCmd;
	eCprsensorProtocolReplySlot lReplySlot;
	uint32_t lIndex;
	uint8_t lAttempts;
	uint8_t lAttemptMax;
	bool lStillHead;

	protcolMgrEnsureInitialized();
	protcolMgrPromoteReplyLink(IOT_MANAGER_LINK_BLE);

	lAttempts = 0U;
	lAttemptMax = gProtcolMgrTxQueueDepth;
	while ((lAttempts < lAttemptMax) && protcolMgrPeekReply(&lLinkId, &lCmd)) {
		lAttempts++;
		if ((lLinkId <= IOT_MANAGER_LINK_NONE) || (lLinkId >= IOT_MANAGER_LINK_MAX)) {
			protcolMgrClearReplyPending(CPRSENSOR_PROTOCOL_REPLY_SLOT_MAX);
			continue;
		}

		lReplySlot = protcolMgrGetReplySlot(lCmd);
		if (lReplySlot >= CPRSENSOR_PROTOCOL_REPLY_SLOT_MAX) {
			protcolMgrClearReplyPending(CPRSENSOR_PROTOCOL_REPLY_SLOT_MAX);
			continue;
		}

		for (lIndex = 0U; lIndex < (uint32_t)(sizeof(gProtcolMgrReplyFlushTable) / sizeof(gProtcolMgrReplyFlushTable[0])); ++lIndex) {
			if (gProtcolMgrReplyFlushTable[lIndex].replySlot != lReplySlot) {
				continue;
			}

			gProtcolMgrReplyFlushTable[lIndex].flushReply(lLinkId);
			lStillHead = protcolMgrIsReplyHead(lLinkId, lCmd);
			if (lStillHead) {
				protcolMgrRotateReply();
			}
			return;
		}

		protcolMgrClearReplyPending(CPRSENSOR_PROTOCOL_REPLY_SLOT_MAX);
	}
}

static void protcolMgrConsumeReceivedData(void)
{
	uint8_t lRxBuffer[640];
	uint8_t lPayloadBuffer[640];
	uint16_t lRxLength;
	eIotManagerLinkId lLinkId;
	uint8_t lSlot;
	stCprsensorProtocolFrameView lFrameView;
	eCprsensorProtocolStatus lStatus;

	protcolMgrEnsureInitialized();

	repRtosEnterCritical();
	if (gProtcolMgrRxCount == 0U) {
		repRtosExitCritical();
		return;
	}

	lSlot = gProtcolMgrRxHead;
	lRxLength = gProtcolMgrRxLength[lSlot];
	lLinkId = gProtcolMgrRxLink[lSlot];
	(void)memcpy(lRxBuffer, gProtcolMgrRxBuffer[lSlot], lRxLength);
	gProtcolMgrRxHead++;
	if (gProtcolMgrRxHead >= gProtcolMgrRxQueueDepth) {
		gProtcolMgrRxHead = 0U;
	}
	gProtcolMgrRxCount--;
	repRtosExitCritical();

	lStatus = protcolMgrParseIncomingFrame(lRxBuffer,
					      lRxLength,
					      &lFrameView,
					      lPayloadBuffer,
					      (uint16_t)sizeof(lPayloadBuffer));
	if (lStatus != CPRSENSOR_PROTOCOL_STATUS_OK) {
		LOG_W(gProtcolMgrLogTag,
		      "parse failed status=%d len=%u",
		      (int)lStatus,
		      (unsigned int)lRxLength);
		return;
	}

	protcolMgrHandleFrame(lLinkId, &lFrameView);
}

void protcolMgrProcess(void)
{
	protcolMgrFlushPendingReplies();
	if (protcolMgrIsTxQueueFull()) {
		return;
	}
	protcolMgrConsumeReceivedData();
	protcolMgrFlushPendingReplies();
}
/**************************End of file********************************/
