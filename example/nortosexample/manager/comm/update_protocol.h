/***********************************************************************************
* @file     : update_protocol.h
* @brief    : BLE OTA protocol frame declarations.
* @details  : Defines command IDs, status enums, and payload layouts shared by
*             the project communication layer and update flow.
* @author   : GitHub Copilot
* @date     : 2026-04-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_UPDATE_PROTOCOL_H
#define CPRSENSORBOOT_UPDATE_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "../../port/frameparser_port.h"
#include "../../../rep/comm/frameparser/framepareser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UPDATE_PROTOCOL_SYNC0                      0xFAU
#define UPDATE_PROTOCOL_SYNC1                      0xFCU
#define UPDATE_PROTOCOL_SYNC2                      0x01U

#define UPDATE_PROTOCOL_SYNC_SIZE                  3U
#define UPDATE_PROTOCOL_CMD_SIZE                   1U
#define UPDATE_PROTOCOL_ENCODED_LENGTH_SIZE        2U
#define UPDATE_PROTOCOL_FRAME_HEADER_SIZE          (UPDATE_PROTOCOL_SYNC_SIZE + UPDATE_PROTOCOL_CMD_SIZE + UPDATE_PROTOCOL_ENCODED_LENGTH_SIZE)
#define UPDATE_PROTOCOL_FRAME_CRC16_SIZE           2U
#define UPDATE_PROTOCOL_VERSION_SIZE               4U

#define UPDATE_PROTOCOL_CMD_OFFSET                 UPDATE_PROTOCOL_SYNC_SIZE
#define UPDATE_PROTOCOL_ENCODED_LENGTH_OFFSET      (UPDATE_PROTOCOL_CMD_OFFSET + UPDATE_PROTOCOL_CMD_SIZE)
#define UPDATE_PROTOCOL_MAX_PAYLOAD_SIZE           (FRAMEPARSER_PORT_UPDATE_MAX_FRAME_SIZE - UPDATE_PROTOCOL_FRAME_HEADER_SIZE - UPDATE_PROTOCOL_FRAME_CRC16_SIZE)

#define UPDATE_PROTOCOL_OTA_PACKET_LEN_DEFAULT     48U
#define UPDATE_PROTOCOL_OTA_DATA_OVERHEAD          6U
#define UPDATE_PROTOCOL_OTA_DATA_MAX_LEN          (UPDATE_PROTOCOL_OTA_PACKET_LEN_DEFAULT - UPDATE_PROTOCOL_OTA_DATA_OVERHEAD)

#define UPDATE_PROTOCOL_HANDSHAKE_REPLY_SIZE       16U
#define UPDATE_PROTOCOL_OTA_REQUEST_REPLY_SIZE     9U
#define UPDATE_PROTOCOL_OTA_FILE_INFO_REPLY_SIZE   9U
#define UPDATE_PROTOCOL_OTA_OFFSET_REPLY_SIZE      4U
#define UPDATE_PROTOCOL_OTA_DATA_REPLY_SIZE        1U
#define UPDATE_PROTOCOL_OTA_RESULT_REPLY_SIZE      1U

typedef enum eUpdateProtocolTransport {
    E_UPDATE_PROTOCOL_TRANSPORT_UART = 0,
    E_UPDATE_PROTOCOL_TRANSPORT_BLE,
    E_UPDATE_PROTOCOL_TRANSPORT_USB,
} eUpdateProtocolTransport;

typedef enum eUpdateProtocolCmd {
    E_UPDATE_PROTOCOL_CMD_HANDSHAKE = 0xE0,
    E_UPDATE_PROTOCOL_CMD_OTA_REQUEST = 0xEA,
    E_UPDATE_PROTOCOL_CMD_OTA_FILE_INFO = 0xEB,
    E_UPDATE_PROTOCOL_CMD_OTA_OFFSET = 0xEC,
    E_UPDATE_PROTOCOL_CMD_OTA_DATA = 0xED,
    E_UPDATE_PROTOCOL_CMD_OTA_RESULT = 0xEE,
} eUpdateProtocolCmd;

typedef enum eUpdateProtocolOtaFileInfoState {
    E_UPDATE_PROTOCOL_OTA_FILE_INFO_ACCEPT = 0x00,
    E_UPDATE_PROTOCOL_OTA_FILE_INFO_VERSION_REJECT = 0x01,
    E_UPDATE_PROTOCOL_OTA_FILE_INFO_TOO_LARGE = 0x02,
} eUpdateProtocolOtaFileInfoState;

typedef enum eUpdateProtocolOtaDataState {
    E_UPDATE_PROTOCOL_OTA_DATA_ACCEPT = 0x00,
    E_UPDATE_PROTOCOL_OTA_DATA_PACKET_INDEX_ERROR = 0x01,
    E_UPDATE_PROTOCOL_OTA_DATA_LENGTH_ERROR = 0x02,
    E_UPDATE_PROTOCOL_OTA_DATA_CRC16_ERROR = 0x03,
    E_UPDATE_PROTOCOL_OTA_DATA_OTHER_ERROR = 0x04,
} eUpdateProtocolOtaDataState;

typedef enum eUpdateProtocolOtaResultState {
    E_UPDATE_PROTOCOL_OTA_RESULT_ACCEPT = 0x00,
    E_UPDATE_PROTOCOL_OTA_RESULT_TOTAL_LENGTH_ERROR = 0x01,
    E_UPDATE_PROTOCOL_OTA_RESULT_LENGTH_ERROR = 0x02,
    E_UPDATE_PROTOCOL_OTA_RESULT_IMAGE_VERIFY_ERROR = 0x03,
} eUpdateProtocolOtaResultState;

#pragma pack(push, 1)
typedef struct stUpdateProtocolFrameHeader {
    uint8_t sync0;
    uint8_t sync1;
    uint8_t sync2;
    uint8_t cmd;
    uint16_t encodedLength;
} stUpdateProtocolFrameHeader;

typedef struct stUpdateProtocolVersion {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint8_t build;
} stUpdateProtocolVersion;

typedef struct stUpdateProtocolHandshakePayload {
    uint8_t encryptedMac[16];
} stUpdateProtocolHandshakePayload;

typedef struct stUpdateProtocolTimeSyncPayload {
    uint32_t secondsSince20250101;
} stUpdateProtocolTimeSyncPayload;

typedef struct stUpdateProtocolOtaRequestPayload {
    uint16_t preferredPacketLength;
} stUpdateProtocolOtaRequestPayload;

typedef struct stUpdateProtocolOtaRequestReplyPayload {
    uint8_t otaFlag;
    stUpdateProtocolVersion version;
    uint16_t maxPacketLength;
    uint16_t appliedPacketLength;
} stUpdateProtocolOtaRequestReplyPayload;

typedef struct stUpdateProtocolOtaFileInfoPayload {
    stUpdateProtocolVersion version;
    uint32_t fileSize;
    uint32_t fileCrc32;
} stUpdateProtocolOtaFileInfoPayload;

typedef struct stUpdateProtocolOtaFileInfoReplyPayload {
    uint8_t state;
    uint32_t resumeOffset;
    uint32_t resumeCrc32;
} stUpdateProtocolOtaFileInfoReplyPayload;

typedef struct stUpdateProtocolOtaOffsetPayload {
    uint32_t offset;
} stUpdateProtocolOtaOffsetPayload;

typedef struct stUpdateProtocolOtaOffsetReplyPayload {
    uint32_t offset;
} stUpdateProtocolOtaOffsetReplyPayload;

typedef struct stUpdateProtocolOtaDataPayload {
    uint16_t packetNo;
    uint16_t dataLength;
    uint16_t dataCrc16;
} stUpdateProtocolOtaDataPayload;

typedef struct stUpdateProtocolOtaDataReplyPayload {
    uint8_t state;
} stUpdateProtocolOtaDataReplyPayload;

typedef struct stUpdateProtocolOtaResultReplyPayload {
    uint8_t state;
} stUpdateProtocolOtaResultReplyPayload;

typedef struct stUpdateProtocolReply {
    uint16_t payloadLength;
    uint8_t cmd;
    bool hasReply;
    uint8_t payload[UPDATE_PROTOCOL_MAX_PAYLOAD_SIZE];
} stUpdateProtocolReply;
#pragma pack(pop)

uint32_t updateProtocolGetPktLen(const uint8_t *buf, uint32_t headLen, uint32_t availLen, void *userCtx);
uint32_t updateProtocolCalcCrc16(const uint8_t *buf, uint32_t len, void *userCtx);
bool updateProtocolEncodePayload(eUpdateProtocolTransport transport,
                                 uint8_t cmd,
                                 const uint8_t *payload,
                                 uint16_t payloadLength,
                                 uint8_t *encodedPayload,
                                 uint16_t encodedPayloadSize,
                                 uint16_t *encodedPayloadLength);
bool updateProtocolHandlePkt(const stFrmPsrPkt *pkt, eUpdateProtocolTransport transport, stUpdateProtocolReply *reply);
const stUpdateProtocolVersion *updateProtocolGetSupportedVersion(void);
const stUpdateProtocolOtaRequestPayload *updateProtocolGetDefaultOtaRequest(void);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_UPDATE_PROTOCOL_H
/**************************End of file********************************/
