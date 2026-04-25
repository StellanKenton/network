/************************************************************************************
* @file     : cprsensor_protocol.h
* @brief    : CPR sensor BLE protocol helpers.
* @details  : Defines on-wire payload layouts and provides frame packing helpers
*             for the project-side BLE protocol summary.
* @author   : GitHub Copilot
* @date     : 2026-04-24
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_MANAGER_IOTMANAGER_CPRSENSOR_PROTOCOL_H
#define NETWORK_APP_MANAGER_IOTMANAGER_CPRSENSOR_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CPRSENSOR_PROTOCOL_FRAME_HEAD0               0xFAU
#define CPRSENSOR_PROTOCOL_FRAME_HEAD1               0xFCU
#define CPRSENSOR_PROTOCOL_FRAME_VERSION             0x01U
#define CPRSENSOR_PROTOCOL_FRAME_HEAD_SIZE           6U
#define CPRSENSOR_PROTOCOL_FRAME_TAIL_SIZE           2U
#define CPRSENSOR_PROTOCOL_FRAME_OVERHEAD            8U
#define CPRSENSOR_PROTOCOL_PAYLOAD_LEN_NONE          0U
#define CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE            16U

#define CPRSENSOR_PROTOCOL_MAC_LEN                   6U
#define CPRSENSOR_PROTOCOL_DEVICE_SN_LEN             13U
#define CPRSENSOR_PROTOCOL_BLE_VERSION_LEN           33U
#define CPRSENSOR_PROTOCOL_UTC_OFFSET_LEN            6U
#define CPRSENSOR_PROTOCOL_RAW_WAVE_LEN              8U

#define CPRSENSOR_PROTOCOL_HEARTBEAT_PAYLOAD_LEN     0U
#define CPRSENSOR_PROTOCOL_DISCONNECT_PAYLOAD_LEN    0U
#define CPRSENSOR_PROTOCOL_DEV_INFO_REQ_LEN          0U
#define CPRSENSOR_PROTOCOL_BLE_INFO_REQ_LEN          0U
#define CPRSENSOR_PROTOCOL_BATTERY_REQ_LEN           0U
#define CPRSENSOR_PROTOCOL_CLEAR_MEMORY_REQ_LEN      0U

typedef enum eCprsensorProtocolCmd {
	CPRSENSOR_PROTOCOL_CMD_HANDSHAKE = 0x01,
	CPRSENSOR_PROTOCOL_CMD_HEARTBEAT = 0x03,
	CPRSENSOR_PROTOCOL_CMD_DISCONNECT = 0x04,
	CPRSENSOR_PROTOCOL_CMD_SELF_CHECK = 0x05,
	CPRSENSOR_PROTOCOL_CMD_DEV_INFO = 0x11,
	CPRSENSOR_PROTOCOL_CMD_BLE_INFO = 0x13,
	CPRSENSOR_PROTOCOL_CMD_WIFI_SETTING = 0x14,
	CPRSENSOR_PROTOCOL_CMD_COMM_SETTING = 0x15,
	CPRSENSOR_PROTOCOL_CMD_TCP_SETTING = 0x16,
	CPRSENSOR_PROTOCOL_CMD_UPLOAD_METHOD = 0x30,
	CPRSENSOR_PROTOCOL_CMD_CPR_DATA = 0x31,
	CPRSENSOR_PROTOCOL_CMD_TIME_SYNC = 0x33,
	CPRSENSOR_PROTOCOL_CMD_BATTERY = 0x34,
	CPRSENSOR_PROTOCOL_CMD_LANGUAGE = 0x35,
	CPRSENSOR_PROTOCOL_CMD_VOLUME = 0x36,
	CPRSENSOR_PROTOCOL_CMD_CPR_RAW_DATA = 0x37,
	CPRSENSOR_PROTOCOL_CMD_CLEAR_MEMORY = 0x38,
	CPRSENSOR_PROTOCOL_CMD_BOOT_TIME = 0x39,
	CPRSENSOR_PROTOCOL_CMD_METRONOME = 0x3A,
	CPRSENSOR_PROTOCOL_CMD_UTC_SETTING = 0x3B,
	CPRSENSOR_PROTOCOL_CMD_LOG_DATA = 0x40,
	CPRSENSOR_PROTOCOL_CMD_HISTORY_DATA = 0x41,
} eCprsensorProtocolCmd;

typedef enum eCprsensorProtocolReplySlot {
	CPRSENSOR_PROTOCOL_REPLY_SLOT_HANDSHAKE = 0,
	CPRSENSOR_PROTOCOL_REPLY_SLOT_HEARTBEAT,
	CPRSENSOR_PROTOCOL_REPLY_SLOT_DISCONNECT,
	CPRSENSOR_PROTOCOL_REPLY_SLOT_DEV_INFO,
	CPRSENSOR_PROTOCOL_REPLY_SLOT_BLE_INFO,
	CPRSENSOR_PROTOCOL_REPLY_SLOT_WIFI_SETTING,
	CPRSENSOR_PROTOCOL_REPLY_SLOT_BATTERY,
	CPRSENSOR_PROTOCOL_REPLY_SLOT_LANGUAGE,
	CPRSENSOR_PROTOCOL_REPLY_SLOT_VOLUME,
	CPRSENSOR_PROTOCOL_REPLY_SLOT_METRONOME,
	CPRSENSOR_PROTOCOL_REPLY_SLOT_UTC_SETTING,
	CPRSENSOR_PROTOCOL_REPLY_SLOT_MAX,
} eCprsensorProtocolReplySlot;

typedef enum eCprsensorProtocolCommPriority {
	CPRSENSOR_PROTOCOL_COMM_PRIORITY_BLE = 0,
	CPRSENSOR_PROTOCOL_COMM_PRIORITY_WIFI = 1,
} eCprsensorProtocolCommPriority;

typedef enum eCprsensorProtocolStatus {
	CPRSENSOR_PROTOCOL_STATUS_OK = 0,
	CPRSENSOR_PROTOCOL_STATUS_ERROR_PARAM,
	CPRSENSOR_PROTOCOL_STATUS_ERROR_LENGTH,
	CPRSENSOR_PROTOCOL_STATUS_ERROR_BUFFER,
	CPRSENSOR_PROTOCOL_STATUS_ERROR_HEADER,
	CPRSENSOR_PROTOCOL_STATUS_ERROR_VERSION,
	CPRSENSOR_PROTOCOL_STATUS_ERROR_FRAME_LEN,
	CPRSENSOR_PROTOCOL_STATUS_ERROR_CRC,
	CPRSENSOR_PROTOCOL_STATUS_ERROR_CIPHER,
} eCprsensorProtocolStatus;

typedef bool (*pfCprsensorProtocolCipher)(void *userData, uint8_t *buffer, uint16_t length);

typedef struct stCprsensorProtocolCrcCfg {
	uint16_t polynomial;
	uint16_t initValue;
	uint16_t xorOut;
	bool reflectInput;
	bool reflectOutput;
} stCprsensorProtocolCrcCfg;

typedef struct stCprsensorProtocolCipherCfg {
	bool enabled;
	uint8_t blockSize;
	pfCprsensorProtocolCipher encrypt;
	pfCprsensorProtocolCipher decrypt;
	void *userData;
} stCprsensorProtocolCipherCfg;

typedef struct stCprsensorProtocolCodecCfg {
	stCprsensorProtocolCrcCfg crc;
	stCprsensorProtocolCipherCfg cipher;
} stCprsensorProtocolCodecCfg;

typedef struct stCprsensorProtocolFrameHead {
	uint8_t head0;
	uint8_t head1;
	uint8_t version;
	uint8_t cmd;
	uint8_t dataLenBe[2];
} stCprsensorProtocolFrameHead;

typedef struct stCprsensorProtocolFrameView {
	eCprsensorProtocolCmd cmd;
	uint8_t version;
	const uint8_t *encodedPayload;
	uint16_t encodedPayloadLen;
	const uint8_t *payload;
	uint16_t payloadLen;
	uint16_t crc16;
} stCprsensorProtocolFrameView;

typedef struct stCprsensorProtocolHistorySubRecordHead {
	uint8_t recordLen;
	uint8_t recordCmd;
} stCprsensorProtocolHistorySubRecordHead;

typedef struct stCprsensorProtocolHistorySubRecordView {
	eCprsensorProtocolCmd cmd;
	const uint8_t *recordData;
	uint8_t recordDataLen;
	uint8_t recordLen;
} stCprsensorProtocolHistorySubRecordView;

typedef struct stCprsensorProtocolHandshakePayload {
	uint8_t mac[CPRSENSOR_PROTOCOL_MAC_LEN];
} stCprsensorProtocolHandshakePayload;

typedef stCprsensorProtocolHandshakePayload stCprsensorProtocolHandshakeReplyPayload;

typedef struct stCprsensorProtocolWifiSettingPayloadHead {
	uint8_t ssidLen;
} stCprsensorProtocolWifiSettingPayloadHead;

typedef stCprsensorProtocolWifiSettingPayloadHead stCprsensorProtocolWifiSettingReplyPayloadHead;

typedef struct stCprsensorProtocolCommSettingPayload {
	uint8_t priority;
} stCprsensorProtocolCommSettingPayload;

typedef stCprsensorProtocolCommSettingPayload stCprsensorProtocolCommSettingReplyPayload;

typedef struct stCprsensorProtocolTcpSettingPayloadHead {
	uint8_t ipLen;
} stCprsensorProtocolTcpSettingPayloadHead;

typedef struct stCprsensorProtocolUploadMethodPayload {
	uint8_t uploadMethod;
} stCprsensorProtocolUploadMethodPayload;

typedef struct stCprsensorProtocolTimeSyncPayload {
	uint8_t worldTimeBe[4];
} stCprsensorProtocolTimeSyncPayload;

typedef stCprsensorProtocolTimeSyncPayload stCprsensorProtocolTimeSyncReplyPayload;

typedef struct stCprsensorProtocolLanguagePayload {
	uint8_t language;
} stCprsensorProtocolLanguagePayload;

typedef stCprsensorProtocolLanguagePayload stCprsensorProtocolLanguageReplyPayload;

typedef struct stCprsensorProtocolVolumePayload {
	uint8_t volume;
} stCprsensorProtocolVolumePayload;

typedef stCprsensorProtocolVolumePayload stCprsensorProtocolVolumeReplyPayload;

typedef struct stCprsensorProtocolMetronomePayload {
	uint8_t metronomeFreq;
} stCprsensorProtocolMetronomePayload;

typedef stCprsensorProtocolMetronomePayload stCprsensorProtocolMetronomeReplyPayload;

typedef struct stCprsensorProtocolUtcSettingPayload {
	char utcOffset[CPRSENSOR_PROTOCOL_UTC_OFFSET_LEN];
} stCprsensorProtocolUtcSettingPayload;

typedef stCprsensorProtocolUtcSettingPayload stCprsensorProtocolUtcSettingReplyPayload;

typedef struct stCprsensorProtocolSelfCheckReplyPayload {
	uint8_t feedbackSelfCheck;
	uint8_t powerSelfCheck;
	uint8_t audioSelfCheck;
	uint8_t wirelessSelfCheck;
	uint8_t memorySelfCheck;
	uint8_t timestampBe[4];
} stCprsensorProtocolSelfCheckReplyPayload;

typedef struct stCprsensorProtocolDevInfoReplyPayload {
	uint8_t deviceType;
	uint8_t deviceSn[CPRSENSOR_PROTOCOL_DEVICE_SN_LEN];
	uint8_t protocolOrFlag;
	uint8_t swVersion;
	uint8_t swSubVersion;
	uint8_t swBuildVersion;
} stCprsensorProtocolDevInfoReplyPayload;

typedef struct stCprsensorProtocolBleInfoReplyPayload {
	uint8_t bleVersion[CPRSENSOR_PROTOCOL_BLE_VERSION_LEN];
} stCprsensorProtocolBleInfoReplyPayload;

typedef struct stCprsensorProtocolUploadMethodReplyPayload {
	uint8_t uploadStatus;
} stCprsensorProtocolUploadMethodReplyPayload;

typedef struct stCprsensorProtocolCprDataReplyPayload {
	uint8_t timestampBe[4];
	uint8_t freqBe[2];
	uint8_t depth;
	uint8_t realseDepth;
	uint8_t interval;
	uint8_t bootTimestampBe[4];
} stCprsensorProtocolCprDataReplyPayload;

typedef struct stCprsensorProtocolBatteryReplyPayload {
	uint8_t batPercent;
	uint8_t batMvBe[2];
	uint8_t chargeState;
} stCprsensorProtocolBatteryReplyPayload;

typedef struct stCprsensorProtocolCprRawDataPayload {
	uint8_t rawWave[CPRSENSOR_PROTOCOL_RAW_WAVE_LEN];
} stCprsensorProtocolCprRawDataPayload;

typedef struct stCprsensorProtocolClearMemoryReplyPayload {
	uint8_t clearResult;
} stCprsensorProtocolClearMemoryReplyPayload;

typedef struct stCprsensorProtocolBootTimeReplyPayload {
	uint8_t bootTimeBe[4];
} stCprsensorProtocolBootTimeReplyPayload;

typedef stCprsensorProtocolBootTimeReplyPayload stCprsensorProtocolBootTimePayload;

typedef struct stCprsensorProtocolLogReplyPayload {
	uint8_t timestampBe[4];
	uint8_t chargeStatus;
	uint8_t dcVoltageDiv10;
	uint8_t batVoltageDiv10;
	uint8_t v5VoltageDiv10;
	uint8_t v33VoltageDiv10;
	uint8_t bleState;
	uint8_t wifiState;
	uint8_t cprState;
} stCprsensorProtocolLogReplyPayload;

typedef struct stCprsensorProtocolHistoryTimeSyncRecord {
	uint8_t recordLen;
	uint8_t recordCmd;
	uint8_t timestampBe[4];
} stCprsensorProtocolHistoryTimeSyncRecord;

typedef struct stCprsensorProtocolHistoryBootTimeRecord {
	uint8_t recordLen;
	uint8_t recordCmd;
	uint8_t timestampBe[4];
} stCprsensorProtocolHistoryBootTimeRecord;

typedef struct stCprsensorProtocolHistoryCprRecord {
	uint8_t recordLen;
	uint8_t recordCmd;
	uint8_t timestampBe[4];
	uint8_t freqBe[2];
	uint8_t depth;
	uint8_t realseDepth;
	uint8_t interval;
	uint8_t bootTimestampBe[4];
} stCprsensorProtocolHistoryCprRecord;

typedef struct stCprsensorProtocolHistorySelfCheckRecord {
	uint8_t recordLen;
	uint8_t recordCmd;
	uint8_t feedbackSelfCheck;
	uint8_t powerSelfCheck;
	uint8_t audioSelfCheck;
	uint8_t wirelessSelfCheck;
	uint8_t memorySelfCheck;
	uint8_t timestampBe[4];
} stCprsensorProtocolHistorySelfCheckRecord;

typedef struct stCprsensorProtocolHistoryLogRecord {
	uint8_t recordLen;
	uint8_t recordCmd;
	uint8_t timestampBe[4];
	uint8_t chargeStatus;
	uint8_t dcVoltageDiv10;
	uint8_t batVoltageDiv10;
	uint8_t v5VoltageDiv10;
	uint8_t v33VoltageDiv10;
	uint8_t bleState;
	uint8_t wifiState;
	uint8_t cprState;
} stCprsensorProtocolHistoryLogRecord;

uint16_t cprsensorProtocolReadU16Be(const uint8_t *buffer);
uint16_t cprsensorProtocolReadU16Le(const uint8_t *buffer);
uint32_t cprsensorProtocolReadU32Be(const uint8_t *buffer);
uint32_t cprsensorProtocolReadU32Le(const uint8_t *buffer);

void cprsensorProtocolWriteU16Be(uint8_t *buffer, uint16_t value);
void cprsensorProtocolWriteU16Le(uint8_t *buffer, uint16_t value);
void cprsensorProtocolWriteU32Be(uint8_t *buffer, uint32_t value);
void cprsensorProtocolWriteU32Le(uint8_t *buffer, uint32_t value);

uint16_t cprsensorProtocolAlignLength(uint16_t length, uint8_t alignSize);
uint16_t cprsensorProtocolGetFrameLength(uint16_t encodedPayloadLen);
uint16_t cprsensorProtocolCrc16Calculate(const uint8_t *data, uint16_t length, const stCprsensorProtocolCrcCfg *crcCfg);

eCprsensorProtocolStatus cprsensorProtocolTryGetFrameLength(const uint8_t *buffer, uint16_t bufferLen, uint16_t *frameLen);
eCprsensorProtocolStatus cprsensorProtocolParseFrame(const uint8_t *buffer,
					  uint16_t bufferLen,
					  const stCprsensorProtocolCodecCfg *codecCfg,
					  uint8_t *payloadBuffer,
					  uint16_t payloadBufferSize,
					  stCprsensorProtocolFrameView *frameView);
eCprsensorProtocolStatus cprsensorProtocolPackFrame(eCprsensorProtocolCmd cmd,
					 const uint8_t *payload,
					 uint16_t payloadLen,
					 const stCprsensorProtocolCodecCfg *codecCfg,
					 uint8_t *frameBuffer,
					 uint16_t frameCapacity,
					 uint16_t *frameLen);

eCprsensorProtocolStatus cprsensorProtocolPackHistorySubRecord(eCprsensorProtocolCmd recordCmd,
						 const uint8_t *recordData,
						 uint8_t recordDataLen,
						 uint8_t *buffer,
						 uint16_t bufferCapacity,
						 uint16_t *recordLen);
eCprsensorProtocolStatus cprsensorProtocolParseHistorySubRecord(const uint8_t *buffer,
						  uint16_t bufferLen,
						  stCprsensorProtocolHistorySubRecordView *recordView);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
