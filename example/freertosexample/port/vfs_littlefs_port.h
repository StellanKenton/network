/************************************************************************************
* @file     : vfs_littlefs_port.h
* @brief    : Project-side littlefs mount binding for vfs.
* @details  : Binds the GD25Qxxx external flash memory region to the reusable
*             vfs littlefs backend and registers the `/mem` mount.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_VFS_LITTLEFS_PORT_H
#define REBUILDCPR_VFS_LITTLEFS_PORT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool vfsLittlefsPortInit(void);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_VFS_LITTLEFS_PORT_H
/**************************End of file********************************/
