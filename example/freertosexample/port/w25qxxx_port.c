/***********************************************************************************
* @file     : w25qxxx_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "w25qxxx_port.h"

#include "main.h"
#include "drvspi_port.h"
#include "rtos.h"

static eDrvStatus w25qxxxPortSpiInit(uint8_t bus)
{
    return drvSpiInit(bus);
}

static eDrvStatus w25qxxxPortSpiTransfer(uint8_t bus, const uint8_t *writeBuffer, uint16_t writeLength, const uint8_t *secondWriteBuffer, uint16_t secondWriteLength, uint8_t *readBuffer, uint16_t readLength, uint8_t readFillData)
{
    stDrvSpiTransfer transfer;

    transfer.writeBuffer = writeBuffer;
    transfer.writeLength = writeLength;
    transfer.secondWriteBuffer = secondWriteBuffer;
    transfer.secondWriteLength = secondWriteLength;
    transfer.readBuffer = readBuffer;
    transfer.readLength = readLength;
    transfer.readFillData = readFillData;
    return drvSpiTransfer(bus, &transfer);
}

static const stW25qxxxSpiInterface gW25qxxxSpiInterface = {
    .init = w25qxxxPortSpiInit,
    .transfer = w25qxxxPortSpiTransfer,
};

void w25qxxxLoadPlatformDefaultCfg(eW25qxxxMapType device, stW25qxxxCfg *cfg)
{
    if ((cfg == NULL) || ((uint32_t)device >= (uint32_t)W25QXXX_DEV_MAX)) {
        return;
    }

    cfg->linkId = DRVSPI_BUS0;
}

const stW25qxxxSpiInterface *w25qxxxGetPlatformSpiInterface(const stW25qxxxCfg *cfg)
{
    if (!w25qxxxPlatformIsValidCfg(cfg)) {
        return NULL;
    }

    return &gW25qxxxSpiInterface;
}

bool w25qxxxPlatformIsValidCfg(const stW25qxxxCfg *cfg)
{
    return (cfg != NULL) && (cfg->linkId < DRVSPI_MAX);
}

void w25qxxxPlatformDelayMs(uint32_t delayMs)
{
    (void)repRtosDelayMs(delayMs);
}

/**************************End of file********************************/
