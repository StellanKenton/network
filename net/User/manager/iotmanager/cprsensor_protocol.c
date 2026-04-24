/***********************************************************************************
* @file     : cprsensor_protocol.c
* @brief    : CPR sensor BLE protocol helpers.
* @details  : Implements CRC16, frame pack/parse, and history sub-record helpers.
* @author   : GitHub Copilot
* @date     : 2026-04-24
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "cprsensor_protocol.h"

#include <stddef.h>
#include <string.h>

static uint8_t cprsensorProtocolReverse8(uint8_t value);
static uint16_t cprsensorProtocolReverse16(uint16_t value);
static bool cprsensorProtocolIsCipherCfgValid(const stCprsensorProtocolCipherCfg *cipherCfg);
static uint16_t cprsensorProtocolGetEncodedPayloadLen(uint16_t payloadLen, const stCprsensorProtocolCipherCfg *cipherCfg);

static uint8_t cprsensorProtocolReverse8(uint8_t value)
{
	uint8_t lResult;
	uint8_t lIndex;

	lResult = 0U;
	for (lIndex = 0U; lIndex < 8U; lIndex++) {
		lResult = (uint8_t)((lResult << 1U) | (value & 0x01U));
		value >>= 1U;
	}

	return lResult;
}

static uint16_t cprsensorProtocolReverse16(uint16_t value)
{
	uint16_t lResult;
	uint8_t lIndex;

	lResult = 0U;
	for (lIndex = 0U; lIndex < 16U; lIndex++) {
		lResult = (uint16_t)((lResult << 1U) | (value & 0x01U));
		value >>= 1U;
	}

	return lResult;
}

static bool cprsensorProtocolIsCipherCfgValid(const stCprsensorProtocolCipherCfg *cipherCfg)
{
	if (cipherCfg == NULL) {
		return false;
	}

	if (!cipherCfg->enabled) {
		return true;
	}

	if ((cipherCfg->blockSize == 0U) || (cipherCfg->encrypt == NULL) || (cipherCfg->decrypt == NULL)) {
		return false;
	}

	return true;
}

static uint16_t cprsensorProtocolGetEncodedPayloadLen(uint16_t payloadLen, const stCprsensorProtocolCipherCfg *cipherCfg)
{
	if ((cipherCfg == NULL) || !cipherCfg->enabled || (payloadLen == 0U)) {
		return payloadLen;
	}

	return cprsensorProtocolAlignLength(payloadLen, cipherCfg->blockSize);
}

uint16_t cprsensorProtocolReadU16Be(const uint8_t *buffer)
{
	if (buffer == NULL) {
		return 0U;
	}

	return (uint16_t)(((uint16_t)buffer[0] << 8U) | (uint16_t)buffer[1]);
}

uint16_t cprsensorProtocolReadU16Le(const uint8_t *buffer)
{
	if (buffer == NULL) {
		return 0U;
	}

	return (uint16_t)(((uint16_t)buffer[1] << 8U) | (uint16_t)buffer[0]);
}

uint32_t cprsensorProtocolReadU32Be(const uint8_t *buffer)
{
	if (buffer == NULL) {
		return 0UL;
	}

	return ((uint32_t)buffer[0] << 24U) |
		   ((uint32_t)buffer[1] << 16U) |
		   ((uint32_t)buffer[2] << 8U) |
		   (uint32_t)buffer[3];
}

uint32_t cprsensorProtocolReadU32Le(const uint8_t *buffer)
{
	if (buffer == NULL) {
		return 0UL;
	}

	return ((uint32_t)buffer[3] << 24U) |
		   ((uint32_t)buffer[2] << 16U) |
		   ((uint32_t)buffer[1] << 8U) |
		   (uint32_t)buffer[0];
}

void cprsensorProtocolWriteU16Be(uint8_t *buffer, uint16_t value)
{
	if (buffer == NULL) {
		return;
	}

	buffer[0] = (uint8_t)(value >> 8U);
	buffer[1] = (uint8_t)value;
}

void cprsensorProtocolWriteU16Le(uint8_t *buffer, uint16_t value)
{
	if (buffer == NULL) {
		return;
	}

	buffer[0] = (uint8_t)value;
	buffer[1] = (uint8_t)(value >> 8U);
}

void cprsensorProtocolWriteU32Be(uint8_t *buffer, uint32_t value)
{
	if (buffer == NULL) {
		return;
	}

	buffer[0] = (uint8_t)(value >> 24U);
	buffer[1] = (uint8_t)(value >> 16U);
	buffer[2] = (uint8_t)(value >> 8U);
	buffer[3] = (uint8_t)value;
}

void cprsensorProtocolWriteU32Le(uint8_t *buffer, uint32_t value)
{
	if (buffer == NULL) {
		return;
	}

	buffer[0] = (uint8_t)value;
	buffer[1] = (uint8_t)(value >> 8U);
	buffer[2] = (uint8_t)(value >> 16U);
	buffer[3] = (uint8_t)(value >> 24U);
}

uint16_t cprsensorProtocolAlignLength(uint16_t length, uint8_t alignSize)
{
	uint16_t lRemainder;
	uint16_t lPadding;

	if ((alignSize == 0U) || (length == 0U)) {
		return length;
	}

	lRemainder = (uint16_t)(length % alignSize);
	if (lRemainder == 0U) {
		return length;
	}

	lPadding = (uint16_t)(alignSize - lRemainder);
	return (uint16_t)(length + lPadding);
}

uint16_t cprsensorProtocolGetFrameLength(uint16_t encodedPayloadLen)
{
	return (uint16_t)(CPRSENSOR_PROTOCOL_FRAME_OVERHEAD + encodedPayloadLen);
}

uint16_t cprsensorProtocolCrc16Calculate(const uint8_t *data, uint16_t length, const stCprsensorProtocolCrcCfg *crcCfg)
{
	uint16_t lCrc;
	uint16_t lByteIndex;
	uint8_t lBitIndex;
	uint8_t lDataByte;

	if ((data == NULL) || (crcCfg == NULL)) {
		return 0U;
	}

	lCrc = crcCfg->initValue;
	for (lByteIndex = 0U; lByteIndex < length; lByteIndex++) {
		lDataByte = data[lByteIndex];
		if (crcCfg->reflectInput) {
			lDataByte = cprsensorProtocolReverse8(lDataByte);
		}

		lCrc ^= (uint16_t)((uint16_t)lDataByte << 8U);
		for (lBitIndex = 0U; lBitIndex < 8U; lBitIndex++) {
			if ((lCrc & 0x8000U) != 0U) {
				lCrc = (uint16_t)((lCrc << 1U) ^ crcCfg->polynomial);
			} else {
				lCrc <<= 1U;
			}
		}
	}

	if (crcCfg->reflectOutput) {
		lCrc = cprsensorProtocolReverse16(lCrc);
	}

	lCrc ^= crcCfg->xorOut;
	return lCrc;
}

eCprsensorProtocolStatus cprsensorProtocolTryGetFrameLength(const uint8_t *buffer, uint16_t bufferLen, uint16_t *frameLen)
{
	uint16_t lEncodedPayloadLen;

	if ((buffer == NULL) || (frameLen == NULL)) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_PARAM;
	}

	if (bufferLen < CPRSENSOR_PROTOCOL_FRAME_HEAD_SIZE) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_LENGTH;
	}

	if ((buffer[0] != CPRSENSOR_PROTOCOL_FRAME_HEAD0) || (buffer[1] != CPRSENSOR_PROTOCOL_FRAME_HEAD1)) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_HEADER;
	}

	if (buffer[2] != CPRSENSOR_PROTOCOL_FRAME_VERSION) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_VERSION;
	}

	lEncodedPayloadLen = cprsensorProtocolReadU16Be(&buffer[4]);
	*frameLen = cprsensorProtocolGetFrameLength(lEncodedPayloadLen);
	if (*frameLen < CPRSENSOR_PROTOCOL_FRAME_OVERHEAD) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_FRAME_LEN;
	}

	if (bufferLen < *frameLen) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_LENGTH;
	}

	return CPRSENSOR_PROTOCOL_STATUS_OK;
}

eCprsensorProtocolStatus cprsensorProtocolParseFrame(const uint8_t *buffer,
					  uint16_t bufferLen,
					  const stCprsensorProtocolCodecCfg *codecCfg,
					  uint8_t *payloadBuffer,
					  uint16_t payloadBufferSize,
					  stCprsensorProtocolFrameView *frameView)
{
	uint16_t lFrameLen;
	uint16_t lEncodedPayloadLen;
	uint16_t lCrc16;
	uint16_t lCalculatedCrc16;
	const uint8_t *lEncodedPayload;

	if ((buffer == NULL) || (codecCfg == NULL) || (frameView == NULL)) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_PARAM;
	}

	if (!cprsensorProtocolIsCipherCfgValid(&codecCfg->cipher)) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_PARAM;
	}

	lCalculatedCrc16 = 0U;
	lFrameLen = 0U;
	if (cprsensorProtocolTryGetFrameLength(buffer, bufferLen, &lFrameLen) != CPRSENSOR_PROTOCOL_STATUS_OK) {
		return cprsensorProtocolTryGetFrameLength(buffer, bufferLen, &lFrameLen);
	}

	lEncodedPayloadLen = cprsensorProtocolReadU16Be(&buffer[4]);
	lEncodedPayload = &buffer[CPRSENSOR_PROTOCOL_FRAME_HEAD_SIZE];
	lCrc16 = cprsensorProtocolReadU16Be(&buffer[CPRSENSOR_PROTOCOL_FRAME_HEAD_SIZE + lEncodedPayloadLen]);
	lCalculatedCrc16 = cprsensorProtocolCrc16Calculate(&buffer[3],
							      (uint16_t)(1U + 2U + lEncodedPayloadLen),
							      &codecCfg->crc);
	if (lCrc16 != lCalculatedCrc16) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_CRC;
	}

	if (lEncodedPayloadLen > 0U) {
		if ((payloadBuffer == NULL) || (payloadBufferSize < lEncodedPayloadLen)) {
			return CPRSENSOR_PROTOCOL_STATUS_ERROR_BUFFER;
		}

		(void)memcpy(payloadBuffer, lEncodedPayload, lEncodedPayloadLen);
		if (codecCfg->cipher.enabled && !codecCfg->cipher.decrypt(codecCfg->cipher.userData,
								 payloadBuffer,
								 lEncodedPayloadLen)) {
			return CPRSENSOR_PROTOCOL_STATUS_ERROR_CIPHER;
		}
	}

	frameView->cmd = (eCprsensorProtocolCmd)buffer[3];
	frameView->version = buffer[2];
	frameView->encodedPayload = (lEncodedPayloadLen > 0U) ? lEncodedPayload : NULL;
	frameView->encodedPayloadLen = lEncodedPayloadLen;
	frameView->payload = (lEncodedPayloadLen > 0U) ? payloadBuffer : NULL;
	frameView->payloadLen = lEncodedPayloadLen;
	frameView->crc16 = lCrc16;
	(void)lFrameLen;
	return CPRSENSOR_PROTOCOL_STATUS_OK;
}

eCprsensorProtocolStatus cprsensorProtocolPackFrame(eCprsensorProtocolCmd cmd,
					 const uint8_t *payload,
					 uint16_t payloadLen,
					 const stCprsensorProtocolCodecCfg *codecCfg,
					 uint8_t *frameBuffer,
					 uint16_t frameCapacity,
					 uint16_t *frameLen)
{
	uint16_t lEncodedPayloadLen;
	uint16_t lCrc16;
	uint16_t lTotalLen;
	uint8_t *lPayloadStart;

	if ((codecCfg == NULL) || (frameBuffer == NULL) || (frameLen == NULL)) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_PARAM;
	}

	if ((payloadLen > 0U) && (payload == NULL)) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_PARAM;
	}

	if (!cprsensorProtocolIsCipherCfgValid(&codecCfg->cipher)) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_PARAM;
	}

	lEncodedPayloadLen = cprsensorProtocolGetEncodedPayloadLen(payloadLen, &codecCfg->cipher);
	lTotalLen = cprsensorProtocolGetFrameLength(lEncodedPayloadLen);
	if (frameCapacity < lTotalLen) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_BUFFER;
	}

	frameBuffer[0] = CPRSENSOR_PROTOCOL_FRAME_HEAD0;
	frameBuffer[1] = CPRSENSOR_PROTOCOL_FRAME_HEAD1;
	frameBuffer[2] = CPRSENSOR_PROTOCOL_FRAME_VERSION;
	frameBuffer[3] = (uint8_t)cmd;
	cprsensorProtocolWriteU16Be(&frameBuffer[4], lEncodedPayloadLen);

	lPayloadStart = &frameBuffer[CPRSENSOR_PROTOCOL_FRAME_HEAD_SIZE];
	if (lEncodedPayloadLen > 0U) {
		(void)memset(lPayloadStart, 0, lEncodedPayloadLen);
		if (payloadLen > 0U) {
			(void)memcpy(lPayloadStart, payload, payloadLen);
		}

		if (codecCfg->cipher.enabled && !codecCfg->cipher.encrypt(codecCfg->cipher.userData,
											 lPayloadStart,
											 lEncodedPayloadLen)) {
			return CPRSENSOR_PROTOCOL_STATUS_ERROR_CIPHER;
		}
	}

	lCrc16 = cprsensorProtocolCrc16Calculate(&frameBuffer[3],
							     (uint16_t)(1U + 2U + lEncodedPayloadLen),
							     &codecCfg->crc);
	cprsensorProtocolWriteU16Be(&frameBuffer[CPRSENSOR_PROTOCOL_FRAME_HEAD_SIZE + lEncodedPayloadLen], lCrc16);

	*frameLen = lTotalLen;
	return CPRSENSOR_PROTOCOL_STATUS_OK;
}

eCprsensorProtocolStatus cprsensorProtocolPackHistorySubRecord(eCprsensorProtocolCmd recordCmd,
						 const uint8_t *recordData,
						 uint8_t recordDataLen,
						 uint8_t *buffer,
						 uint16_t bufferCapacity,
						 uint16_t *recordLen)
{
	uint16_t lTotalLen;

	if ((buffer == NULL) || (recordLen == NULL)) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_PARAM;
	}

	if ((recordDataLen > 0U) && (recordData == NULL)) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_PARAM;
	}

	lTotalLen = (uint16_t)(recordDataLen + 2U);
	if (bufferCapacity < lTotalLen) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_BUFFER;
	}

	buffer[0] = (uint8_t)(recordDataLen + 1U);
	buffer[1] = (uint8_t)recordCmd;
	if (recordDataLen > 0U) {
		(void)memcpy(&buffer[2], recordData, recordDataLen);
	}

	*recordLen = lTotalLen;
	return CPRSENSOR_PROTOCOL_STATUS_OK;
}

eCprsensorProtocolStatus cprsensorProtocolParseHistorySubRecord(const uint8_t *buffer,
						  uint16_t bufferLen,
						  stCprsensorProtocolHistorySubRecordView *recordView)
{
	uint16_t lTotalLen;

	if ((buffer == NULL) || (recordView == NULL)) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_PARAM;
	}

	if (bufferLen < 2U) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_LENGTH;
	}

	lTotalLen = (uint16_t)(buffer[0] + 1U);
	if ((buffer[0] == 0U) || (lTotalLen > bufferLen)) {
		return CPRSENSOR_PROTOCOL_STATUS_ERROR_FRAME_LEN;
	}

	recordView->cmd = (eCprsensorProtocolCmd)buffer[1];
	recordView->recordLen = buffer[0];
	recordView->recordDataLen = (uint8_t)(buffer[0] - 1U);
	recordView->recordData = (recordView->recordDataLen > 0U) ? &buffer[2] : NULL;
	return CPRSENSOR_PROTOCOL_STATUS_OK;
}

/**************************End of file********************************/
