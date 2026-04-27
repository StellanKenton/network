/************************************************************************************
* @file     : bspusb_ec800.c
* @brief    : EC800M USB transport helper implementation.
* @details  : Uses drvusb as the project USB transport boundary and keeps a small
*             RX cache for flowparser polling.
* @author   : GitHub Copilot
* @date     : 2026-04-27
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "bspusb_ec800.h"

#include "../port/drvusb_port.h"
#include "../../rep/tools/ringbuffer/ringbuffer.h"

typedef struct stBspUsbEc800Context {
    bool isInitialized;
    stRingBuffer rxRing;
    uint8_t rxStorage[BSPUSB_EC800_RX_STORAGE_SIZE];
    uint8_t pollBuffer[BSPUSB_EC800_RX_POLL_CHUNK_SIZE];
} stBspUsbEc800Context;

static stBspUsbEc800Context gBspUsbEc800Context;

static bool bspUsbEc800IsValidLink(uint8_t linkId);
static void bspUsbEc800PollRx(uint8_t linkId);

eDrvStatus bspUsbEc800Init(uint8_t linkId)
{
    eDrvStatus status;

    if (!bspUsbEc800IsValidLink(linkId)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (gBspUsbEc800Context.isInitialized) {
        return DRV_STATUS_OK;
    }

    if (ringBufferInit(&gBspUsbEc800Context.rxRing,
                       gBspUsbEc800Context.rxStorage,
                       sizeof(gBspUsbEc800Context.rxStorage)) != RINGBUFFER_OK) {
        return DRV_STATUS_ERROR;
    }

    status = drvUsbInit(linkId);
    if (status != DRV_STATUS_OK) {
        return status;
    }

    if (drvUsbGetRole(linkId) != DRVUSB_ROLE_HOST) {
        return DRV_STATUS_UNSUPPORTED;
    }

    status = drvUsbStart(linkId);
    if (status != DRV_STATUS_OK) {
        return status;
    }

    gBspUsbEc800Context.isInitialized = true;
    return DRV_STATUS_OK;
}

eDrvStatus bspUsbEc800Write(uint8_t linkId, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    if (!bspUsbEc800IsValidLink(linkId) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!bspUsbEc800IsLinkReady(linkId)) {
        return DRV_STATUS_NOT_READY;
    }

    return drvUsbTransmitTimeout(linkId, DRVUSB_PORT_EC800M_AT_OUT_EP, buffer, length, timeoutMs);
}

uint16_t bspUsbEc800GetRxLen(uint8_t linkId)
{
    if (!bspUsbEc800IsValidLink(linkId) || !gBspUsbEc800Context.isInitialized) {
        return 0U;
    }

    bspUsbEc800PollRx(linkId);
    return (uint16_t)ringBufferGetUsed(&gBspUsbEc800Context.rxRing);
}

eDrvStatus bspUsbEc800Read(uint8_t linkId, uint8_t *buffer, uint16_t length)
{
    uint32_t readLen;

    if (!bspUsbEc800IsValidLink(linkId) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    bspUsbEc800PollRx(linkId);
    readLen = ringBufferRead(&gBspUsbEc800Context.rxRing, buffer, length);
    return (readLen == length) ? DRV_STATUS_OK : DRV_STATUS_NOT_READY;
}

bool bspUsbEc800IsLinkReady(uint8_t linkId)
{
    return bspUsbEc800IsValidLink(linkId) && gBspUsbEc800Context.isInitialized &&
           (drvUsbGetRole(linkId) == DRVUSB_ROLE_HOST) && drvUsbIsConfigured(linkId);
}

static bool bspUsbEc800IsValidLink(uint8_t linkId)
{
    return linkId == DRVUSB_CELLULAR;
}

static void bspUsbEc800PollRx(uint8_t linkId)
{
    uint16_t actualLength;
    eDrvStatus status;

    if (!bspUsbEc800IsLinkReady(linkId)) {
        return;
    }

    do {
        actualLength = 0U;
        status = drvUsbReceiveTimeout(linkId,
                                      DRVUSB_PORT_EC800M_AT_IN_EP,
                                      gBspUsbEc800Context.pollBuffer,
                                      sizeof(gBspUsbEc800Context.pollBuffer),
                                      &actualLength,
                                      1U);
        if ((status == DRV_STATUS_OK) && (actualLength > 0U)) {
            (void)ringBufferWriteOverwrite(&gBspUsbEc800Context.rxRing,
                                           gBspUsbEc800Context.pollBuffer,
                                           actualLength);
        }
    } while ((status == DRV_STATUS_OK) && (actualLength > 0U));
}

/**************************End of file********************************/
