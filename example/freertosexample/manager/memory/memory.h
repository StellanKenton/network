/************************************************************************************
* @file     : memory.h
* @brief    : Project-side littlefs storage manager declarations.
* @details  : Initializes the GD25Qxxx-backed littlefs instance and exposes a
*             small set of file operations for product-side data persistence.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_MEMORY_H
#define REBUILDCPR_MEMORY_H

#include <stdbool.h>
#include <stdint.h>

#include "../../../rep/service/vfs/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEMORY_LOG_TAG                        "memory"
#define MEMORY_MOUNT_PATH                     "/mem"
#define MEMORY_ENTRY_NAME_MAX                 VFS_ENTRY_NAME_MAX

typedef enum eMemoryState {
    eMEMORY_STATE_UNINIT = 0,
    eMEMORY_STATE_READY,
    eMEMORY_STATE_FAULT,
} eMemoryState;

typedef struct stMemoryStatus {
    eMemoryState state;
    bool isMounted;
    int32_t lastError;
} stMemoryStatus;

typedef struct stMemorySpaceInfo {
    uint32_t totalSize;
    uint32_t usedSize;
    uint32_t freeSize;
} stMemorySpaceInfo;

typedef enum eMemoryEntryType {
    eMEMORY_ENTRY_TYPE_FILE = 0,
    eMEMORY_ENTRY_TYPE_DIR,
} eMemoryEntryType;

typedef struct stMemoryEntryInfo {
    eMemoryEntryType type;
    uint32_t size;
    char name[MEMORY_ENTRY_NAME_MAX + 1U];
} stMemoryEntryInfo;

typedef bool (*pfMemoryDirVisitor)(void *context, const stMemoryEntryInfo *entry);

bool memoryInit(void);
void memoryProcess(void);
bool memoryIsReady(void);
const stMemoryStatus *memoryGetStatus(void);
bool memoryGetSpaceInfo(stMemorySpaceInfo *info);
bool memoryFormat(void);
bool memoryMkdir(const char *path);
bool memoryGetInfo(const char *path, stMemoryEntryInfo *info);
bool memoryListDir(const char *path, pfMemoryDirVisitor visitor, void *context, uint32_t *entryCount);
bool memoryExists(const char *path);
bool memoryDelete(const char *path);
bool memoryRename(const char *oldPath, const char *newPath);
bool memoryGetFileSize(const char *path, uint32_t *size);
bool memoryReadFile(const char *path, void *buffer, uint32_t bufferSize, uint32_t *actualSize);
bool memoryWriteFile(const char *path, const void *data, uint32_t size);
bool memoryAppendFile(const char *path, const void *data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_MEMORY_H
/**************************End of file********************************/
