/************************************************************************************
* @file     : bsp_sdio.h
* @brief    : Board SDIO BSP declarations.
***********************************************************************************/
#ifndef NETWORK_APP_BSP_SDIO_H
#define NETWORK_APP_BSP_SDIO_H

#include "../../rep/module/sdcard/sdcard_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_SDIO_DEV0                0U
#define BSP_SDIO_DEV_MAX             1U

eDrvStatus bspSdioInit(uint8_t sdio, uint32_t timeoutMs);
eDrvStatus bspSdioGetStatus(uint8_t sdio, bool *isPresent, bool *isWriteProtected);
eDrvStatus bspSdioReadBlocks(uint8_t sdio, uint32_t startBlock, uint8_t *buffer, uint32_t blockCount);
eDrvStatus bspSdioWriteBlocks(uint8_t sdio, uint32_t startBlock, const uint8_t *buffer, uint32_t blockCount);
eDrvStatus bspSdioIoctl(uint8_t sdio, uint32_t command, void *buffer);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
