/***********************************************************************************
* @file     : update_protocol.c
* @brief    : BLE OTA protocol helpers.
* @details  : Provides shared helpers for the project communication layer and
*             update flow.
* @author   : GitHub Copilot
* @date     : 2026-04-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "update_protocol.h"

#include <stddef.h>
#include <string.h>

#include "../wireless/wirelss_mgr.h"
#include "../../manager/update/update_mgr.h"

#include "../../../rep/tools/aes/aes.h"
#include "../../../rep/tools/md5/md5.h"

#include "../../../rep/service/log/log.h"
#include "../../system/system.h"

#define UPDATE_PROTOCOL_LOG_TAG                      "UpdateProto"

static const stUpdateProtocolVersion gSupportedVersion = {
    .major = (uint8_t)FW_VER_MAJOR,
    .minor = (uint8_t)FW_VER_MINOR,
    .patch = (uint8_t)FW_VER_PATCH,
    .build = 0U,
};

static const stUpdateProtocolOtaRequestPayload gDefaultOtaRequest = {
    .preferredPacketLength = UPDATE_PROTOCOL_OTA_PACKET_LEN_DEFAULT,
};

static bool updateProtocolIsPktValid(const stFrmPsrPkt *pkt);
static uint8_t updateProtocolGetCmd(const stFrmPsrPkt *pkt);
static void updateProtocolPrepareReply(stUpdateProtocolReply *reply, uint8_t cmd);
static bool updateProtocolReplyHandshake(const stFrmPsrPkt *pkt, eUpdateProtocolTransport transport, stUpdateProtocolReply *reply);
static bool updateProtocolReplyRequest(const stFrmPsrPkt *pkt, eUpdateProtocolTransport transport, stUpdateProtocolReply *reply);
static bool updateProtocolReplyFileInfo(const stFrmPsrPkt *pkt, eUpdateProtocolTransport transport, stUpdateProtocolReply *reply);
static bool updateProtocolReplyOffset(const stFrmPsrPkt *pkt, eUpdateProtocolTransport transport, stUpdateProtocolReply *reply);
static bool updateProtocolReplyData(const stFrmPsrPkt *pkt, eUpdateProtocolTransport transport, stUpdateProtocolReply *reply);
static bool updateProtocolReplyResult(const stFrmPsrPkt *pkt, eUpdateProtocolTransport transport, stUpdateProtocolReply *reply);
static bool updateProtocolEncodeMacText(const char *macText, uint8_t *encodedMac, uint16_t encodedMacSize);
static bool updateProtocolIsHexChar(uint8_t ch);
static bool updateProtocolShouldUseBleCrypto(eUpdateProtocolTransport transport);
static bool updateProtocolLoadBleCrypto(uint8_t *macBytes, uint16_t macByteSize, uint8_t *aesKey, uint16_t aesKeySize);
static bool updateProtocolParseMacText(const char *macText, uint8_t *macBytes, uint16_t macByteSize);
static int8_t updateProtocolHexToNibble(char value);
static bool updateProtocolCryptPayload(bool encrypt,
                                       const uint8_t *input,
                                       uint16_t inputLength,
                                       uint8_t *output,
                                       uint16_t outputSize,
                                       uint16_t *outputLength);
static bool updateProtocolDecodePayload(const stFrmPsrPkt *pkt,
                                        eUpdateProtocolTransport transport,
                                        uint8_t *plainPayload,
                                        uint16_t plainPayloadSize,
                                        const uint8_t **payloadOut,
                                        uint16_t *payloadLengthOut);

__attribute__((weak)) bool updateManagerBuildOtaRequestReply(const stUpdateProtocolOtaRequestPayload *request,
                                                             stUpdateProtocolOtaRequestReplyPayload *reply)
{
    (void)request;
    (void)reply;
    return false;
}

__attribute__((weak)) bool updateManagerPrepareOtaImage(const stUpdateProtocolOtaFileInfoPayload *fileInfo,
                                                        stUpdateProtocolOtaFileInfoReplyPayload *reply)
{
    (void)fileInfo;
    (void)reply;
    return false;
}

__attribute__((weak)) bool updateManagerGetOtaOffset(const stUpdateProtocolOtaOffsetPayload *request,
                                                     stUpdateProtocolOtaOffsetReplyPayload *reply)
{
    (void)request;
    (void)reply;
    return false;
}

__attribute__((weak)) bool updateManagerWriteOtaData(const stUpdateProtocolOtaDataPayload *payload,
                                                     const uint8_t *data,
                                                     uint16_t actualDataLength,
                                                     stUpdateProtocolOtaDataReplyPayload *reply)
{
    (void)payload;
    (void)data;
    (void)actualDataLength;
    (void)reply;
    return false;
}

__attribute__((weak)) bool updateManagerFinalizeOtaImage(stUpdateProtocolOtaResultReplyPayload *reply)
{
    (void)reply;
    return false;
}


uint32_t updateProtocolGetPktLen(const uint8_t *buf, uint32_t headLen, uint32_t availLen, void *userCtx)
{
    uint32_t lEncodedLength;

    (void)headLen;
    (void)userCtx;

    if ((buf == NULL) || (availLen < UPDATE_PROTOCOL_FRAME_HEADER_SIZE)) {
        return 0U;
    }

    lEncodedLength = ((uint32_t)buf[UPDATE_PROTOCOL_ENCODED_LENGTH_OFFSET] << 8U) |
                     (uint32_t)buf[UPDATE_PROTOCOL_ENCODED_LENGTH_OFFSET + 1U];
    return UPDATE_PROTOCOL_FRAME_HEADER_SIZE + lEncodedLength + UPDATE_PROTOCOL_FRAME_CRC16_SIZE;
}

uint32_t updateProtocolCalcCrc16(const uint8_t *buf, uint32_t len, void *userCtx)
{
    uint32_t lIndex;
    uint8_t lBit;
    uint16_t lCrc = 0U;

    (void)userCtx;

    if ((buf == NULL) && (len > 0U)) {
        return 0U;
    }

    for (lIndex = 0U; lIndex < len; lIndex++) {
        lCrc ^= buf[lIndex];
        for (lBit = 0U; lBit < 8U; lBit++) {
            if ((lCrc & 0x0001U) != 0U) {
                lCrc = (uint16_t)((lCrc >> 1U) ^ 0xA001U);
            } else {
                lCrc >>= 1U;
            }
        }
    }

    return lCrc;
}

static bool updateProtocolIsPktValid(const stFrmPsrPkt *pkt)
{
    if ((pkt == NULL) || (pkt->cmdBuf == NULL) || (pkt->cmdFieldLen == 0U)) {
        LOG_W(UPDATE_PROTOCOL_LOG_TAG, "drop invalid packet view");
        return false;
    }

    return true;
}

static uint8_t updateProtocolGetCmd(const stFrmPsrPkt *pkt)
{
    return pkt->cmdBuf[0];
}

static void updateProtocolPrepareReply(stUpdateProtocolReply *reply, uint8_t cmd)
{
    if (reply == NULL) {
        return;
    }

    (void)memset(reply, 0, sizeof(*reply));
    reply->cmd = cmd;
    reply->hasReply = false;
}

static bool updateProtocolReplyHandshake(const stFrmPsrPkt *pkt, eUpdateProtocolTransport transport, stUpdateProtocolReply *reply)
{
    stUpdateProtocolHandshakePayload lReplyPayload;
    char lMacText[WIRELESS_MGR_MAC_ADDRESS_TEXT_MAX_LENGTH];

    (void)pkt;
    (void)transport;

    if (reply == NULL) {
        return false;
    }

    (void)memset(&lReplyPayload, 0, sizeof(lReplyPayload));
    if (!wirelessMgrGetMacAddress(lMacText, (uint16_t)sizeof(lMacText))) {
        LOG_W(UPDATE_PROTOCOL_LOG_TAG, "handshake mac unavailable");
        return false;
    }

    if (!updateProtocolEncodeMacText(lMacText,
                                     lReplyPayload.encryptedMac,
                                     (uint16_t)sizeof(lReplyPayload.encryptedMac))) {
        LOG_W(UPDATE_PROTOCOL_LOG_TAG, "handshake mac encode failed");
        return false;
    }

    reply->payloadLength = sizeof(lReplyPayload);
    (void)memcpy(reply->payload, &lReplyPayload, sizeof(lReplyPayload));
    reply->hasReply = true;
    return true;
}

static bool updateProtocolReplyRequest(const stFrmPsrPkt *pkt, eUpdateProtocolTransport transport, stUpdateProtocolReply *reply)
{
    stUpdateProtocolOtaRequestPayload lRequest;
    stUpdateProtocolOtaRequestReplyPayload lReplyPayload;
    uint8_t lPlainPayload[UPDATE_PROTOCOL_MAX_PAYLOAD_SIZE];
    const uint8_t *lpPayload;
    uint16_t lPayloadLength;

    if ((pkt == NULL) || (reply == NULL)) {
        return false;
    }

    if (!updateProtocolDecodePayload(pkt,
                                     transport,
                                     lPlainPayload,
                                     (uint16_t)sizeof(lPlainPayload),
                                     &lpPayload,
                                     &lPayloadLength) ||
        (lPayloadLength < sizeof(lRequest))) {
        return false;
    }

    (void)memcpy(&lRequest, lpPayload, sizeof(lRequest));
    if (!updateManagerBuildOtaRequestReply(&lRequest, &lReplyPayload)) {
        return false;
    }

    reply->payloadLength = sizeof(lReplyPayload);
    (void)memcpy(reply->payload, &lReplyPayload, sizeof(lReplyPayload));
    reply->hasReply = true;
    return true;
}

static bool updateProtocolReplyFileInfo(const stFrmPsrPkt *pkt, eUpdateProtocolTransport transport, stUpdateProtocolReply *reply)
{
    stUpdateProtocolOtaFileInfoPayload lFileInfo;
    stUpdateProtocolOtaFileInfoReplyPayload lReplyPayload;
    uint8_t lPlainPayload[UPDATE_PROTOCOL_MAX_PAYLOAD_SIZE];
    const uint8_t *lpPayload;
    uint16_t lPayloadLength;

    if ((pkt == NULL) || (reply == NULL)) {
        return false;
    }

    if (!updateProtocolDecodePayload(pkt,
                                     transport,
                                     lPlainPayload,
                                     (uint16_t)sizeof(lPlainPayload),
                                     &lpPayload,
                                     &lPayloadLength) ||
        (lPayloadLength < sizeof(lFileInfo))) {
        return false;
    }

    (void)memcpy(&lFileInfo, lpPayload, sizeof(lFileInfo));
    if (!updateManagerPrepareOtaImage(&lFileInfo, &lReplyPayload)) {
        return false;
    }

    reply->payloadLength = sizeof(lReplyPayload);
    (void)memcpy(reply->payload, &lReplyPayload, sizeof(lReplyPayload));
    reply->hasReply = true;
    return true;
}

static bool updateProtocolReplyOffset(const stFrmPsrPkt *pkt, eUpdateProtocolTransport transport, stUpdateProtocolReply *reply)
{
    stUpdateProtocolOtaOffsetPayload lOffset;
    stUpdateProtocolOtaOffsetReplyPayload lReplyPayload;
    uint8_t lPlainPayload[UPDATE_PROTOCOL_MAX_PAYLOAD_SIZE];
    const uint8_t *lpPayload;
    uint16_t lPayloadLength;

    if ((pkt == NULL) || (reply == NULL)) {
        return false;
    }

    if (!updateProtocolDecodePayload(pkt,
                                     transport,
                                     lPlainPayload,
                                     (uint16_t)sizeof(lPlainPayload),
                                     &lpPayload,
                                     &lPayloadLength) ||
        (lPayloadLength < sizeof(lOffset))) {
        return false;
    }

    (void)memcpy(&lOffset, lpPayload, sizeof(lOffset));
    if (!updateManagerGetOtaOffset(&lOffset, &lReplyPayload)) {
        return false;
    }

    reply->payloadLength = sizeof(lReplyPayload);
    (void)memcpy(reply->payload, &lReplyPayload, sizeof(lReplyPayload));
    reply->hasReply = true;
    return true;
}

static bool updateProtocolReplyData(const stFrmPsrPkt *pkt, eUpdateProtocolTransport transport, stUpdateProtocolReply *reply)
{
    stUpdateProtocolOtaDataPayload lData;
    stUpdateProtocolOtaDataReplyPayload lReplyPayload;
    uint16_t lActualDataLength;
    uint8_t lPlainPayload[UPDATE_PROTOCOL_MAX_PAYLOAD_SIZE];
    const uint8_t *lpPayload;
    const uint8_t *lpData;
    uint16_t lPayloadLength;

    if ((pkt == NULL) || (reply == NULL)) {
        return false;
    }

    if (!updateProtocolDecodePayload(pkt,
                                     transport,
                                     lPlainPayload,
                                     (uint16_t)sizeof(lPlainPayload),
                                     &lpPayload,
                                     &lPayloadLength) ||
        (lPayloadLength < sizeof(lData))) {
        return false;
    }

    (void)memcpy(&lData, lpPayload, sizeof(lData));
    if ((lPayloadLength - sizeof(lData)) < lData.dataLength) {
        return false;
    }

    lActualDataLength = updateProtocolShouldUseBleCrypto(transport) ? lData.dataLength : (uint16_t)(lPayloadLength - sizeof(lData));
    lpData = lpPayload + sizeof(lData);
    if (!updateManagerWriteOtaData(&lData, lpData, lActualDataLength, &lReplyPayload)) {
        return false;
    }

    reply->payloadLength = sizeof(lReplyPayload);
    (void)memcpy(reply->payload, &lReplyPayload, sizeof(lReplyPayload));
    reply->hasReply = true;
    return true;
}

static bool updateProtocolReplyResult(const stFrmPsrPkt *pkt, eUpdateProtocolTransport transport, stUpdateProtocolReply *reply)
{
    stUpdateProtocolOtaResultReplyPayload lReplyPayload;

    if ((pkt == NULL) || (reply == NULL)) {
        return false;
    }

    (void)transport;

    if (!updateManagerFinalizeOtaImage(&lReplyPayload)) {
        return false;
    }

    reply->payloadLength = sizeof(lReplyPayload);
    (void)memcpy(reply->payload, &lReplyPayload, sizeof(lReplyPayload));
    reply->hasReply = true;
    return true;
}

static bool updateProtocolEncodeMacText(const char *macText, uint8_t *encodedMac, uint16_t encodedMacSize)
{
    uint16_t lInputIndex;
    uint16_t lOutputIndex;
    uint8_t lCh;

    if ((macText == NULL) || (encodedMac == NULL) || (encodedMacSize < UPDATE_PROTOCOL_HANDSHAKE_REPLY_SIZE)) {
        return false;
    }

    lOutputIndex = 0U;
    while (*macText != '\0') {
        lCh = (uint8_t)(*macText++);
        if ((lCh == ':') || (lCh == '-')) {
            continue;
        }

        if ((lCh >= 'a') && (lCh <= 'f')) {
            lCh = (uint8_t)(lCh - ('a' - 'A'));
        }

        if (!updateProtocolIsHexChar(lCh) || (lOutputIndex >= 12U)) {
            return false;
        }

        encodedMac[lOutputIndex++] = lCh;
    }

    if (lOutputIndex != 12U) {
        return false;
    }

    for (lInputIndex = lOutputIndex; lInputIndex < encodedMacSize; lInputIndex++) {
        encodedMac[lInputIndex] = 0U;
    }

    return true;
}

static bool updateProtocolIsHexChar(uint8_t ch)
{
    return ((ch >= '0') && (ch <= '9')) || ((ch >= 'A') && (ch <= 'F'));
}

static bool updateProtocolShouldUseBleCrypto(eUpdateProtocolTransport transport)
{
    return transport == E_UPDATE_PROTOCOL_TRANSPORT_BLE;
}

static bool updateProtocolLoadBleCrypto(uint8_t *macBytes, uint16_t macByteSize, uint8_t *aesKey, uint16_t aesKeySize)
{
    char lMacText[WIRELESS_MGR_MAC_ADDRESS_TEXT_MAX_LENGTH];

    if ((macBytes == NULL) || (macByteSize < 6U) || (aesKey == NULL) || (aesKeySize < AES_BLOCK_SIZE)) {
        return false;
    }

    if (!wirelessMgrGetMacAddress(lMacText, (uint16_t)sizeof(lMacText))) {
        return false;
    }
    if (!updateProtocolParseMacText(lMacText, macBytes, macByteSize)) {
        return false;
    }
    if (md5CalcData(macBytes, 6U, aesKey) != MD5_STATUS_OK) {
        return false;
    }

    return true;
}

static bool updateProtocolParseMacText(const char *macText, uint8_t *macBytes, uint16_t macByteSize)
{
    uint16_t lHexCount;
    int8_t lHigh;
    int8_t lLow;
    char lHexText[12U];

    if ((macText == NULL) || (macBytes == NULL) || (macByteSize < 6U)) {
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

    for (lHexCount = 0U; lHexCount < 6U; lHexCount++) {
        lHigh = updateProtocolHexToNibble(lHexText[lHexCount * 2U]);
        lLow = updateProtocolHexToNibble(lHexText[(lHexCount * 2U) + 1U]);
        if ((lHigh < 0) || (lLow < 0)) {
            return false;
        }
        macBytes[lHexCount] = (uint8_t)(((uint8_t)lHigh << 4U) | (uint8_t)lLow);
    }

    return true;
}

static int8_t updateProtocolHexToNibble(char value)
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

static bool updateProtocolCryptPayload(bool encrypt,
                                       const uint8_t *input,
                                       uint16_t inputLength,
                                       uint8_t *output,
                                       uint16_t outputSize,
                                       uint16_t *outputLength)
{
    uint8_t lMacBytes[6U];
    uint8_t lAesKey[AES_BLOCK_SIZE];
    stAesContext lAesContext;
    uint16_t lCryptLength;

    if ((output == NULL) || (outputLength == NULL)) {
        return false;
    }

    if ((input == NULL) && (inputLength > 0U)) {
        return false;
    }

    if (encrypt) {
        lCryptLength = (uint16_t)((inputLength + (AES_BLOCK_SIZE - 1U)) & (uint16_t)~(AES_BLOCK_SIZE - 1U));
    } else {
        if ((inputLength % AES_BLOCK_SIZE) != 0U) {
            return false;
        }
        lCryptLength = inputLength;
    }

    if (lCryptLength > outputSize) {
        return false;
    }

    if (!updateProtocolLoadBleCrypto(lMacBytes, (uint16_t)sizeof(lMacBytes), lAesKey, (uint16_t)sizeof(lAesKey))) {
        return false;
    }
    if (aesInit(&lAesContext, AES_TYPE_128, AES_MODE_ECB, lAesKey, NULL) != AES_STATUS_OK) {
        return false;
    }

    if (lCryptLength == 0U) {
        *outputLength = 0U;
        return true;
    }

    (void)memset(output, 0, lCryptLength);
    if (inputLength > 0U) {
        (void)memcpy(output, input, inputLength);
    }

    if (encrypt) {
        if (aesEncrypt(&lAesContext, output, output, lCryptLength) != AES_STATUS_OK) {
            return false;
        }
    } else {
        if (aesDecrypt(&lAesContext, input, output, lCryptLength) != AES_STATUS_OK) {
            return false;
        }
    }

    *outputLength = lCryptLength;
    return true;
}

static bool updateProtocolDecodePayload(const stFrmPsrPkt *pkt,
                                        eUpdateProtocolTransport transport,
                                        uint8_t *plainPayload,
                                        uint16_t plainPayloadSize,
                                        const uint8_t **payloadOut,
                                        uint16_t *payloadLengthOut)
{
    uint16_t lDecodedLength;

    if ((pkt == NULL) || (payloadOut == NULL) || (payloadLengthOut == NULL)) {
        return false;
    }

    if (!updateProtocolShouldUseBleCrypto(transport)) {
        *payloadOut = pkt->dataBuf;
        *payloadLengthOut = pkt->dataLen;
        return true;
    }

    if ((plainPayload == NULL) || (plainPayloadSize == 0U)) {
        return false;
    }

    if (!updateProtocolCryptPayload(false,
                                    pkt->dataBuf,
                                    pkt->dataLen,
                                    plainPayload,
                                    plainPayloadSize,
                                    &lDecodedLength)) {
        return false;
    }

    *payloadOut = plainPayload;
    *payloadLengthOut = lDecodedLength;
    return true;
}

bool updateProtocolEncodePayload(eUpdateProtocolTransport transport,
                                 uint8_t cmd,
                                 const uint8_t *payload,
                                 uint16_t payloadLength,
                                 uint8_t *encodedPayload,
                                 uint16_t encodedPayloadSize,
                                 uint16_t *encodedPayloadLength)
{
    (void)cmd;

    if ((encodedPayload == NULL) || (encodedPayloadLength == NULL)) {
        return false;
    }

    if (!updateProtocolShouldUseBleCrypto(transport)) {
        if (payloadLength > encodedPayloadSize) {
            return false;
        }
        if ((payload != NULL) && (payloadLength > 0U)) {
            (void)memcpy(encodedPayload, payload, payloadLength);
        }
        *encodedPayloadLength = payloadLength;
        return true;
    }

    return updateProtocolCryptPayload(true,
                                      payload,
                                      payloadLength,
                                      encodedPayload,
                                      encodedPayloadSize,
                                      encodedPayloadLength);
}

bool updateProtocolHandlePkt(const stFrmPsrPkt *pkt, eUpdateProtocolTransport transport, stUpdateProtocolReply *reply)
{
    if (!updateProtocolIsPktValid(pkt) || (reply == NULL)) {
        return false;
    }

    updateProtocolPrepareReply(reply, updateProtocolGetCmd(pkt));

    switch (updateProtocolGetCmd(pkt)) {
        case E_UPDATE_PROTOCOL_CMD_HANDSHAKE:
            return updateProtocolReplyHandshake(pkt, transport, reply);
        case E_UPDATE_PROTOCOL_CMD_OTA_REQUEST:
            return updateProtocolReplyRequest(pkt, transport, reply);
        case E_UPDATE_PROTOCOL_CMD_OTA_FILE_INFO:
            return updateProtocolReplyFileInfo(pkt, transport, reply);
        case E_UPDATE_PROTOCOL_CMD_OTA_OFFSET:
            return updateProtocolReplyOffset(pkt, transport, reply);
        case E_UPDATE_PROTOCOL_CMD_OTA_DATA:
            return updateProtocolReplyData(pkt, transport, reply);
        case E_UPDATE_PROTOCOL_CMD_OTA_RESULT:
            return updateProtocolReplyResult(pkt, transport, reply);
        default:
            LOG_W(UPDATE_PROTOCOL_LOG_TAG,
                  "unknown command: 0x%02X, payload=%u",
                  updateProtocolGetCmd(pkt),
                  (unsigned int)pkt->dataLen);
            return false;
    }
}

const stUpdateProtocolVersion *updateProtocolGetSupportedVersion(void)
{
    return &gSupportedVersion;
}

const stUpdateProtocolOtaRequestPayload *updateProtocolGetDefaultOtaRequest(void)
{
    return &gDefaultOtaRequest;
}

/**************************End of file********************************/
