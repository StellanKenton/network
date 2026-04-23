/***********************************************************************************
* @file     : system_storage.c
* @brief    : Project storage and VFS integration.
**********************************************************************************/
#include "system_storage.h"

#include <stdint.h>
#include <string.h>

#include "../../rep/module/w25qxxx/w25qxxx.h"
#include "../../rep/module/sdcard/sdcard.h"
#include "../../rep/service/log/log.h"
#include "../../rep/service/rtos/rtos.h"
#include "../../rep/service/vfs/vfs.h"
#include "../../rep/service/vfs/vfs_debug.h"
#include "../../rep/service/vfs/vfs_fatfs.h"
#include "../../rep/service/vfs/vfs_littlefs.h"

#define SYSTEM_STORAGE_LOG_TAG            "storage"
#define SYSTEM_STORAGE_FLASH_MOUNT_PATH   "/mem"
#define SYSTEM_STORAGE_SD_MOUNT_PATH      "/sd"
#define SYSTEM_STORAGE_W25Q_REGION_OFFSET 0UL
#define SYSTEM_STORAGE_W25Q_REGION_SIZE   (512UL * 1024UL)
#define SYSTEM_STORAGE_MOUNT_RETRY_MS     1000UL
#define SYSTEM_STORAGE_FORMAT_TASK_PRIO   14UL
#define SYSTEM_STORAGE_FORMAT_TASK_STACK  1024UL
#define SYSTEM_STORAGE_FORMAT_TASK_DELAY  100UL
#define SYSTEM_STORAGE_FORMAT_LOG_STEP    10UL

typedef struct stSystemStorageMountState {
    const char *mountPath;
    uint32_t lastAttemptTickMs;
    eVfsResult lastError;
    bool hasLastError;
    bool wasMounted;
    bool hasPresence;
    bool isPresent;
} stSystemStorageMountState;

typedef struct stSystemStorageFlashState {
    uint32_t lastProbeTickMs;
    eW25qxxxStatus lastStatus;
    bool hasLastStatus;
    bool isReady;
    uint8_t manufacturerId;
    uint8_t memoryType;
    uint8_t capacityId;
} stSystemStorageFlashState;

typedef struct stSystemStorageFormatState {
    uint32_t erasedBytes;
    uint32_t totalBytes;
    uint32_t nextLogPercent;
    eVfsResult lastError;
    bool isTaskCreated;
    volatile bool isPending;
    volatile bool isRunning;
    bool hasLastError;
} stSystemStorageFormatState;

static bool systemStorageFlashInit(void *deviceContext);
static bool systemStorageFlashRead(void *deviceContext, uint32_t address, void *buffer, uint32_t size);
static bool systemStorageFlashProg(void *deviceContext, uint32_t address, const void *buffer, uint32_t size);
static bool systemStorageFlashErase(void *deviceContext, uint32_t address, uint32_t size);
static bool systemStorageFlashSync(void *deviceContext);
static bool systemStoragePrepareFlash(eW25qxxxMapType device);
static bool systemStorageRegisterMounts(void);
static bool systemStorageEnsureFormatTask(void);
static void systemStorageFormatTask(void *argument);
static void systemStorageRequestFlashFormat(void);
static bool systemStorageIsFlashFormatActive(void);
static void systemStorageResetFormatProgress(void);
static void systemStorageLogFormatProgress(void);
static bool systemStorageCheckSdMountPresence(stSystemStorageMountState *state);
static void systemStorageTryMount(const char *mountPath);
static stSystemStorageMountState *systemStorageGetMountState(const char *mountPath);

