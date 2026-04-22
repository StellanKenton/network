/***********************************************************************************
* @file     : drvuart_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvuart_port.h"

#include <stddef.h>

#include "drvuart.h"

#include "../bsp/bspuart.h"

static stRingBuffer gDrvUartRingBuffer[DRVUART_MAX];
static uint8_t gDrvUartRingStorageDebug[DRVUART_RECVLEN_DEBUGUART];
static uint8_t gDrvUartRingStorageAudio[DRVUART_RECVLEN_AUDIO];

static stDrvUartBspInterface gDrvUartBspInterface[DRVUART_MAX] = {
    [DRVUART_DEBUG] = {
        .init = bspUartInit,
        .transmit = bspUartTransmit,
        .transmitIt = bspUartTransmitIt,
        .transmitDma = bspUartTransmitDma,
        .getDataLen = bspUartGetDataLen,
        .receive = bspUartReceive,
        .Buffer = gDrvUartRingStorageDebug,
    },
    [DRVUART_AUDIO] = {
        .init = bspUartInit,
        .transmit = bspUartTransmit,
        .transmitIt = bspUartTransmitIt,
        .transmitDma = bspUartTransmitDma,
        .getDataLen = bspUartGetDataLen,
        .receive = bspUartReceive,
        .Buffer = gDrvUartRingStorageAudio,
    },
};

const stDrvUartBspInterface *drvUartGetPlatformBspInterfaces(void)
{
    return gDrvUartBspInterface;
}

stRingBuffer *drvUartGetPlatformRingBuffer(uint8_t uart)
{
    if (uart >= DRVUART_MAX) {
        return NULL;
    }

    return &gDrvUartRingBuffer[uart];
}

eDrvStatus drvUartGetPlatformStorageConfig(uint8_t uart, uint8_t **storage, uint32_t *capacity)
{
    if ((storage == NULL) || (capacity == NULL) || (uart >= DRVUART_MAX)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    switch ((eDrvUartPortMap)uart) {
        case DRVUART_DEBUG:
            *storage = gDrvUartRingStorageDebug;
            *capacity = sizeof(gDrvUartRingStorageDebug);
            return DRV_STATUS_OK;
        case DRVUART_AUDIO:
            *storage = gDrvUartRingStorageAudio;
            *capacity = sizeof(gDrvUartRingStorageAudio);
            return DRV_STATUS_OK;
        default:
            *storage = NULL;
            *capacity = 0U;
            return DRV_STATUS_UNSUPPORTED;
    }
}

/**************************End of file********************************/
