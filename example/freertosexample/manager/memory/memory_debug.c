#include "memory_debug.h"

#include "memory.h"
#include "../../../rep/service/vfs/vfs_debug.h"

bool memoryDebugConsoleRegister(void)
{
    return vfsDebugConsoleRegister(MEMORY_MOUNT_PATH);
}

/**************************End of file********************************/