static stVfsLittlefsContext gSystemStorageLittlefsContext;
static stVfsFatfsContext gSystemStorageFatfsContext;
static stSystemStorageFlashState gSystemStorageFlashState;
static stSystemStorageFormatState gSystemStorageFormatState;
static bool gSystemStorageMountsRegistered = false;
static bool gSystemStorageConsoleRegistered = false;
static bool gSystemStorageReady = false;
static repRtosStackType gSystemStorageFormatTaskStack[SYSTEM_STORAGE_FORMAT_TASK_STACK];
static stSystemStorageMountState gSystemStorageFlashMountState = {
    .mountPath = SYSTEM_STORAGE_FLASH_MOUNT_PATH,
};
static stSystemStorageMountState gSystemStorageSdMountState = {
    .mountPath = SYSTEM_STORAGE_SD_MOUNT_PATH,
};

static const stVfsLittlefsBlockDeviceOps gSystemStorageFlashOps = {
    .init = systemStorageFlashInit,
    .read = systemStorageFlashRead,
    .prog = systemStorageFlashProg,
    .erase = systemStorageFlashErase,
    .sync = systemStorageFlashSync,
};

void systemStorageProcess(void)
{
    if (!vfsInit()) {
        LOG_E(SYSTEM_STORAGE_LOG_TAG, "vfs init failed err=%u", (unsigned)vfsGetStatus()->lastError);
        gSystemStorageReady = false;
        return;
    }

    if (!gSystemStorageMountsRegistered) {
        if (!systemStorageRegisterMounts()) {
            LOG_E(SYSTEM_STORAGE_LOG_TAG, "mount registration failed err=%u", (unsigned)vfsGetStatus()->lastError);
            gSystemStorageReady = false;
            return;
        }
        gSystemStorageMountsRegistered = true;
    }

    if (!gSystemStorageConsoleRegistered) {
        if (!vfsDebugConsoleRegister("/")) {
            LOG_E(SYSTEM_STORAGE_LOG_TAG, "vfs console register failed");
            gSystemStorageReady = false;
            return;
        }

        gSystemStorageConsoleRegistered = true;
        LOG_I(SYSTEM_STORAGE_LOG_TAG, "vfs shell ready at /");
    }

    if (!gSystemStorageFormatState.isTaskCreated && !systemStorageEnsureFormatTask()) {
        LOG_E(SYSTEM_STORAGE_LOG_TAG, "format task create failed");
        gSystemStorageReady = false;
        return;
    }

    gSystemStorageReady = true;

    (void)systemStoragePrepareFlash(W25QXXX_DEV0);
    systemStorageTryMount(SYSTEM_STORAGE_FLASH_MOUNT_PATH);
    systemStorageTryMount(SYSTEM_STORAGE_SD_MOUNT_PATH);
}

bool systemStorageIsReady(void)
{
    return gSystemStorageReady;
}

bool systemStorageIsBusy(void)
{
    if (systemStorageIsFlashFormatActive()) {
        return true;
    }

    return gSystemStorageFlashMountState.hasLastError &&
           (gSystemStorageFlashMountState.lastError == eVFS_CORRUPT) &&
           !vfsIsMounted(SYSTEM_STORAGE_FLASH_MOUNT_PATH);
}

static bool systemStorageFlashInit(void *deviceContext)
{
    eW25qxxxMapType lDevice = (eW25qxxxMapType)(uintptr_t)deviceContext;
    return systemStoragePrepareFlash(lDevice);
}

static bool systemStorageFlashRead(void *deviceContext, uint32_t address, void *buffer, uint32_t size)
{
    eW25qxxxMapType lDevice = (eW25qxxxMapType)(uintptr_t)deviceContext;
    return w25qxxxRead(lDevice, address, (uint8_t *)buffer, size) == W25QXXX_STATUS_OK;
}

static bool systemStorageFlashProg(void *deviceContext, uint32_t address, const void *buffer, uint32_t size)
{
    eW25qxxxMapType lDevice = (eW25qxxxMapType)(uintptr_t)deviceContext;
    return w25qxxxWrite(lDevice, address, (const uint8_t *)buffer, size) == W25QXXX_STATUS_OK;
}

