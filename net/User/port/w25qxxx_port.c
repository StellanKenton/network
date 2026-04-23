/***********************************************************************************
* @file     : w25qxxx_port.c
* @brief    : W25Qxxx project port-layer implementation.
**********************************************************************************/
#include "w25qxxx_port.h"

#include "drvspi_port.h"
#include "../../rep/service/rtos/rtos.h"

static eDrvStatus w25qxxxPortSpiInit(uint8_t bus)
{
    return drvSpiInit(bus);
}

static eDrvStatus w25qxxxPortSpiTransfer(uint8_t bus,
                                         const uint8_t *writeBuffer,
                                         uint16_t writeLength,
                                         const uint8_t *secondWriteBuffer,
                                         uint16_t secondWriteLength,
                                         uint8_t *readBuffer,
                                         uint16_t readLength,
                                         uint8_t readFillData)
{
    stDrvSpiTransfer lTransfer;

    lTransfer.writeBuffer = writeBuffer;
    lTransfer.writeLength = writeLength;
    lTransfer.secondWriteBuffer = secondWriteBuffer;
    lTransfer.secondWriteLength = secondWriteLength;
    lTransfer.readBuffer = readBuffer;
    lTransfer.readLength = readLength;
    lTransfer.readFillData = readFillData;
    return drvSpiTransfer(bus, &lTransfer);
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

    cfg->linkId = DRVSPI_FLASH;
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
