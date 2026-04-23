/***********************************************************************************
* @file     : update_mgr.c
* @brief    : Update manager adapter implementation.
* @details  : Bridges project-specific watchdog and jump handling to the reusable
*             rep update service.
* @author   : GitHub Copilot
* @date     : 2026-04-16
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "update_mgr.h"

#include <stddef.h>
#include <string.h>

#include "stm32f10x.h"

#include "../../port/update_port.h"
#include "../../../rep/driver/drvmcuflash/drvmcuflash.h"
#include "../../../rep/service/log/log.h"

#define UPDATE_MANAGER_LOG_TAG             "UpdateMgr"
#define UPDATE_APP_VECTOR_STACK_MASK       0x2FFE0000UL
#define UPDATE_APP_VECTOR_STACK_BASE       0x20000000UL
#define UPDATE_APP_VECTOR_ENTRY_MASK       0xFFFFFFFEUL
#define UPDATE_NVIC_REGISTER_COUNT         8U
#define UPDATE_MANAGER_META_SLOT_COUNT     2U
#define UPDATE_MANAGER_VERIFY_CHUNK_SIZE   256U

typedef struct stUpdateManagerOtaSession {
    stUpdateProtocolVersion version;
    uint16_t appliedPacketLength;
    uint32_t imageSize;
    uint32_t imageCrc32;
    uint32_t writeOffset;
    uint32_t headerSequence;
    bool isPrepared;
} stUpdateManagerOtaSession;

static stUpdateManagerOtaSession gUpdateManagerOtaSession;

static bool updateManagerCanBootNormally(const stUpdateBootRecord *record);
static void updateManagerResetOtaSession(void);
static uint16_t updateManagerClampPacketLength(uint16_t preferredPacketLength);
static bool updateManagerGetRegionCfg(uint8_t regionId, stUpdateRegionCfg *cfg);
static const stUpdateStorageOps *updateManagerGetRegionOps(uint8_t regionId, stUpdateRegionCfg *cfg);
static bool updateManagerStorageRead(uint8_t regionId, uint32_t offset, uint8_t *buffer, uint32_t length);
static bool updateManagerStorageWrite(uint8_t regionId, uint32_t offset, const uint8_t *buffer, uint32_t length);
static bool updateManagerStorageErase(uint8_t regionId, uint32_t offset, uint32_t length);
static uint32_t updateManagerCrc32Update(uint32_t crc, const uint8_t *data, uint32_t length);
static uint32_t updateManagerCrc32Finalize(uint32_t crc);
static uint32_t updateManagerCalcImageHeaderCrc(const stUpdateImageHeader *header);
static uint32_t updateManagerGetNextMetaSequence(uint32_t currentSequence);
static uint32_t updateManagerPackVersion(const stUpdateProtocolVersion *version);
static bool updateManagerMetaCommit(uint8_t regionId, const void *payload, uint32_t payloadSize, uint32_t *sequenceInOut);
static void updateManagerBuildStagingHeader(stUpdateImageHeader *header, uint32_t writeOffset, uint32_t imageState);
static bool updateManagerStoreStagingHeader(uint32_t writeOffset, uint32_t imageState);
static bool updateManagerVerifyPreparedImage(uint32_t *imageCrc32Out);
static void updateManagerLogCrcCheck(const char *stageName, uint32_t actualCrc32, uint32_t expectedCrc32, bool passed);
static bool updateManagerGetExecutableRegion(uint8_t regionId, stUpdateRegionCfg *cfg);
static bool updateManagerReadAppVector(const stUpdateRegionCfg *cfg, uint32_t vector[2]);
static bool updateManagerJumpToRegionInternal(uint8_t regionId);
static void updateManagerPrepareAppJump(uint32_t vectorBase);
static __ASM void updateManagerLaunchApp(uint32_t stackPointer, uint32_t resetHandler);

static void updateManagerResetOtaSession(void)
{
    uint16_t lPacketLength;

    lPacketLength = gUpdateManagerOtaSession.appliedPacketLength;
    (void)memset(&gUpdateManagerOtaSession, 0, sizeof(gUpdateManagerOtaSession));
    if (lPacketLength == 0U) {
        lPacketLength = UPDATE_PROTOCOL_OTA_PACKET_LEN_DEFAULT;
    }
    gUpdateManagerOtaSession.appliedPacketLength = lPacketLength;
}

static uint16_t updateManagerClampPacketLength(uint16_t preferredPacketLength)
{
    uint16_t lPacketLength;

    lPacketLength = preferredPacketLength;
    if (lPacketLength == 0U) {
        lPacketLength = UPDATE_PROTOCOL_OTA_PACKET_LEN_DEFAULT;
    }

    if (lPacketLength > UPDATE_PROTOCOL_OTA_PACKET_LEN_DEFAULT) {
        lPacketLength = UPDATE_PROTOCOL_OTA_PACKET_LEN_DEFAULT;
    }

    if (lPacketLength <= UPDATE_PROTOCOL_OTA_DATA_OVERHEAD) {
        lPacketLength = (uint16_t)(UPDATE_PROTOCOL_OTA_DATA_OVERHEAD + 1U);
    }

    return lPacketLength;
}

static bool updateManagerGetRegionCfg(uint8_t regionId, stUpdateRegionCfg *cfg)
{
    if ((cfg == NULL) || !updatePortGetRegionMap(regionId, cfg)) {
        return false;
    }

    return (cfg->size > 0U) && (cfg->storageId < E_UPDATE_STORAGE_MAX);
}

static const stUpdateStorageOps *updateManagerGetRegionOps(uint8_t regionId, stUpdateRegionCfg *cfg)
{
    if (!updateManagerGetRegionCfg(regionId, cfg)) {
        return NULL;
    }

    return updatePortGetStorageOps(cfg->storageId);
}

static bool updateManagerStorageRead(uint8_t regionId, uint32_t offset, uint8_t *buffer, uint32_t length)
{
    const stUpdateStorageOps *lpOps;
    stUpdateRegionCfg lCfg;
    uint32_t lAddress;

    if ((buffer == NULL) || (length == 0U)) {
        return false;
    }

    lpOps = updateManagerGetRegionOps(regionId, &lCfg);
    if ((lpOps == NULL) || (lpOps->read == NULL) || !lCfg.isReadable || (offset > lCfg.size) || (length > (lCfg.size - offset))) {
        return false;
    }

    lAddress = lCfg.startAddress + offset;
    if ((lpOps->isRangeValid != NULL) && !lpOps->isRangeValid(lAddress, length)) {
        return false;
    }

    return lpOps->read(lAddress, buffer, length);
}

static bool updateManagerStorageWrite(uint8_t regionId, uint32_t offset, const uint8_t *buffer, uint32_t length)
{
    const stUpdateStorageOps *lpOps;
    stUpdateRegionCfg lCfg;
    uint32_t lAddress;

    if ((buffer == NULL) || (length == 0U)) {
        return false;
    }

    lpOps = updateManagerGetRegionOps(regionId, &lCfg);
    if ((lpOps == NULL) || (lpOps->write == NULL) || !lCfg.isWritable || (offset > lCfg.size) || (length > (lCfg.size - offset))) {
        return false;
    }

    lAddress = lCfg.startAddress + offset;
    if ((lpOps->isRangeValid != NULL) && !lpOps->isRangeValid(lAddress, length)) {
        return false;
    }

    return lpOps->write(lAddress, buffer, length);
}

static bool updateManagerStorageErase(uint8_t regionId, uint32_t offset, uint32_t length)
{
    const stUpdateStorageOps *lpOps;
    stUpdateRegionCfg lCfg;
    uint32_t lAddress;

    if (length == 0U) {
        return false;
    }

    lpOps = updateManagerGetRegionOps(regionId, &lCfg);
    if ((lpOps == NULL) || (lpOps->erase == NULL) || !lCfg.isWritable || (offset > lCfg.size) || (length > (lCfg.size - offset))) {
        return false;
    }

    lAddress = lCfg.startAddress + offset;
    if ((lpOps->isRangeValid != NULL) && !lpOps->isRangeValid(lAddress, length)) {
        return false;
    }

    return lpOps->erase(lAddress, length);
}

static uint32_t updateManagerCrc32Update(uint32_t crc, const uint8_t *data, uint32_t length)
{
    uint32_t lIndex;
    uint8_t lBit;

    if ((data == NULL) && (length > 0U)) {
        return crc;
    }

    for (lIndex = 0U; lIndex < length; lIndex++) {
        crc ^= data[lIndex];
        for (lBit = 0U; lBit < 8U; lBit++) {
            if ((crc & 0x1U) != 0U) {
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            } else {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

static uint32_t updateManagerCrc32Finalize(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFUL;
}

static uint32_t updateManagerCalcImageHeaderCrc(const stUpdateImageHeader *header)
{
    if (header == NULL) {
        return 0U;
    }

    return updateManagerCrc32Finalize(updateManagerCrc32Update(0xFFFFFFFFUL,
                                                               (const uint8_t *)header,
                                                               offsetof(stUpdateImageHeader, headerCrc32)));
}

static uint32_t updateManagerGetNextMetaSequence(uint32_t currentSequence)
{
    if ((currentSequence == 0U) || (currentSequence == 0xFFFFFFFFUL)) {
        return 1U;
    }

    return currentSequence + 1U;
}

static uint32_t updateManagerPackVersion(const stUpdateProtocolVersion *version)
{
    if (version == NULL) {
        return 0U;
    }

    return ((uint32_t)version->major) |
           ((uint32_t)version->minor << 8U) |
           ((uint32_t)version->patch << 16U) |
           ((uint32_t)version->build << 24U);
}

static bool updateManagerMetaCommit(uint8_t regionId, const void *payload, uint32_t payloadSize, uint32_t *sequenceInOut)
{
    stUpdateCfg lCfg;
    stUpdateRegionCfg lRegionCfg;
    stUpdateMetaRecord lRecord;
    stUpdateMetaRecord lReadback;
    uint32_t lSlotCount;
    uint32_t lSlotIndex;
    uint32_t lActiveSlot = 0U;
    uint32_t lActiveSequence = 0U;
    uint32_t lTargetSlot;
    uint32_t lNextSequence;
    uint32_t lCommitOffset;
    bool lHasActiveSlot = false;

    if ((payload == NULL) || (sequenceInOut == NULL) || (payloadSize > sizeof(lRecord.payload)) ||
        !updatePortLoadDefaultCfg(&lCfg) || !updateManagerGetRegionCfg(regionId, &lRegionCfg) ||
        (lRegionCfg.eraseUnit == 0U) || (lRegionCfg.size < lRegionCfg.eraseUnit)) {
        return false;
    }

    lSlotCount = lRegionCfg.size / lRegionCfg.eraseUnit;
    if (lSlotCount > UPDATE_MANAGER_META_SLOT_COUNT) {
        lSlotCount = UPDATE_MANAGER_META_SLOT_COUNT;
    }
    if (lSlotCount == 0U) {
        return false;
    }

    for (lSlotIndex = 0U; lSlotIndex < lSlotCount; lSlotIndex++) {
        uint32_t lHeaderCrc;
        uint32_t lPayloadCrc;

        if (!updateManagerStorageRead(regionId, lSlotIndex * lRegionCfg.eraseUnit, (uint8_t *)&lReadback, sizeof(lReadback))) {
            continue;
        }

        lHeaderCrc = updateManagerCrc32Finalize(updateManagerCrc32Update(0xFFFFFFFFUL,
                                                                         (const uint8_t *)&lReadback,
                                                                         offsetof(stUpdateMetaRecord, headerCrc32)));
        if ((lReadback.recordMagic != lCfg.metaRecordMagic) ||
            (lReadback.commitMarker != lCfg.metaCommitMarker) ||
            (lReadback.payloadLength != payloadSize) ||
            (lReadback.headerCrc32 != lHeaderCrc)) {
            continue;
        }

        lPayloadCrc = updateManagerCrc32Finalize(updateManagerCrc32Update(0xFFFFFFFFUL,
                                                                          lReadback.payload,
                                                                          lReadback.payloadLength));
        if (lReadback.payloadCrc32 != lPayloadCrc) {
            continue;
        }

        if (!lHasActiveSlot || (lReadback.sequence >= lActiveSequence)) {
            lHasActiveSlot = true;
            lActiveSlot = lSlotIndex;
            lActiveSequence = lReadback.sequence;
        }
    }

    lTargetSlot = (lHasActiveSlot && (lSlotCount > 1U)) ? ((lActiveSlot + 1U) % lSlotCount) : 0U;
    lNextSequence = updateManagerGetNextMetaSequence(*sequenceInOut);

    (void)memset(&lRecord, 0xFF, sizeof(lRecord));
    lRecord.recordMagic = lCfg.metaRecordMagic;
    lRecord.sequence = lNextSequence;
    lRecord.payloadLength = payloadSize;
    (void)memcpy(lRecord.payload, payload, payloadSize);
    lRecord.payloadCrc32 = updateManagerCrc32Finalize(updateManagerCrc32Update(0xFFFFFFFFUL,
                                                                               lRecord.payload,
                                                                               payloadSize));
    lRecord.headerCrc32 = updateManagerCrc32Finalize(updateManagerCrc32Update(0xFFFFFFFFUL,
                                                                              (const uint8_t *)&lRecord,
                                                                              offsetof(stUpdateMetaRecord, headerCrc32)));

    if (!updateManagerStorageErase(regionId, lTargetSlot * lRegionCfg.eraseUnit, lRegionCfg.eraseUnit)) {
        return false;
    }

    if (!updateManagerStorageWrite(regionId,
                                   lTargetSlot * lRegionCfg.eraseUnit,
                                   (const uint8_t *)&lRecord,
                                   offsetof(stUpdateMetaRecord, commitMarker))) {
        return false;
    }

    if (!updateManagerStorageRead(regionId,
                                  lTargetSlot * lRegionCfg.eraseUnit,
                                  (uint8_t *)&lReadback,
                                  offsetof(stUpdateMetaRecord, commitMarker))) {
        return false;
    }

    if ((lReadback.recordMagic != lRecord.recordMagic) ||
        (lReadback.sequence != lRecord.sequence) ||
        (lReadback.payloadLength != lRecord.payloadLength) ||
        (lReadback.payloadCrc32 != lRecord.payloadCrc32) ||
        (lReadback.headerCrc32 != lRecord.headerCrc32)) {
        return false;
    }

    lCommitOffset = (lTargetSlot * lRegionCfg.eraseUnit) + offsetof(stUpdateMetaRecord, commitMarker);
    if (!updateManagerStorageWrite(regionId,
                                   lCommitOffset,
                                   (const uint8_t *)&lCfg.metaCommitMarker,
                                   sizeof(lCfg.metaCommitMarker))) {
        return false;
    }

    *sequenceInOut = lNextSequence;
    return true;
}

static void updateManagerBuildStagingHeader(stUpdateImageHeader *header, uint32_t writeOffset, uint32_t imageState)
{
    if (header == NULL) {
        return;
    }

    (void)memset(header, 0, sizeof(*header));
    header->magic = UPDATE_IMAGE_MAGIC;
    header->headerVersion = UPDATE_HEADER_VERSION;
    header->imageType = (uint32_t)E_UPDATE_IMAGE_TYPE_APP;
    header->imageVersion = updateManagerPackVersion(&gUpdateManagerOtaSession.version);
    header->imageSize = gUpdateManagerOtaSession.imageSize;
    header->imageCrc32 = gUpdateManagerOtaSession.imageCrc32;
    header->writeOffset = writeOffset;
    header->imageState = imageState;
    header->headerCrc32 = updateManagerCalcImageHeaderCrc(header);
}

static bool updateManagerStoreStagingHeader(uint32_t writeOffset, uint32_t imageState)
{
    stUpdateImageHeader lHeader;

    if (!gUpdateManagerOtaSession.isPrepared) {
        return false;
    }

    updateManagerBuildStagingHeader(&lHeader, writeOffset, imageState);
    return updateManagerMetaCommit(E_UPDATE_REGION_STAGING_APP_HEADER,
                                   &lHeader,
                                   sizeof(lHeader),
                                   &gUpdateManagerOtaSession.headerSequence);
}

static bool updateManagerVerifyPreparedImage(uint32_t *imageCrc32Out)
{
    uint8_t lBuffer[UPDATE_MANAGER_VERIFY_CHUNK_SIZE];
    uint32_t lOffset = 0U;
    uint32_t lChunkSize;
    uint32_t lCrc = 0xFFFFFFFFUL;

    if (!gUpdateManagerOtaSession.isPrepared || (gUpdateManagerOtaSession.imageSize == 0U)) {
        return false;
    }

    while (lOffset < gUpdateManagerOtaSession.imageSize) {
        lChunkSize = gUpdateManagerOtaSession.imageSize - lOffset;
        if (lChunkSize > sizeof(lBuffer)) {
            lChunkSize = sizeof(lBuffer);
        }

        if (!updateManagerStorageRead(E_UPDATE_REGION_STAGING_APP, lOffset, lBuffer, lChunkSize)) {
            return false;
        }

        lCrc = updateManagerCrc32Update(lCrc, lBuffer, lChunkSize);
        lOffset += lChunkSize;
    }

    lCrc = updateManagerCrc32Finalize(lCrc);
    if (imageCrc32Out != NULL) {
        *imageCrc32Out = lCrc;
    }
    return true;
}

static void updateManagerLogCrcCheck(const char *stageName, uint32_t actualCrc32, uint32_t expectedCrc32, bool passed)
{
    if (stageName == NULL) {
        return;
    }

    if (passed) {
        LOG_I(UPDATE_MANAGER_LOG_TAG,
              "%s crc32: actual=0x%08lX expected=0x%08lX result=PASS",
              stageName,
              (unsigned long)actualCrc32,
              (unsigned long)expectedCrc32);
    } else {
        LOG_W(UPDATE_MANAGER_LOG_TAG,
              "%s crc32: actual=0x%08lX expected=0x%08lX result=FAIL",
              stageName,
              (unsigned long)actualCrc32,
              (unsigned long)expectedCrc32);
    }
}

void updateManagerReset(void)
{
    updateManagerResetOtaSession();
    updateReset();
}

bool updateManagerInit(void)
{
    updateManagerResetOtaSession();
    if (!updateInit()) {
        return false;
    }

#if (UPDATE_MANAGER_FORCE_RUN_APP_BOOT_RECORD == 1)
    if (!updateManagerSetBootRecordRunApp()) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "force run_app boot record failed");
        return false;
    }
#endif

    return true;
}

void updateManagerProcess(uint32_t nowTickMs)
{
    const stUpdateStatus *lStatus;

    updateProcess(nowTickMs);

    lStatus = updateGetStatus();
    if ((lStatus != NULL) && (lStatus->state == E_UPDATE_STATE_JUMP_TARGET)) {
        (void)updateJumpToTargetIfValid();
    }
}

const stUpdateStatus *updateManagerGetStatus(void)
{
    return updateGetStatus();
}

bool updateManagerGetBootRecord(stUpdateBootRecord *record)
{
    return updateReadBootRecord(record);
}

bool updateManagerHasNormalAppBootFlag(void)
{
    stUpdateBootRecord lRecord;

    if (!updateReadBootRecord(&lRecord)) {
        return false;
    }

    return updateManagerCanBootNormally(&lRecord);
}

bool updateManagerJumpToAppIfValid(void)
{
    return updateJumpToTargetIfValid();
}

bool updateManagerSetBootRecordRunApp(void)
{
    stUpdateBootRecord lRecord;

    if (!updateReadBootRecord(&lRecord)) {
        (void)memset(&lRecord, 0, sizeof(lRecord));
        lRecord.magic = UPDATE_BOOT_RECORD_MAGIC;
    }

    lRecord.magic = UPDATE_BOOT_RECORD_MAGIC;
    lRecord.requestFlag = (uint32_t)E_UPDATE_REQUEST_RUN_APP;
    lRecord.lastError = (uint32_t)E_UPDATE_ERROR_NONE;
    lRecord.targetRegion = (uint32_t)E_UPDATE_REGION_RUN_APP;

    if (!updateWriteBootRecord(&lRecord)) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "store run_app boot record failed");
        return false;
    }

    LOG_I(UPDATE_MANAGER_LOG_TAG, "boot record forced to run_app");
    return true;
}

bool updateManagerBuildOtaRequestReply(const stUpdateProtocolOtaRequestPayload *request,
                                       stUpdateProtocolOtaRequestReplyPayload *reply)
{
    const stUpdateProtocolVersion *lpVersion;
    uint16_t lPreferredPacketLength;
    uint16_t lAppliedPacketLength;

    if (reply == NULL) {
        return false;
    }

    lPreferredPacketLength = (request != NULL) ? request->preferredPacketLength : 0U;
    lAppliedPacketLength = updateManagerClampPacketLength(lPreferredPacketLength);
    lpVersion = updateProtocolGetSupportedVersion();

    (void)memset(reply, 0, sizeof(*reply));
    reply->otaFlag = 0x01U;
    if (lpVersion != NULL) {
        reply->version = *lpVersion;
    }
    reply->maxPacketLength = UPDATE_PROTOCOL_OTA_PACKET_LEN_DEFAULT;
    reply->appliedPacketLength = lAppliedPacketLength;
    gUpdateManagerOtaSession.appliedPacketLength = lAppliedPacketLength;
    return true;
}

bool updateManagerPrepareOtaImage(const stUpdateProtocolOtaFileInfoPayload *fileInfo,
                                  stUpdateProtocolOtaFileInfoReplyPayload *reply)
{
    stUpdateRegionCfg lStagingCfg;
    stUpdateRegionCfg lHeaderCfg;

    if ((fileInfo == NULL) || (reply == NULL)) {
        return false;
    }

    (void)memset(reply, 0, sizeof(*reply));
    reply->state = (uint8_t)E_UPDATE_PROTOCOL_OTA_FILE_INFO_ACCEPT;

    if (!updateManagerGetRegionCfg(E_UPDATE_REGION_STAGING_APP, &lStagingCfg) ||
        !updateManagerGetRegionCfg(E_UPDATE_REGION_STAGING_APP_HEADER, &lHeaderCfg) ||
        (fileInfo->fileSize == 0U) || (fileInfo->fileSize > lStagingCfg.size)) {
        reply->state = (uint8_t)E_UPDATE_PROTOCOL_OTA_FILE_INFO_TOO_LARGE;
        return true;
    }

    updateManagerResetOtaSession();
    gUpdateManagerOtaSession.appliedPacketLength = updateManagerClampPacketLength(gUpdateManagerOtaSession.appliedPacketLength);
    gUpdateManagerOtaSession.version = fileInfo->version;
    gUpdateManagerOtaSession.imageSize = fileInfo->fileSize;
    gUpdateManagerOtaSession.imageCrc32 = fileInfo->fileCrc32;
    gUpdateManagerOtaSession.writeOffset = 0U;
    gUpdateManagerOtaSession.headerSequence = 0U;
    gUpdateManagerOtaSession.isPrepared = true;

    if (!updateManagerStorageErase(E_UPDATE_REGION_STAGING_APP_HEADER, 0U, lHeaderCfg.size) ||
        !updateManagerStorageErase(E_UPDATE_REGION_STAGING_APP, 0U, fileInfo->fileSize) ||
        !updateManagerStoreStagingHeader(0U, (uint32_t)E_UPDATE_IMAGE_STATE_RECEIVING)) {
        gUpdateManagerOtaSession.isPrepared = false;
        reply->state = (uint8_t)E_UPDATE_PROTOCOL_OTA_FILE_INFO_TOO_LARGE;
        return true;
    }

    return true;
}

bool updateManagerGetOtaOffset(const stUpdateProtocolOtaOffsetPayload *request,
                               stUpdateProtocolOtaOffsetReplyPayload *reply)
{
    (void)request;

    if (reply == NULL) {
        return false;
    }

    reply->offset = gUpdateManagerOtaSession.isPrepared ? gUpdateManagerOtaSession.writeOffset : 0U;
    return true;
}

bool updateManagerWriteOtaData(const stUpdateProtocolOtaDataPayload *payload,
                               const uint8_t *data,
                               uint16_t actualDataLength,
                               stUpdateProtocolOtaDataReplyPayload *reply)
{
    uint16_t lChunkSize;
    uint32_t lExpectedPacketNo;
    uint16_t lDataCrc16;

    if ((payload == NULL) || (reply == NULL) || (data == NULL)) {
        return false;
    }

    reply->state = (uint8_t)E_UPDATE_PROTOCOL_OTA_DATA_OTHER_ERROR;
    if (!gUpdateManagerOtaSession.isPrepared) {
        return true;
    }

    lChunkSize = (gUpdateManagerOtaSession.appliedPacketLength > UPDATE_PROTOCOL_OTA_DATA_OVERHEAD) ?
                 (uint16_t)(gUpdateManagerOtaSession.appliedPacketLength - UPDATE_PROTOCOL_OTA_DATA_OVERHEAD) :
                 0U;
    if (lChunkSize == 0U) {
        return true;
    }

    lExpectedPacketNo = gUpdateManagerOtaSession.writeOffset / lChunkSize;
    if ((uint32_t)payload->packetNo != lExpectedPacketNo) {
        reply->state = (uint8_t)E_UPDATE_PROTOCOL_OTA_DATA_PACKET_INDEX_ERROR;
        return true;
    }

    if ((payload->dataLength == 0U) || (payload->dataLength != actualDataLength) ||
        (payload->dataLength > lChunkSize) ||
        (gUpdateManagerOtaSession.writeOffset > gUpdateManagerOtaSession.imageSize) ||
        (payload->dataLength > (gUpdateManagerOtaSession.imageSize - gUpdateManagerOtaSession.writeOffset))) {
        reply->state = (uint8_t)E_UPDATE_PROTOCOL_OTA_DATA_LENGTH_ERROR;
        return true;
    }

    lDataCrc16 = (uint16_t)updateProtocolCalcCrc16(data, actualDataLength, NULL);
    if (lDataCrc16 != payload->dataCrc16) {
        reply->state = (uint8_t)E_UPDATE_PROTOCOL_OTA_DATA_CRC16_ERROR;
        return true;
    }

    if (!updateManagerStorageWrite(E_UPDATE_REGION_STAGING_APP,
                                   gUpdateManagerOtaSession.writeOffset,
                                   data,
                                   actualDataLength)) {
        return true;
    }

    gUpdateManagerOtaSession.writeOffset += actualDataLength;
    if (!updateManagerStoreStagingHeader(gUpdateManagerOtaSession.writeOffset,
                                         (uint32_t)E_UPDATE_IMAGE_STATE_RECEIVING)) {
        return true;
    }

    reply->state = (uint8_t)E_UPDATE_PROTOCOL_OTA_DATA_ACCEPT;
    return true;
}

bool updateManagerFinalizeOtaImage(stUpdateProtocolOtaResultReplyPayload *reply)
{
    uint32_t lImageCrc32 = 0U;
    bool lVerifyPassed;

    if (reply == NULL) {
        return false;
    }

    reply->state = (uint8_t)E_UPDATE_PROTOCOL_OTA_RESULT_LENGTH_ERROR;
    if (!gUpdateManagerOtaSession.isPrepared) {
        return true;
    }

    if (gUpdateManagerOtaSession.writeOffset != gUpdateManagerOtaSession.imageSize) {
        reply->state = (uint8_t)E_UPDATE_PROTOCOL_OTA_RESULT_TOTAL_LENGTH_ERROR;
        return true;
    }

    lVerifyPassed = updateManagerVerifyPreparedImage(&lImageCrc32);
    if (lVerifyPassed) {
        updateManagerLogCrcCheck("verify prepared image",
                                 lImageCrc32,
                                 gUpdateManagerOtaSession.imageCrc32,
                                 lImageCrc32 == gUpdateManagerOtaSession.imageCrc32);
    }

    if (!lVerifyPassed || (lImageCrc32 != gUpdateManagerOtaSession.imageCrc32)) {
        reply->state = (uint8_t)E_UPDATE_PROTOCOL_OTA_RESULT_IMAGE_VERIFY_ERROR;
        return true;
    }

    if (!updateManagerStoreStagingHeader(gUpdateManagerOtaSession.writeOffset,
                                         (uint32_t)E_UPDATE_IMAGE_STATE_READY) ||
        !updateRequestProgramRegion((uint32_t)E_UPDATE_REGION_RUN_APP)) {
        reply->state = (uint8_t)E_UPDATE_PROTOCOL_OTA_RESULT_IMAGE_VERIFY_ERROR;
        return true;
    }

    reply->state = (uint8_t)E_UPDATE_PROTOCOL_OTA_RESULT_ACCEPT;
    updateManagerResetOtaSession();
    return true;
}

static bool updateManagerCanBootNormally(const stUpdateBootRecord *record)
{
    if (record == NULL) {
        return false;
    }

    if (record->magic != UPDATE_BOOT_RECORD_MAGIC) {
        return false;
    }

    return (record->requestFlag == (uint32_t)E_UPDATE_REQUEST_IDLE) ||
           (record->requestFlag == (uint32_t)E_UPDATE_REQUEST_RUN_APP) ||
           (record->requestFlag == (uint32_t)E_UPDATE_REQUEST_FAILED);
}

static bool updateManagerGetExecutableRegion(uint8_t regionId, stUpdateRegionCfg *cfg)
{
    if ((cfg == NULL) || !updatePortGetRegionMap(regionId, cfg)) {
        return false;
    }

    return cfg->isExecutable && (cfg->storageId == E_UPDATE_STORAGE_INTERNAL_FLASH) && (cfg->size >= 8U);
}

static bool updateManagerReadAppVector(const stUpdateRegionCfg *cfg, uint32_t vector[2])
{
    if ((cfg == NULL) || (vector == NULL)) {
        return false;
    }

    return drvMcuFlashRead(cfg->startAddress, (uint8_t *)vector, sizeof(uint32_t) * 2U);
}

static bool updateManagerJumpToRegionInternal(uint8_t regionId)
{
    stUpdateRegionCfg lRegionCfg;
    uint32_t lVector[2] = {0U, 0U};
    uint32_t lResetHandler;

    if (!updateManagerGetExecutableRegion(regionId, &lRegionCfg)) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "jump region invalid, region=%u", (unsigned int)regionId);
        return false;
    }

    if (!updateManagerReadAppVector(&lRegionCfg, lVector)) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "read app vector failed, base=0x%08lX", (unsigned long)lRegionCfg.startAddress);
        return false;
    }

    if ((lVector[0] & UPDATE_APP_VECTOR_STACK_MASK) != UPDATE_APP_VECTOR_STACK_BASE) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "invalid app stack pointer: 0x%08lX", (unsigned long)lVector[0]);
        return false;
    }

    if ((lVector[1] & 0x1U) == 0U) {
        LOG_E(UPDATE_MANAGER_LOG_TAG, "invalid app reset vector: 0x%08lX", (unsigned long)lVector[1]);
        return false;
    }

    lResetHandler = lVector[1] & UPDATE_APP_VECTOR_ENTRY_MASK;
    if ((lResetHandler < lRegionCfg.startAddress) ||
        (lResetHandler >= (lRegionCfg.startAddress + lRegionCfg.size))) {
        LOG_E(UPDATE_MANAGER_LOG_TAG,
              "app reset vector out of range: 0x%08lX",
              (unsigned long)lResetHandler);
        return false;
    }

    LOG_I(UPDATE_MANAGER_LOG_TAG,
          "jump to region %u: sp=0x%08lX reset=0x%08lX",
          (unsigned int)regionId,
          (unsigned long)lVector[0],
          (unsigned long)lVector[1]);
    updateManagerPrepareAppJump(lRegionCfg.startAddress);
    updateManagerLaunchApp(lVector[0], lVector[1]);
    return false;
}

static void updateManagerPrepareAppJump(uint32_t vectorBase)
{
    uint32_t lIndex;

    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    for (lIndex = 0U; lIndex < UPDATE_NVIC_REGISTER_COUNT; lIndex++) {
        NVIC->ICER[lIndex] = 0xFFFFFFFFUL;
        NVIC->ICPR[lIndex] = 0xFFFFFFFFUL;
    }

    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;
    SCB->SHCSR &= ~(SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_MEMFAULTENA_Msk);
    SCB->CFSR = 0xFFFFFFFFUL;
    SCB->HFSR = 0xFFFFFFFFUL;
    SCB->DFSR = 0xFFFFFFFFUL;
    SCB->AFSR = 0xFFFFFFFFUL;

    __set_CONTROL(0U);
    __set_PSP(0U);
    __set_BASEPRI(0U);
    __set_FAULTMASK(0U);
    SCB->VTOR = vectorBase;

    __DSB();
    __ISB();
}

static __ASM void updateManagerLaunchApp(uint32_t stackPointer, uint32_t resetHandler)
{
    MSR MSP, r0
    MOVS r0, #0
    MSR PRIMASK, r0
    DSB
    ISB
    BX  r1
}

void updatePortFeedWatchdog(void)
{
    return;
}

bool updatePortJumpToRegion(uint8_t regionId)
{
    return updateManagerJumpToRegionInternal(regionId);
}

/**************************End of file********************************/