static bool systemStorageFlashErase(void *deviceContext, uint32_t address, uint32_t size)
{
    eW25qxxxMapType lDevice = (eW25qxxxMapType)(uintptr_t)deviceContext;
    uint32_t lOffset = 0UL;

    if ((size == 0UL) || ((size % W25QXXX_SECTOR_SIZE) != 0UL)) {
        return false;
    }

    while (lOffset < size) {
        if (w25qxxxEraseSector(lDevice, address + lOffset) != W25QXXX_STATUS_OK) {
            return false;
        }

        if (gSystemStorageFormatState.isRunning) {
            gSystemStorageFormatState.erasedBytes += W25QXXX_SECTOR_SIZE;
            systemStorageLogFormatProgress();
        }

        lOffset += W25QXXX_SECTOR_SIZE;
    }

    return true;
}

static bool systemStorageFlashSync(void *deviceContext)
{
    (void)deviceContext;
    return true;
}

static bool systemStoragePrepareFlash(eW25qxxxMapType device)
{
    uint32_t lNowTickMs;
    uint8_t lManufacturerId = 0U;
    uint8_t lMemoryType = 0U;
    uint8_t lCapacityId = 0U;
    eW25qxxxStatus lStatus;

    if (gSystemStorageFlashState.isReady) {
        return true;
    }

    lNowTickMs = repRtosGetTickMs();
    if ((gSystemStorageFlashState.lastProbeTickMs != 0UL) &&
        ((lNowTickMs - gSystemStorageFlashState.lastProbeTickMs) < SYSTEM_STORAGE_MOUNT_RETRY_MS)) {
        return false;
    }

    gSystemStorageFlashState.lastProbeTickMs = lNowTickMs;

    lStatus = w25qxxxReadJedecId(device, &lManufacturerId, &lMemoryType, &lCapacityId);
    if (lStatus != W25QXXX_STATUS_OK) {
        if (!gSystemStorageFlashState.hasLastStatus || (gSystemStorageFlashState.lastStatus != lStatus) || gSystemStorageFlashState.isReady) {
            LOG_W(SYSTEM_STORAGE_LOG_TAG, "w25q read jedec fail status=%d", (int)lStatus);
        }

        gSystemStorageFlashState.isReady = false;
        gSystemStorageFlashState.hasLastStatus = true;
        gSystemStorageFlashState.lastStatus = lStatus;
        return false;
    }

    if ((!gSystemStorageFlashState.isReady) ||
        (gSystemStorageFlashState.manufacturerId != lManufacturerId) ||
        (gSystemStorageFlashState.memoryType != lMemoryType) ||
        (gSystemStorageFlashState.capacityId != lCapacityId)) {
        LOG_I(SYSTEM_STORAGE_LOG_TAG,
              "w25q jedec=%02X %02X %02X",
              (unsigned int)lManufacturerId,
              (unsigned int)lMemoryType,
              (unsigned int)lCapacityId);
    }

    lStatus = w25qxxxInit(device);
    if (lStatus != W25QXXX_STATUS_OK) {
        if (!gSystemStorageFlashState.hasLastStatus || (gSystemStorageFlashState.lastStatus != lStatus) || gSystemStorageFlashState.isReady) {
            LOG_W(SYSTEM_STORAGE_LOG_TAG, "w25q init fail status=%d", (int)lStatus);
        }

        gSystemStorageFlashState.isReady = false;
        gSystemStorageFlashState.hasLastStatus = true;
        gSystemStorageFlashState.lastStatus = lStatus;
        return false;
    }

    gSystemStorageFlashState.isReady = true;
    gSystemStorageFlashState.hasLastStatus = false;
    gSystemStorageFlashState.manufacturerId = lManufacturerId;
    gSystemStorageFlashState.memoryType = lMemoryType;
    gSystemStorageFlashState.capacityId = lCapacityId;
    return true;
}

