/***********************************************************************************
* @file     : sdcard_port.c
* @brief    : Project-side SD card binding.
**********************************************************************************/
#include "sdcard_port.h"

#include "../bsp/bsp_sdio.h"

static const stSdcardInterface gSdcardPlatformInterface = {
    bspSdioInit,
    bspSdioGetStatus,
    bspSdioReadBlocks,
    bspSdioWriteBlocks,
    bspSdioIoctl,
};

void sdcardLoadPlatformDefaultCfg(eSdcardMapType device, stSdcardCfg *cfg)
{
    if ((cfg == NULL) || (device >= SDCARD_DEV_MAX)) {
        return;
    }

    cfg->linkId = SDCARD_PORT_LINK0;
    cfg->initTimeoutMs = SDCARD_DEFAULT_INIT_TIMEOUT_MS;
}

const stSdcardInterface *sdcardGetPlatformInterface(const stSdcardCfg *cfg)
{
    return sdcardPlatformIsValidCfg(cfg) ? &gSdcardPlatformInterface : NULL;
}

bool sdcardPlatformIsValidCfg(const stSdcardCfg *cfg)
{
    if (cfg == NULL) {
        return false;
    }

    return (cfg->linkId == SDCARD_PORT_LINK0);
}

/**************************End of file********************************/
