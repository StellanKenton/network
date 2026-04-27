/************************************************************************************
* @file     : ec800m_port.c
* @brief    : Project-side EC800M transport binding.
* @details  : Maps EC800M-CN to USB cellular link and PWRKEY/RESET pins.
* @author   : GitHub Copilot
* @date     : 2026-04-27
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "ec800m_port.h"

#include "drvgpio_port.h"
#include "drvusb_port.h"
#include "../../rep/driver/drvgpio/drvgpio.h"
#include "../../rep/service/rtos/rtos.h"
#include "../bsp/bspusb_ec800.h"

static eDrvStatus ec800mPortTransportInit(uint8_t linkId)
{
    return bspUsbEc800Init(linkId);
}

static eDrvStatus ec800mPortTransportWrite(uint8_t linkId, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    return bspUsbEc800Write(linkId, buffer, length, timeoutMs);
}

static uint16_t ec800mPortTransportGetRxLen(uint8_t linkId)
{
    return bspUsbEc800GetRxLen(linkId);
}

static eDrvStatus ec800mPortTransportRead(uint8_t linkId, uint8_t *buffer, uint16_t length)
{
    return bspUsbEc800Read(linkId, buffer, length);
}

static uint32_t ec800mPortGetTickMs(void)
{
    return repRtosGetTickMs();
}

static void ec800mPortControlInit(uint8_t pwrkeyPin, uint8_t resetPin)
{
    drvGpioInit();
    drvGpioWrite(resetPin, DRVGPIO_PIN_RESET);
    drvGpioWrite(pwrkeyPin, DRVGPIO_PIN_RESET);
}

static void ec800mPortSetActiveLowLevel(uint8_t pin, bool isActive)
{
    drvGpioWrite(pin, isActive ? DRVGPIO_PIN_RESET : DRVGPIO_PIN_SET);
}

static void ec800mPortSetResetLevel(uint8_t pin, bool isActive)
{
    drvGpioWrite(pin, isActive ? DRVGPIO_PIN_RESET : DRVGPIO_PIN_SET);
}

static const stEc800mTransportInterface gEc800mTransportInterface = {
    .init = ec800mPortTransportInit,
    .write = ec800mPortTransportWrite,
    .getRxLen = ec800mPortTransportGetRxLen,
    .read = ec800mPortTransportRead,
    .getTickMs = ec800mPortGetTickMs,
};

static const stEc800mControlInterface gEc800mControlInterface = {
    .init = ec800mPortControlInit,
    .setPwrkeyLevel = ec800mPortSetActiveLowLevel,
    .setResetLevel = ec800mPortSetResetLevel,
};

void ec800mLoadPlatformDefaultCfg(eEc800mMapType device, stEc800mCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->linkId = DRVUSB_CELLULAR;
    cfg->pwrkeyPin = DRVGPIO_CELLULAR_PWRKEY;
    cfg->resetPin = DRVGPIO_CELLULAR_RESET;
    cfg->rxPollChunkSize = EC800M_RX_POLL_CHUNK_SIZE;
    cfg->txTimeoutMs = EC800M_DEFAULT_TX_TIMEOUT_MS;
    cfg->bootWaitMs = EC800M_DEFAULT_BOOT_WAIT_MS;
    cfg->pwrkeyPulseMs = EC800M_DEFAULT_PWRKEY_PULSE_MS;
    cfg->resetPulseMs = EC800M_DEFAULT_RESET_PULSE_MS;
    cfg->resetWaitMs = EC800M_DEFAULT_RESET_WAIT_MS;
    cfg->readyTimeoutMs = EC800M_DEFAULT_READY_TIMEOUT_MS;
    cfg->retryIntervalMs = EC800M_DEFAULT_RETRY_INTERVAL_MS;
}

const stEc800mTransportInterface *ec800mGetPlatformTransportInterface(const stEc800mCfg *cfg)
{
    if (!ec800mPlatformIsValidCfg(cfg)) {
        return NULL;
    }

    return &gEc800mTransportInterface;
}

const stEc800mControlInterface *ec800mGetPlatformControlInterface(eEc800mMapType device)
{
    (void)device;
    return &gEc800mControlInterface;
}

bool ec800mPlatformIsValidCfg(const stEc800mCfg *cfg)
{
    return (cfg != NULL) && (cfg->linkId == DRVUSB_CELLULAR) &&
           (cfg->pwrkeyPin == DRVGPIO_CELLULAR_PWRKEY) &&
           (cfg->resetPin == DRVGPIO_CELLULAR_RESET) &&
           (cfg->rxPollChunkSize > 0U);
}

/**************************End of file********************************/