static bool systemStorageRegisterMounts(void)
{
    stVfsLittlefsCfg lLittlefsCfg;
    stVfsFatfsCfg lFatfsCfg;
    stVfsMountCfg lMountCfg;

    (void)memset(&lLittlefsCfg, 0, sizeof(lLittlefsCfg));
    lLittlefsCfg.blockDeviceOps = &gSystemStorageFlashOps;
    lLittlefsCfg.blockDeviceContext = (void *)(uintptr_t)W25QXXX_DEV0;
    lLittlefsCfg.regionOffset = SYSTEM_STORAGE_W25Q_REGION_OFFSET;
    lLittlefsCfg.regionSizeBytes = SYSTEM_STORAGE_W25Q_REGION_SIZE;
    lLittlefsCfg.readSize = 1U;
    lLittlefsCfg.progSize = 256U;
    lLittlefsCfg.blockSize = W25QXXX_SECTOR_SIZE;
    lLittlefsCfg.cacheSize = 256U;
    lLittlefsCfg.lookaheadSize = 32U;
    lLittlefsCfg.blockCycles = 200;
    if (!vfsLittlefsInitContext(&gSystemStorageLittlefsContext, &lLittlefsCfg)) {
        return false;
    }

    lFatfsCfg.physicalDrive = 0U;
    if (!vfsFatfsInitContext(&gSystemStorageFatfsContext, &lFatfsCfg)) {
        return false;
    }

    (void)memset(&lMountCfg, 0, sizeof(lMountCfg));
    lMountCfg.mountPath = SYSTEM_STORAGE_FLASH_MOUNT_PATH;
    lMountCfg.backendOps = vfsLittlefsGetBackendOps();
    lMountCfg.backendContext = &gSystemStorageLittlefsContext;
    lMountCfg.isAutoMount = false;
    lMountCfg.isReadOnly = false;
    if (!vfsRegisterMount(&lMountCfg)) {
        return false;
    }

    lMountCfg.mountPath = SYSTEM_STORAGE_SD_MOUNT_PATH;
    lMountCfg.backendOps = vfsFatfsGetBackendOps();
    lMountCfg.backendContext = &gSystemStorageFatfsContext;
    lMountCfg.isAutoMount = false;
    lMountCfg.isReadOnly = false;
    if (!vfsRegisterMount(&lMountCfg)) {
        return false;
    }

    return true;
}

static bool systemStorageEnsureFormatTask(void)
{
    stRepRtosTaskConfig lTaskCfg;

    if (gSystemStorageFormatState.isTaskCreated) {
        return true;
    }

    (void)memset(&lTaskCfg, 0, sizeof(lTaskCfg));
    lTaskCfg.name = "StorageFmt";
    lTaskCfg.entry = systemStorageFormatTask;
    lTaskCfg.argument = NULL;
    lTaskCfg.stackBuffer = &gSystemStorageFormatTaskStack[0];
    lTaskCfg.stackSize = SYSTEM_STORAGE_FORMAT_TASK_STACK;
    lTaskCfg.priority = SYSTEM_STORAGE_FORMAT_TASK_PRIO;
    lTaskCfg.handle = NULL;

    if (repRtosTaskCreate(&lTaskCfg) != REP_RTOS_STATUS_OK) {
        return false;
    }

    gSystemStorageFormatState.isTaskCreated = true;
    return true;
}

static void systemStorageFormatTask(void *argument)
{
    eVfsResult lError;

    (void)argument;

    for (;;) {
        if (!gSystemStorageFormatState.isPending) {
            (void)repRtosDelayMs(SYSTEM_STORAGE_FORMAT_TASK_DELAY);
            continue;
        }

        gSystemStorageFormatState.isPending = false;
        gSystemStorageFormatState.isRunning = true;
        gSystemStorageFormatState.hasLastError = false;
        systemStorageResetFormatProgress();

        LOG_I(SYSTEM_STORAGE_LOG_TAG,
              "background format start %s size=%luKB",
              SYSTEM_STORAGE_FLASH_MOUNT_PATH,
              (unsigned long)(SYSTEM_STORAGE_W25Q_REGION_SIZE / 1024UL));
        logProcessOutput();

        if (vfsFormat(SYSTEM_STORAGE_FLASH_MOUNT_PATH)) {
            gSystemStorageFormatState.erasedBytes = gSystemStorageFormatState.totalBytes;
            gSystemStorageFormatState.hasLastError = false;
            systemStorageLogFormatProgress();
            gSystemStorageFormatState.isRunning = false;
            LOG_I(SYSTEM_STORAGE_LOG_TAG, "background format done %s", SYSTEM_STORAGE_FLASH_MOUNT_PATH);
            logProcessOutput();
            continue;
        }

        lError = vfsGetStatus()->lastError;
        gSystemStorageFormatState.isRunning = false;
        gSystemStorageFormatState.lastError = lError;
        gSystemStorageFormatState.hasLastError = true;
        LOG_W(SYSTEM_STORAGE_LOG_TAG,
              "background format fail %s err=%u",
              SYSTEM_STORAGE_FLASH_MOUNT_PATH,
              (unsigned)lError);
        logProcessOutput();
        (void)repRtosDelayMs(SYSTEM_STORAGE_MOUNT_RETRY_MS);
    }
}

