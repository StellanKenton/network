/***********************************************************************************
* @file     : fc41d_port.c
* @brief    : Project-side FC41D transport binding.
* @details  : Maps FC41D to DRVUART_WIFI and system tick for the current board.
* @author   : GitHub Copilot
* @date     : 2026-04-21
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "fc41d_port.h"

#include "drvuart_port.h"
#include "drvgpio_port.h"
#include "../system/system.h"
#include "../../rep/driver/drvgpio/drvgpio.h"
#include "../../rep/driver/drvuart/drvuart.h"

static eDrvStatus fc41dPortTransportInit(uint8_t linkId)
{
    return drvUartInit(linkId);
}

static eDrvStatus fc41dPortTransportWrite(uint8_t linkId, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    return drvUartTransmit(linkId, buffer, length, timeoutMs);
}

static uint16_t fc41dPortTransportGetRxLen(uint8_t linkId)
{
    return drvUartGetDataLen(linkId);
}

static eDrvStatus fc41dPortTransportRead(uint8_t linkId, uint8_t *buffer, uint16_t length)
{
    return drvUartReceive(linkId, buffer, length);
}

static uint32_t fc41dPortGetTickMs(void)
{
    return systemGetTickMs();
}

static void fc41dPortControlInit(uint8_t resetPin)
{
    drvGpioInit();
    drvGpioWrite(resetPin, DRVGPIO_PIN_RESET);
}

static void fc41dPortControlSetResetLevel(uint8_t resetPin, bool isActive)
{
    drvGpioWrite(resetPin, isActive ? DRVGPIO_PIN_SET : DRVGPIO_PIN_RESET);
}

static const stFc41dTransportInterface gFc41dTransportInterface = {
    .init = fc41dPortTransportInit,
    .write = fc41dPortTransportWrite,
    .getRxLen = fc41dPortTransportGetRxLen,
    .read = fc41dPortTransportRead,
    .getTickMs = fc41dPortGetTickMs,
};

static const stFc41dControlInterface gFc41dControlInterface = {
    .init = fc41dPortControlInit,
    .setResetLevel = fc41dPortControlSetResetLevel,
};

void fc41dLoadPlatformDefaultCfg(eFc41dMapType device, stFc41dCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->linkId = DRVUART_WIFI;
    cfg->resetPin = DRVGPIO_RESET_WIFI;
    cfg->rxPollChunkSize = FC41D_RX_POLL_CHUNK_SIZE;
    cfg->txTimeoutMs = FC41D_DEFAULT_TX_TIMEOUT_MS;
    cfg->bootWaitMs = FC41D_DEFAULT_BOOT_WAIT_MS;
    cfg->resetPulseMs = FC41D_DEFAULT_RESET_PULSE_MS;
    cfg->resetWaitMs = FC41D_DEFAULT_RESET_WAIT_MS;
    cfg->readyTimeoutMs = FC41D_DEFAULT_READY_TIMEOUT_MS;
    cfg->readySettleMs = FC41D_DEFAULT_READY_SETTLE_MS;
    cfg->retryIntervalMs = FC41D_DEFAULT_RETRY_INTERVAL_MS;
}

const stFc41dTransportInterface *fc41dGetPlatformTransportInterface(const stFc41dCfg *cfg)
{
    if (!fc41dPlatformIsValidCfg(cfg)) {
        return NULL;
    }

    return &gFc41dTransportInterface;
}

const stFc41dControlInterface *fc41dGetPlatformControlInterface(eFc41dMapType device)
{
    (void)device;
    return &gFc41dControlInterface;
}

bool fc41dPlatformIsValidCfg(const stFc41dCfg *cfg)
{
    return (cfg != NULL) && (cfg->linkId == DRVUART_WIFI) && (cfg->resetPin == DRVGPIO_RESET_WIFI) &&
           (cfg->rxPollChunkSize > 0U);
}

/**************************End of file********************************/
