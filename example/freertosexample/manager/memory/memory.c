/***********************************************************************************
* @file     : memory.c
* @brief    : Project-side littlefs storage manager.
* @details  : Mounts littlefs on the GD25Qxxx external flash and provides a
*             minimal set of file operations for persistent product data.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "memory.h"

#include <string.h>

#include "../../../rep/service/log/log.h"
#include "../../port/vfs_littlefs_port.h"

static void memorySyncStatus(void);
static bool memoryBuildVfsPath(const char *memoryPath, char *vfsPath, uint32_t capacity);
static void memoryFillEntryInfo(const stVfsNodeInfo *vfsInfo, stMemoryEntryInfo *info);
static bool memoryListDirVisitorAdapter(void *context, const stVfsNodeInfo *entry);

struct stMemoryListDirContext {
    pfMemoryDirVisitor visitor;
    void *visitorContext;
};

static stMemoryStatus gMemoryStatus = {
    .state = eMEMORY_STATE_UNINIT,
    .isMounted = false,
    .lastError = 0,
};

static void memorySyncStatus(void)
{
    const stVfsStatus *lVfsStatus = vfsGetStatus();

    gMemoryStatus.isMounted = vfsIsMounted(MEMORY_MOUNT_PATH);
    if (gMemoryStatus.isMounted) {
        gMemoryStatus.state = eMEMORY_STATE_READY;
    } else if ((lVfsStatus != NULL) && (lVfsStatus->state == eVFS_STATE_FAULT)) {
        gMemoryStatus.state = eMEMORY_STATE_FAULT;
    } else {
        gMemoryStatus.state = eMEMORY_STATE_UNINIT;
    }

    gMemoryStatus.lastError = (lVfsStatus != NULL) ? (int32_t)lVfsStatus->lastError : 0;
}

static bool memoryBuildVfsPath(const char *memoryPath, char *vfsPath, uint32_t capacity)
{
    if ((memoryPath == NULL) || (memoryPath[0] != '/')) {
        return false;
    }

    return vfsTranslateMountPath(MEMORY_MOUNT_PATH, memoryPath, vfsPath, capacity);
}

static void memoryFillEntryInfo(const stVfsNodeInfo *vfsInfo, stMemoryEntryInfo *info)
{
    if ((vfsInfo == NULL) || (info == NULL)) {
        return;
    }

    (void)memset(info, 0, sizeof(*info));
    info->type = (vfsInfo->type == eVFS_NODE_DIR) ? eMEMORY_ENTRY_TYPE_DIR : eMEMORY_ENTRY_TYPE_FILE;
    info->size = vfsInfo->size;
    (void)memcpy(info->name, vfsInfo->name, sizeof(info->name));
    info->name[sizeof(info->name) - 1U] = '\0';
}

static bool memoryListDirVisitorAdapter(void *context, const stVfsNodeInfo *entry)
{
    struct stMemoryListDirContext *lAdapterContext = (struct stMemoryListDirContext *)context;
    stMemoryEntryInfo lInfo;

    if ((lAdapterContext == NULL) || (lAdapterContext->visitor == NULL) || (entry == NULL)) {
        return false;
    }

    memoryFillEntryInfo(entry, &lInfo);
    return lAdapterContext->visitor(lAdapterContext->visitorContext, &lInfo);
}

bool memoryInit(void)
{
    if (!vfsLittlefsPortInit()) {
        memorySyncStatus();
        LOG_E(MEMORY_LOG_TAG, "vfs littlefs port init fail err=%ld", (long)gMemoryStatus.lastError);
        return false;
    }

    if (!vfsMount(MEMORY_MOUNT_PATH) && (vfsGetStatus()->lastError != eVFS_OK)) {
        memorySyncStatus();
        LOG_E(MEMORY_LOG_TAG, "memory mount fail err=%ld", (long)gMemoryStatus.lastError);
        return false;
    }

    memorySyncStatus();
    return gMemoryStatus.isMounted;
}

void memoryProcess(void)
{
    
}

bool memoryIsReady(void)
{
    memorySyncStatus();
    return gMemoryStatus.isMounted && (gMemoryStatus.state == eMEMORY_STATE_READY);
}

const stMemoryStatus *memoryGetStatus(void)
{
    return &gMemoryStatus;
}

bool memoryGetSpaceInfo(stMemorySpaceInfo *info)
{
    if (info == NULL) {
        return false;
    }

    if (!memoryInit()) {
        return false;
    }

    if (!vfsGetSpaceInfo(MEMORY_MOUNT_PATH, (stVfsSpaceInfo *)info)) {
        memorySyncStatus();
        return false;
    }

    memorySyncStatus();
    return true;
}

bool memoryFormat(void)
{
    if (!memoryInit()) {
        return false;
    }

    if (!vfsFormat(MEMORY_MOUNT_PATH)) {
        memorySyncStatus();
        return false;
    }

    memorySyncStatus();
    return true;
}

bool memoryMkdir(const char *path)
{
    char lVfsPath[VFS_PATH_MAX];

    if ((path == NULL) || !memoryBuildVfsPath(path, lVfsPath, sizeof(lVfsPath)) || !memoryInit()) {
        return false;
    }

    if (!vfsMkdir(lVfsPath)) {
        memorySyncStatus();
        return false;
    }

    memorySyncStatus();
    return true;
}

bool memoryGetInfo(const char *path, stMemoryEntryInfo *info)
{
    char lVfsPath[VFS_PATH_MAX];
    stVfsNodeInfo lInfo;

    if ((path == NULL) || (info == NULL) || !memoryBuildVfsPath(path, lVfsPath, sizeof(lVfsPath)) || !memoryInit()) {
        return false;
    }

    if ((path[0] == '/') && (path[1] == '\0')) {
        info->type = eMEMORY_ENTRY_TYPE_DIR;
        info->name[0] = '/';
        info->name[1] = '\0';
        info->size = 0U;
        memorySyncStatus();
        return true;
    }

    if (!vfsGetInfo(lVfsPath, &lInfo)) {
        memorySyncStatus();
        return false;
    }

    memoryFillEntryInfo(&lInfo, info);
    memorySyncStatus();
    return true;
}

bool memoryListDir(const char *path, pfMemoryDirVisitor visitor, void *context, uint32_t *entryCount)
{
    char lVfsPath[VFS_PATH_MAX];
    struct stMemoryListDirContext lAdapterContext;

    if ((path == NULL) || !memoryBuildVfsPath(path, lVfsPath, sizeof(lVfsPath)) || !memoryInit()) {
        return false;
    }

    lAdapterContext.visitor = visitor;
    lAdapterContext.visitorContext = context;
    if (!vfsListDir(lVfsPath, (visitor != NULL) ? memoryListDirVisitorAdapter : NULL, &lAdapterContext, entryCount)) {
        memorySyncStatus();
        return false;
    }

    memorySyncStatus();
    return true;
}

bool memoryExists(const char *path)
{
    char lVfsPath[VFS_PATH_MAX];

    if ((path == NULL) || !memoryBuildVfsPath(path, lVfsPath, sizeof(lVfsPath)) || !memoryInit()) {
        return false;
    }

    return vfsExists(lVfsPath);
}

bool memoryDelete(const char *path)
{
    char lVfsPath[VFS_PATH_MAX];

    if ((path == NULL) || !memoryBuildVfsPath(path, lVfsPath, sizeof(lVfsPath)) || !memoryInit()) {
        return false;
    }

    if (!vfsDelete(lVfsPath)) {
        memorySyncStatus();
        return false;
    }

    memorySyncStatus();
    return true;
}

bool memoryRename(const char *oldPath, const char *newPath)
{
    char lOldVfsPath[VFS_PATH_MAX];
    char lNewVfsPath[VFS_PATH_MAX];

    if ((oldPath == NULL) || (newPath == NULL) ||
        !memoryBuildVfsPath(oldPath, lOldVfsPath, sizeof(lOldVfsPath)) ||
        !memoryBuildVfsPath(newPath, lNewVfsPath, sizeof(lNewVfsPath)) ||
        !memoryInit()) {
        return false;
    }

    if (!vfsRename(lOldVfsPath, lNewVfsPath)) {
        memorySyncStatus();
        return false;
    }

    memorySyncStatus();
    return true;
}

bool memoryGetFileSize(const char *path, uint32_t *size)
{
    char lVfsPath[VFS_PATH_MAX];

    if ((path == NULL) || (size == NULL) || !memoryBuildVfsPath(path, lVfsPath, sizeof(lVfsPath)) || !memoryInit()) {
        return false;
    }

    if (!vfsGetFileSize(lVfsPath, size)) {
        memorySyncStatus();
        return false;
    }

    memorySyncStatus();
    return true;
}

bool memoryReadFile(const char *path, void *buffer, uint32_t bufferSize, uint32_t *actualSize)
{
    char lVfsPath[VFS_PATH_MAX];

    if ((path == NULL) || ((buffer == NULL) && (bufferSize > 0U)) || !memoryBuildVfsPath(path, lVfsPath, sizeof(lVfsPath)) || !memoryInit()) {
        return false;
    }

    if (!vfsReadFile(lVfsPath, buffer, bufferSize, actualSize)) {
        memorySyncStatus();
        return false;
    }

    memorySyncStatus();
    return true;
}

bool memoryWriteFile(const char *path, const void *data, uint32_t size)
{
    char lVfsPath[VFS_PATH_MAX];

    if ((path == NULL) || ((data == NULL) && (size > 0U)) || !memoryBuildVfsPath(path, lVfsPath, sizeof(lVfsPath)) || !memoryInit()) {
        return false;
    }

    if (!vfsWriteFile(lVfsPath, data, size)) {
        memorySyncStatus();
        return false;
    }

    memorySyncStatus();
    return true;
}

bool memoryAppendFile(const char *path, const void *data, uint32_t size)
{
    char lVfsPath[VFS_PATH_MAX];

    if ((path == NULL) || ((data == NULL) && (size > 0U)) || !memoryBuildVfsPath(path, lVfsPath, sizeof(lVfsPath)) || !memoryInit()) {
        return false;
    }

    if (!vfsAppendFile(lVfsPath, data, size)) {
        memorySyncStatus();
        return false;
    }

    memorySyncStatus();
    return true;
}

/**************************End of file********************************/