static void systemStorageRequestFlashFormat(void)
{
    if (gSystemStorageFormatState.isPending || gSystemStorageFormatState.isRunning) {
        return;
    }

    if (!systemStorageEnsureFormatTask()) {
        LOG_W(SYSTEM_STORAGE_LOG_TAG, "background format unavailable %s", SYSTEM_STORAGE_FLASH_MOUNT_PATH);
        return;
    }

    gSystemStorageFormatState.isPending = true;
    LOG_I(SYSTEM_STORAGE_LOG_TAG, "background format scheduled %s", SYSTEM_STORAGE_FLASH_MOUNT_PATH);
}

static bool systemStorageIsFlashFormatActive(void)
{
    return gSystemStorageFormatState.isPending || gSystemStorageFormatState.isRunning;
}

static void systemStorageResetFormatProgress(void)
{
    gSystemStorageFormatState.erasedBytes = 0UL;
    gSystemStorageFormatState.totalBytes = SYSTEM_STORAGE_W25Q_REGION_SIZE;
    gSystemStorageFormatState.nextLogPercent = SYSTEM_STORAGE_FORMAT_LOG_STEP;
}

static void systemStorageLogFormatProgress(void)
{
    uint32_t lPercent;

    if (!gSystemStorageFormatState.isRunning || (gSystemStorageFormatState.totalBytes == 0UL)) {
        return;
    }

    lPercent = (gSystemStorageFormatState.erasedBytes * 100UL) / gSystemStorageFormatState.totalBytes;
    if (lPercent > 100UL) {
        lPercent = 100UL;
    }

    if ((lPercent < gSystemStorageFormatState.nextLogPercent) &&
        (gSystemStorageFormatState.erasedBytes < gSystemStorageFormatState.totalBytes)) {
        return;
    }

    LOG_I(SYSTEM_STORAGE_LOG_TAG,
          "background format progress %s %lu/%luKB (%lu%%)",
          SYSTEM_STORAGE_FLASH_MOUNT_PATH,
          (unsigned long)(gSystemStorageFormatState.erasedBytes / 1024UL),
          (unsigned long)(gSystemStorageFormatState.totalBytes / 1024UL),
          (unsigned long)lPercent);
    logProcessOutput();

    while (gSystemStorageFormatState.nextLogPercent <= lPercent) {
        gSystemStorageFormatState.nextLogPercent += SYSTEM_STORAGE_FORMAT_LOG_STEP;
    }
}

