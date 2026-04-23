/***********************************************************************************
* @file     : wirelss_mgr.c
* @brief    : Wireless manager implementation.
* @details  : Configures FC41D BLE defaults and bridges project code to the
*             FC41D background service.
* @author   : GitHub Copilot
* @date     : 2026-04-21
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "wirelss_mgr.h"

#include <string.h>

#include "../comm/update_protocol.h"
#include "../../port/frameparser_port.h"
#include "../../../rep/tools/aes/aes.h"
#include "../../../rep/tools/md5/md5.h"
#include "../../../rep/comm/frameparser/framepareser.h"
#include "../../../rep/module/fc41d/fc41d.h"
#include "../../../rep/service/log/log.h"

#define WIRELESS_MGR_LOG_TAG               "Wireless"
#define WIRELESS_MGR_DEVICE                FC41D_DEV0
#define WIRELESS_MGR_RETRY_INTERVAL_MS     1000U
#define WIRELESS_MGR_BLE_INIT_MODE         2U

enum {
    WIRELESS_MGR_HANDSHAKE_CMD = 0x01U,
    WIRELESS_MGR_TIME_SYNC_CMD = 0x33U,
    WIRELESS_MGR_HANDSHAKE_PAYLOAD_SIZE = 16U,
    WIRELESS_MGR_RX_PUMP_CHUNK_SIZE = 64U,
    WIRELESS_MGR_MAC_BYTE_SIZE = 6U,
};

static uint32_t gWirelessMgrLastWarnTick;
static eWirelessMgrHandshakeState gWirelessMgrHandshakeState;
static bool gWirelessMgrLastBleConnected;
static bool gWirelessMgrHasLoggedMac;
static stFrmPsr gWirelessMgrParser;
static uint8_t gWirelessMgrParserStreamBuf[FRAMEPARSER_PORT_UPDATE_MAX_FRAME_SIZE];
static uint8_t gWirelessMgrParserFrameBuf[FRAMEPARSER_PORT_UPDATE_MAX_FRAME_SIZE];

static bool wirelessMgrCopyText(char *buffer, uint16_t bufferSize, const char *text);
static bool wirelessMgrInitParser(void);
static void wirelessMgrResetHandshakeContext(void);
static void wirelessMgrLogLinkState(const stFc41dState *state);
static void wirelessMgrUpdateHandshakeState(const stFc41dState *state, bool wasBleConnected);
static eFc41dRawMatchSta wirelessMgrMatchRawFrame(void *userData, const uint8_t *buf, uint16_t availLen, uint16_t *frameLen);
static void wirelessMgrPumpParserRx(void);
static void wirelessMgrFeedParser(const uint8_t *buffer, uint16_t length);
static void wirelessMgrDrainParser(void);
static void wirelessMgrHandleFrame(const stFrmPsrPkt *pkt);
static bool wirelessMgrValidateHandshakeFrame(const stFrmPsrPkt *pkt);
static bool wirelessMgrBuildHandshakePayload(uint8_t *payload, uint16_t payloadSize);
static bool wirelessMgrSendHandshakeAck(void);
static bool wirelessMgrSendProtocolFrame(uint8_t cmd, const uint8_t *payload, uint16_t payloadLength);
static bool wirelessMgrIsUpdateProtocolCmd(uint8_t cmd);
static bool wirelessMgrLoadMacCrypto(uint8_t *macBytes, uint16_t macByteSize, uint8_t *aesKey, uint16_t aesKeySize);
static bool wirelessMgrParseMacText(const char *macText, uint8_t *macBytes, uint16_t macByteSize);
static int8_t wirelessMgrHexToNibble(char value);

void wirelessMgrReset(void)
{
    fc41dStop(WIRELESS_MGR_DEVICE);
    gWirelessMgrLastWarnTick = 0U;
    gWirelessMgrLastBleConnected = false;
    gWirelessMgrHasLoggedMac = false;
    wirelessMgrResetHandshakeContext();
}

bool wirelessMgrInit(void)
{
    stFc41dCfg lCfg;
    stFc41dBleCfg lBleCfg;
    eFc41dStatus lStatus;

    wirelessMgrReset();

    lStatus = fc41dGetDefCfg(WIRELESS_MGR_DEVICE, &lCfg);
    if (lStatus != FC41D_STATUS_OK) {
        LOG_E(WIRELESS_MGR_LOG_TAG, "fc41d default cfg failed, status=%d", (int)lStatus);
        return false;
    }

    lStatus = fc41dGetDefBleCfg(WIRELESS_MGR_DEVICE, &lBleCfg);
    if (lStatus != FC41D_STATUS_OK) {
        LOG_E(WIRELESS_MGR_LOG_TAG, "fc41d default ble cfg failed, status=%d", (int)lStatus);
        return false;
    }

    lBleCfg.initMode = WIRELESS_MGR_BLE_INIT_MODE;
    if (!wirelessMgrCopyText(lBleCfg.name, (uint16_t)sizeof(lBleCfg.name), WIRELESS_MGR_BLE_NAME) ||
        !wirelessMgrCopyText(lBleCfg.serviceUuid, (uint16_t)sizeof(lBleCfg.serviceUuid), WIRELESS_MGR_BLE_SERVICE_UUID) ||
        !wirelessMgrCopyText(lBleCfg.rxCharUuid, (uint16_t)sizeof(lBleCfg.rxCharUuid), WIRELESS_MGR_BLE_CHAR_UUID_RX) ||
        !wirelessMgrCopyText(lBleCfg.txCharUuid, (uint16_t)sizeof(lBleCfg.txCharUuid), WIRELESS_MGR_BLE_CHAR_UUID_TX)) {
        LOG_E(WIRELESS_MGR_LOG_TAG, "fc41d ble cfg text copy failed");
        return false;
    }

    lStatus = fc41dSetCfg(WIRELESS_MGR_DEVICE, &lCfg);
    if (lStatus != FC41D_STATUS_OK) {
        LOG_E(WIRELESS_MGR_LOG_TAG, "fc41d cfg set failed, status=%d", (int)lStatus);
        return false;
    }

    lStatus = fc41dSetBleCfg(WIRELESS_MGR_DEVICE, &lBleCfg);
    if (lStatus != FC41D_STATUS_OK) {
        LOG_E(WIRELESS_MGR_LOG_TAG, "fc41d ble cfg set failed, status=%d", (int)lStatus);
        return false;
    }

    lStatus = fc41dInit(WIRELESS_MGR_DEVICE);
    if (lStatus != FC41D_STATUS_OK) {
        LOG_E(WIRELESS_MGR_LOG_TAG, "fc41d init failed, status=%d", (int)lStatus);
        return false;
    }

    if (!wirelessMgrInitParser()) {
        LOG_E(WIRELESS_MGR_LOG_TAG, "wireless frame parser init failed");
        return false;
    }

    lStatus = fc41dSetRawMatcher(WIRELESS_MGR_DEVICE, wirelessMgrMatchRawFrame, NULL);
    if (lStatus != FC41D_STATUS_OK) {
        LOG_E(WIRELESS_MGR_LOG_TAG, "fc41d raw matcher set failed, status=%d", (int)lStatus);
        return false;
    }

    lStatus = fc41dStart(WIRELESS_MGR_DEVICE, FC41D_ROLE_BLE_PERIPHERAL);
    if (lStatus != FC41D_STATUS_OK) {
        LOG_E(WIRELESS_MGR_LOG_TAG, "fc41d start failed, status=%d", (int)lStatus);
        return false;
    }

    LOG_I(WIRELESS_MGR_LOG_TAG, "fc41d wireless manager configured");
    return true;
}

void wirelessMgrProcess(uint32_t nowTickMs)
{
    const stFc41dState *lpState;
    eFc41dStatus lStatus;
    bool lWasBleConnected;

    lStatus = fc41dProcess(WIRELESS_MGR_DEVICE, nowTickMs);
    lpState = fc41dGetState(WIRELESS_MGR_DEVICE);
    if ((lStatus != FC41D_STATUS_OK) && ((nowTickMs - gWirelessMgrLastWarnTick) >= WIRELESS_MGR_RETRY_INTERVAL_MS)) {
        gWirelessMgrLastWarnTick = nowTickMs;
        LOG_W(WIRELESS_MGR_LOG_TAG, "fc41d process failed, status=%d, run=%d", (int)lStatus, (lpState == NULL) ? -1 : (int)lpState->runState);
    }

    lWasBleConnected = gWirelessMgrLastBleConnected;
    wirelessMgrLogLinkState(lpState);
    if ((lpState != NULL) && lpState->isReady && !wirelessMgrIsHandshakeReady()) {
        wirelessMgrPumpParserRx();
    }
    wirelessMgrUpdateHandshakeState(lpState, lWasBleConnected);
    if ((lpState != NULL) && lpState->isReady && lpState->isBleConnected && !wirelessMgrIsHandshakeReady()) {
        wirelessMgrDrainParser();
    }
}

bool wirelessMgrIsReady(void)
{
    const stFc41dState *lpState;

    lpState = fc41dGetState(WIRELESS_MGR_DEVICE);
    return (lpState != NULL) && lpState->isReady;
}

eWirelessMgrHandshakeState wirelessMgrGetHandshakeState(void)
{
    return gWirelessMgrHandshakeState;
}

bool wirelessMgrIsHandshakeReady(void)
{
    return gWirelessMgrHandshakeState == WIRELESS_MGR_HANDSHAKE_READY;
}

uint16_t wirelessMgrGetRxLength(void)
{
    if (!wirelessMgrIsHandshakeReady()) {
        return 0U;
    }

    return fc41dGetRxLength(WIRELESS_MGR_DEVICE);
}

uint16_t wirelessMgrReadRxData(uint8_t *buffer, uint16_t bufferSize)
{
    if ((buffer == NULL) || (bufferSize == 0U) || !wirelessMgrIsHandshakeReady()) {
        return 0U;
    }

    return fc41dReadData(WIRELESS_MGR_DEVICE, buffer, bufferSize);
}

bool wirelessMgrWriteData(const uint8_t *buffer, uint16_t length)
{
    if (!wirelessMgrIsHandshakeReady()) {
        return false;
    }

    return fc41dWriteData(WIRELESS_MGR_DEVICE, buffer, length) == FC41D_STATUS_OK;
}

bool wirelessMgrGetMacAddress(char *buffer, uint16_t bufferSize)
{
    return fc41dGetCachedMac(WIRELESS_MGR_DEVICE, buffer, bufferSize);
}

static bool wirelessMgrCopyText(char *buffer, uint16_t bufferSize, const char *text)
{
    uint16_t lLength;

    if ((buffer == NULL) || (bufferSize == 0U) || (text == NULL)) {
        return false;
    }

    lLength = (uint16_t)strlen(text);
    if (lLength >= bufferSize) {
        return false;
    }

    (void)memcpy(buffer, text, lLength + 1U);
    return true;
}

static bool wirelessMgrInitParser(void)
{
    stFrmPsrCfg lParserCfg;

    (void)memset(&gWirelessMgrParser, 0, sizeof(gWirelessMgrParser));
    (void)memset(&lParserCfg, 0, sizeof(lParserCfg));
    lParserCfg.protocolId = (uint32_t)FRAMEPARSER_PORT_PROTOCOL_UPDATE;
    lParserCfg.streamBuf = gWirelessMgrParserStreamBuf;
    lParserCfg.streamBufSize = sizeof(gWirelessMgrParserStreamBuf);
    lParserCfg.frameBuf = gWirelessMgrParserFrameBuf;
    lParserCfg.frameBufSize = sizeof(gWirelessMgrParserFrameBuf);
    return frmPsrInit(&gWirelessMgrParser, &lParserCfg) == FRM_PSR_OK;
}

static void wirelessMgrResetHandshakeContext(void)
{
    gWirelessMgrHandshakeState = WIRELESS_MGR_HANDSHAKE_IDLE;
    (void)wirelessMgrInitParser();
}

static void wirelessMgrLogLinkState(const stFc41dState *state)
{
    bool lIsBleConnected;

    lIsBleConnected = (state != NULL) && state->isBleConnected;
    if (lIsBleConnected != gWirelessMgrLastBleConnected) {
        gWirelessMgrLastBleConnected = lIsBleConnected;
    }

    if (!gWirelessMgrHasLoggedMac && (state != NULL) && state->hasMacAddress && (state->macAddress[0] != '\0')) {
        gWirelessMgrHasLoggedMac = true;
    }
}

static void wirelessMgrUpdateHandshakeState(const stFc41dState *state, bool wasBleConnected)
{
    if ((state == NULL) || !state->isReady) {
        wirelessMgrResetHandshakeContext();
        return;
    }

    if (!state->isBleConnected) {
        if (wasBleConnected || (gWirelessMgrHandshakeState != WIRELESS_MGR_HANDSHAKE_IDLE)) {
            wirelessMgrResetHandshakeContext();
        }
        return;
    }

    if (gWirelessMgrHandshakeState == WIRELESS_MGR_HANDSHAKE_IDLE) {
        gWirelessMgrHandshakeState = WIRELESS_MGR_HANDSHAKE_WAIT_PEER;
    }
}

static eFc41dRawMatchSta wirelessMgrMatchRawFrame(void *userData, const uint8_t *buf, uint16_t availLen, uint16_t *frameLen)
{
    uint16_t lFrameLength;

    (void)userData;
    if ((buf == NULL) || (frameLen == NULL) || (availLen == 0U)) {
        return FC41D_RAW_MATCH_NONE;
    }

    if (buf[0] != UPDATE_PROTOCOL_SYNC0) {
        return FC41D_RAW_MATCH_NONE;
    }
    if (availLen < 2U) {
        return FC41D_RAW_MATCH_NEED_MORE;
    }
    if (buf[1] != UPDATE_PROTOCOL_SYNC1) {
        return FC41D_RAW_MATCH_NONE;
    }
    if (availLen < 3U) {
        return FC41D_RAW_MATCH_NEED_MORE;
    }
    if (buf[2] != UPDATE_PROTOCOL_SYNC2) {
        return FC41D_RAW_MATCH_NONE;
    }
    if (availLen < UPDATE_PROTOCOL_FRAME_HEADER_SIZE) {
        return FC41D_RAW_MATCH_NEED_MORE;
    }

    lFrameLength = (uint16_t)updateProtocolGetPktLen(buf, UPDATE_PROTOCOL_FRAME_HEADER_SIZE, availLen, NULL);
    if ((lFrameLength < (UPDATE_PROTOCOL_FRAME_HEADER_SIZE + UPDATE_PROTOCOL_FRAME_CRC16_SIZE)) ||
        (lFrameLength > FRAMEPARSER_PORT_UPDATE_MAX_FRAME_SIZE)) {
        return FC41D_RAW_MATCH_NONE;
    }
    if (availLen < lFrameLength) {
        return FC41D_RAW_MATCH_NEED_MORE;
    }

    *frameLen = lFrameLength;
    return FC41D_RAW_MATCH_OK;
}

static void wirelessMgrPumpParserRx(void)
{
    uint8_t lBuffer[WIRELESS_MGR_RX_PUMP_CHUNK_SIZE];
    uint16_t lReadLength;
    uint16_t lAvailLength;

    lAvailLength = fc41dGetRxLength(WIRELESS_MGR_DEVICE);
    while (lAvailLength > 0U) {
        lReadLength = lAvailLength;
        if (lReadLength > sizeof(lBuffer)) {
            lReadLength = sizeof(lBuffer);
        }

        lReadLength = fc41dReadData(WIRELESS_MGR_DEVICE, lBuffer, lReadLength);
        if (lReadLength == 0U) {
            break;
        }

        wirelessMgrFeedParser(lBuffer, lReadLength);
        lAvailLength = fc41dGetRxLength(WIRELESS_MGR_DEVICE);
    }
}

static void wirelessMgrFeedParser(const uint8_t *buffer, uint16_t length)
{
    eFrmPsrSta lParserSta;

    if ((buffer == NULL) || (length == 0U)) {
        return;
    }

    lParserSta = frmPsrFeed(&gWirelessMgrParser, buffer, length);
    if (lParserSta == FRM_PSR_OK) {
        return;
    }

    LOG_W(WIRELESS_MGR_LOG_TAG, "wireless frame parser feed failed: %d", (int)lParserSta);
    (void)wirelessMgrInitParser();
}

static void wirelessMgrDrainParser(void)
{
    eFrmPsrSta lParserSta;
    const stFrmPsrPkt *lpPkt;

    while (true) {
        lParserSta = frmPsrProcess(&gWirelessMgrParser);
        if ((lParserSta == FRM_PSR_OK) && gWirelessMgrParser.hasReadyPkt) {
            lpPkt = frmPsrRelease(&gWirelessMgrParser);
            if (lpPkt == NULL) {
                break;
            }

            wirelessMgrHandleFrame(lpPkt);
            continue;
        }

        if ((lParserSta == FRM_PSR_CRC_FAIL) ||
            (lParserSta == FRM_PSR_LEN_INVALID) ||
            (lParserSta == FRM_PSR_HEAD_INVALID) ||
            (lParserSta == FRM_PSR_OUT_BUF_SMALL)) {
            LOG_W(WIRELESS_MGR_LOG_TAG, "wireless frame parser process failed: %d", (int)lParserSta);
            (void)wirelessMgrInitParser();
        }
        break;
    }
}

static void wirelessMgrHandleFrame(const stFrmPsrPkt *pkt)
{
    stUpdateProtocolReply lReply;
    uint8_t lCmd;

    if ((pkt == NULL) || (pkt->cmdBuf == NULL) || (pkt->cmdFieldLen == 0U)) {
        return;
    }

    lCmd = pkt->cmdBuf[0];
    if (lCmd == WIRELESS_MGR_TIME_SYNC_CMD) {
        return;
    }

    if (lCmd == WIRELESS_MGR_HANDSHAKE_CMD) {
        if (!wirelessMgrValidateHandshakeFrame(pkt)) {
            LOG_W(WIRELESS_MGR_LOG_TAG, "upper host handshake validation failed");
            return;
        }

        if (!wirelessMgrSendHandshakeAck()) {
            LOG_W(WIRELESS_MGR_LOG_TAG, "upper host handshake ack send failed");
            return;
        }

        if (gWirelessMgrHandshakeState != WIRELESS_MGR_HANDSHAKE_READY) {
            gWirelessMgrHandshakeState = WIRELESS_MGR_HANDSHAKE_READY;
            LOG_I(WIRELESS_MGR_LOG_TAG, "upper host handshake ready");
        }
        return;
    }

    if (!wirelessMgrIsHandshakeReady()) {
        LOG_W(WIRELESS_MGR_LOG_TAG, "drop cmd before handshake ready: 0x%02X", lCmd);
        return;
    }

    if (updateProtocolHandlePkt(pkt, E_UPDATE_PROTOCOL_TRANSPORT_BLE, &lReply) && lReply.hasReply) {
        if (!wirelessMgrSendProtocolFrame(lReply.cmd, lReply.payload, lReply.payloadLength)) {
            LOG_W(WIRELESS_MGR_LOG_TAG, "send reply failed, cmd=0x%02X", lReply.cmd);
        }
    }
}

static bool wirelessMgrValidateHandshakeFrame(const stFrmPsrPkt *pkt)
{
    uint8_t lMacBytes[WIRELESS_MGR_MAC_BYTE_SIZE];
    uint8_t lAesKey[AES_BLOCK_SIZE];
    uint8_t lPlainText[WIRELESS_MGR_HANDSHAKE_PAYLOAD_SIZE];
    uint8_t lExpectedPlainText[WIRELESS_MGR_HANDSHAKE_PAYLOAD_SIZE];
    stAesContext lAesContext;

    if ((pkt == NULL) || (pkt->dataBuf == NULL) || (pkt->dataLen != WIRELESS_MGR_HANDSHAKE_PAYLOAD_SIZE)) {
        return false;
    }

    if (!wirelessMgrLoadMacCrypto(lMacBytes, (uint16_t)sizeof(lMacBytes), lAesKey, (uint16_t)sizeof(lAesKey))) {
        return false;
    }

    if (aesInit(&lAesContext, AES_TYPE_128, AES_MODE_ECB, lAesKey, NULL) != AES_STATUS_OK) {
        return false;
    }

    if (aesDecrypt(&lAesContext, pkt->dataBuf, lPlainText, AES_BLOCK_SIZE) != AES_STATUS_OK) {
        return false;
    }

    (void)memset(lExpectedPlainText, 0, sizeof(lExpectedPlainText));
    (void)memcpy(lExpectedPlainText, lMacBytes, sizeof(lMacBytes));
    return memcmp(lPlainText, lExpectedPlainText, sizeof(lExpectedPlainText)) == 0;
}

static bool wirelessMgrBuildHandshakePayload(uint8_t *payload, uint16_t payloadSize)
{
    uint8_t lMacBytes[WIRELESS_MGR_MAC_BYTE_SIZE];
    uint8_t lAesKey[AES_BLOCK_SIZE];
    uint8_t lPlainText[WIRELESS_MGR_HANDSHAKE_PAYLOAD_SIZE];
    stAesContext lAesContext;

    if ((payload == NULL) || (payloadSize < WIRELESS_MGR_HANDSHAKE_PAYLOAD_SIZE)) {
        return false;
    }

    if (!wirelessMgrLoadMacCrypto(lMacBytes, (uint16_t)sizeof(lMacBytes), lAesKey, (uint16_t)sizeof(lAesKey))) {
        return false;
    }

    (void)memset(lPlainText, 0, sizeof(lPlainText));
    (void)memcpy(lPlainText, lMacBytes, sizeof(lMacBytes));
    if (aesInit(&lAesContext, AES_TYPE_128, AES_MODE_ECB, lAesKey, NULL) != AES_STATUS_OK) {
        return false;
    }

    return aesEncrypt(&lAesContext, lPlainText, payload, AES_BLOCK_SIZE) == AES_STATUS_OK;
}

static bool wirelessMgrSendHandshakeAck(void)
{
    uint8_t lPayload[WIRELESS_MGR_HANDSHAKE_PAYLOAD_SIZE];

    if (!wirelessMgrBuildHandshakePayload(lPayload, (uint16_t)sizeof(lPayload))) {
        return false;
    }

    return wirelessMgrSendProtocolFrame(WIRELESS_MGR_HANDSHAKE_CMD, lPayload, (uint16_t)sizeof(lPayload));
}

static bool wirelessMgrSendProtocolFrame(uint8_t cmd, const uint8_t *payload, uint16_t payloadLength)
{
    uint8_t lFrame[FRAMEPARSER_PORT_UPDATE_MAX_FRAME_SIZE];
    uint8_t lEncodedPayload[UPDATE_PROTOCOL_MAX_PAYLOAD_SIZE];
    const uint8_t *lpFramePayload;
    uint16_t lEncodedPayloadLength;
    uint16_t lFrameLength;
    uint16_t lCrc16;

    lpFramePayload = payload;
    lEncodedPayloadLength = payloadLength;
    if (wirelessMgrIsUpdateProtocolCmd(cmd)) {
        if (!updateProtocolEncodePayload(E_UPDATE_PROTOCOL_TRANSPORT_BLE,
                                         cmd,
                                         payload,
                                         payloadLength,
                                         lEncodedPayload,
                                         (uint16_t)sizeof(lEncodedPayload),
                                         &lEncodedPayloadLength)) {
            return false;
        }
        lpFramePayload = lEncodedPayload;
    }

    lFrame[0] = UPDATE_PROTOCOL_SYNC0;
    lFrame[1] = UPDATE_PROTOCOL_SYNC1;
    lFrame[2] = UPDATE_PROTOCOL_SYNC2;
    lFrame[3] = cmd;
    lFrame[4] = (uint8_t)((lEncodedPayloadLength >> 8U) & 0xFFU);
    lFrame[5] = (uint8_t)(lEncodedPayloadLength & 0xFFU);
    if ((lpFramePayload != NULL) && (lEncodedPayloadLength > 0U)) {
        (void)memcpy(&lFrame[UPDATE_PROTOCOL_FRAME_HEADER_SIZE], lpFramePayload, lEncodedPayloadLength);
    }

    lFrameLength = (uint16_t)(UPDATE_PROTOCOL_FRAME_HEADER_SIZE + lEncodedPayloadLength + UPDATE_PROTOCOL_FRAME_CRC16_SIZE);
    lCrc16 = (uint16_t)updateProtocolCalcCrc16(&lFrame[UPDATE_PROTOCOL_CMD_OFFSET],
                                               UPDATE_PROTOCOL_CMD_SIZE + UPDATE_PROTOCOL_ENCODED_LENGTH_SIZE + lEncodedPayloadLength,
                                               NULL);
    lFrame[lFrameLength - 2U] = (uint8_t)((lCrc16 >> 8U) & 0xFFU);
    lFrame[lFrameLength - 1U] = (uint8_t)(lCrc16 & 0xFFU);
    return fc41dWriteData(WIRELESS_MGR_DEVICE, lFrame, lFrameLength) == FC41D_STATUS_OK;
}

static bool wirelessMgrIsUpdateProtocolCmd(uint8_t cmd)
{
    switch (cmd) {
        case E_UPDATE_PROTOCOL_CMD_HANDSHAKE:
        case E_UPDATE_PROTOCOL_CMD_OTA_REQUEST:
        case E_UPDATE_PROTOCOL_CMD_OTA_FILE_INFO:
        case E_UPDATE_PROTOCOL_CMD_OTA_OFFSET:
        case E_UPDATE_PROTOCOL_CMD_OTA_DATA:
        case E_UPDATE_PROTOCOL_CMD_OTA_RESULT:
            return true;
        default:
            return false;
    }
}

static bool wirelessMgrLoadMacCrypto(uint8_t *macBytes, uint16_t macByteSize, uint8_t *aesKey, uint16_t aesKeySize)
{
    char lMacText[WIRELESS_MGR_MAC_ADDRESS_TEXT_MAX_LENGTH];

    if ((macBytes == NULL) || (macByteSize < WIRELESS_MGR_MAC_BYTE_SIZE) || (aesKey == NULL) || (aesKeySize < AES_BLOCK_SIZE)) {
        return false;
    }

    if (!wirelessMgrGetMacAddress(lMacText, (uint16_t)sizeof(lMacText))) {
        return false;
    }
    if (!wirelessMgrParseMacText(lMacText, macBytes, macByteSize)) {
        return false;
    }
    if (md5CalcData(macBytes, WIRELESS_MGR_MAC_BYTE_SIZE, aesKey) != MD5_STATUS_OK) {
        return false;
    }

    return true;
}

static bool wirelessMgrParseMacText(const char *macText, uint8_t *macBytes, uint16_t macByteSize)
{
    uint16_t lHexCount;
    int8_t lHigh;
    int8_t lLow;
    char lHexText[WIRELESS_MGR_MAC_BYTE_SIZE * 2U];

    if ((macText == NULL) || (macBytes == NULL) || (macByteSize < WIRELESS_MGR_MAC_BYTE_SIZE)) {
        return false;
    }

    lHexCount = 0U;
    while (*macText != '\0') {
        if ((*macText != ':') && (*macText != '-')) {
            if (lHexCount >= sizeof(lHexText)) {
                return false;
            }
            lHexText[lHexCount++] = *macText;
        }
        macText++;
    }

    if (lHexCount != sizeof(lHexText)) {
        return false;
    }

    for (lHexCount = 0U; lHexCount < WIRELESS_MGR_MAC_BYTE_SIZE; lHexCount++) {
        lHigh = wirelessMgrHexToNibble(lHexText[lHexCount * 2U]);
        lLow = wirelessMgrHexToNibble(lHexText[(lHexCount * 2U) + 1U]);
        if ((lHigh < 0) || (lLow < 0)) {
            return false;
        }
        macBytes[lHexCount] = (uint8_t)(((uint8_t)lHigh << 4U) | (uint8_t)lLow);
    }

    return true;
}

static int8_t wirelessMgrHexToNibble(char value)
{
    if ((value >= '0') && (value <= '9')) {
        return (int8_t)(value - '0');
    }
    if ((value >= 'a') && (value <= 'f')) {
        return (int8_t)(value - 'a' + 10);
    }
    if ((value >= 'A') && (value <= 'F')) {
        return (int8_t)(value - 'A' + 10);
    }

    return -1;
}
/**************************End of file********************************/