static bool systemStorageCheckSdMountPresence(stSystemStorageMountState *state)
{
    bool lIsPresent = false;
    bool lIsWriteProtected = false;
    eSdcardStatus lStatus;

    if (state == NULL) {
        return false;
    }

    lStatus = sdcardGetStatus(SDCARD_DEV0, &lIsPresent, &lIsWriteProtected);
    (void)lIsWriteProtected;
    if ((lStatus != SDCARD_STATUS_OK) && (lStatus != SDCARD_STATUS_NO_MEDIUM)) {
        return true;
    }

    if (!lIsPresent) {
        if (state->wasMounted) {
            (void)vfsUnmount(SYSTEM_STORAGE_SD_MOUNT_PATH);
            LOG_I(SYSTEM_STORAGE_LOG_TAG, "sd removed, unmount %s", SYSTEM_STORAGE_SD_MOUNT_PATH);
        } else if (!state->hasPresence || state->isPresent) {
            LOG_I(SYSTEM_STORAGE_LOG_TAG, "sd absent, skip mount %s", SYSTEM_STORAGE_SD_MOUNT_PATH);
        }

        state->wasMounted = false;
        state->hasLastError = false;
        state->hasPresence = true;
        state->isPresent = false;
        return false;
    }

    if (!state->hasPresence || !state->isPresent) {
        LOG_I(SYSTEM_STORAGE_LOG_TAG, "sd detected, mount %s", SYSTEM_STORAGE_SD_MOUNT_PATH);
        state->lastAttemptTickMs = 0UL;
    }

    state->hasPresence = true;
    state->isPresent = true;
    return true;
}

static void systemStorageTryMount(const char *mountPath)
{
    stSystemStorageMountState *lState;
    uint32_t lNowTickMs;
    eVfsResult lError;

    lState = systemStorageGetMountState(mountPath);
    if (lState == NULL) {
        return;
    }

    if ((strcmp(mountPath, SYSTEM_STORAGE_FLASH_MOUNT_PATH) == 0) && systemStorageIsFlashFormatActive()) {
        return;
    }

    if ((strcmp(mountPath, SYSTEM_STORAGE_SD_MOUNT_PATH) == 0) && !systemStorageCheckSdMountPresence(lState)) {
        return;
    }

    if (vfsIsMounted(mountPath)) {
        if (!lState->wasMounted) {
            LOG_I(SYSTEM_STORAGE_LOG_TAG, "mount ok %s", mountPath);
        }

        lState->wasMounted = true;
        lState->hasLastError = false;
        return;
    }

    lNowTickMs = repRtosGetTickMs();
    if ((lState->lastAttemptTickMs != 0UL) && ((lNowTickMs - lState->lastAttemptTickMs) < SYSTEM_STORAGE_MOUNT_RETRY_MS)) {
        return;
    }

    lState->lastAttemptTickMs = lNowTickMs;

    if (vfsMount(mountPath)) {
        LOG_I(SYSTEM_STORAGE_LOG_TAG, "mount ok %s", mountPath);
        lState->wasMounted = true;
        lState->hasLastError = false;
        return;
    }

    lError = vfsGetStatus()->lastError;
    if ((strcmp(mountPath, SYSTEM_STORAGE_FLASH_MOUNT_PATH) == 0) && (lError == eVFS_CORRUPT)) {
        systemStorageRequestFlashFormat();
        if (!lState->hasLastError || (lState->lastError != lError) || lState->wasMounted) {
            LOG_W(SYSTEM_STORAGE_LOG_TAG, "mount requires background format %s", mountPath);
        }
        lState->wasMounted = false;
        lState->hasLastError = true;
        lState->lastError = lError;
        return;
    }

    if (!lState->hasLastError || (lState->lastError != lError) || lState->wasMounted) {
        LOG_W(SYSTEM_STORAGE_LOG_TAG, "mount fail %s err=%u", mountPath, (unsigned)lError);
    }

    lState->wasMounted = false;
    lState->hasLastError = true;
    lState->lastError = lError;
}

static stSystemStorageMountState *systemStorageGetMountState(const char *mountPath)
{
    if (mountPath == NULL) {
        return NULL;
    }

    if (strcmp(mountPath, SYSTEM_STORAGE_FLASH_MOUNT_PATH) == 0) {
        return &gSystemStorageFlashMountState;
    }

    if (strcmp(mountPath, SYSTEM_STORAGE_SD_MOUNT_PATH) == 0) {
        return &gSystemStorageSdMountState;
    }

    return NULL;
}
/**************************End of file********************************/
