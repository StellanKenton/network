/***********************************************************************************
* @file     : esp32c5_port.c
* @brief    : Project-side ESP32-C5 transport binding.
* @details  : Maps the reusable ESP32-C5 BLE module to DRVUART_WIFI and
*             DRVGPIO_WIFI_EN in the current project.
* @author   : GitHub Copilot
* @date     : 2026-04-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "esp32c5_port.h"

#include "drvgpio_port.h"
#include "drvuart_port.h"
#include "../../rep/driver/drvgpio/drvgpio.h"
#include "../../rep/driver/drvuart/drvuart.h"
#include "../../rep/service/rtos/rtos.h"

static eDrvStatus esp32c5PortTransportInit(uint8_t linkId)
{
    return drvUartInit(linkId);
}

static eDrvStatus esp32c5PortTransportWrite(uint8_t linkId, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    return drvUartTransmit(linkId, buffer, length, timeoutMs);
}

static uint16_t esp32c5PortTransportGetRxLen(uint8_t linkId)
{
    return drvUartGetDataLen(linkId);
}

static eDrvStatus esp32c5PortTransportRead(uint8_t linkId, uint8_t *buffer, uint16_t length)
{
    return drvUartReceive(linkId, buffer, length);
}

static uint32_t esp32c5PortGetTickMs(void)
{
    return repRtosGetTickMs();
}

static void esp32c5PortControlInit(uint8_t resetPin)
{
    drvGpioInit();
    drvGpioWrite(resetPin, DRVGPIO_PIN_RESET);
}

static void esp32c5PortSetResetLevel(uint8_t resetPin, bool isActive)
{
    drvGpioWrite(resetPin, isActive ? DRVGPIO_PIN_RESET : DRVGPIO_PIN_SET);
}

static const stEsp32c5TransportInterface gEsp32c5TransportInterface = {
    .init = esp32c5PortTransportInit,
    .write = esp32c5PortTransportWrite,
    .getRxLen = esp32c5PortTransportGetRxLen,
    .read = esp32c5PortTransportRead,
    .getTickMs = esp32c5PortGetTickMs,
};

static const stEsp32c5ControlInterface gEsp32c5ControlInterface = {
    .init = esp32c5PortControlInit,
    .setResetLevel = esp32c5PortSetResetLevel,
};

void esp32c5LoadPlatformDefaultCfg(eEsp32c5MapType device, stEsp32c5Cfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->linkId = DRVUART_WIFI;
    cfg->resetPin = DRVGPIO_WIFI_EN;
    cfg->rxPollChunkSize = ESP32C5_RX_POLL_CHUNK_SIZE;
    cfg->txTimeoutMs = ESP32C5_DEFAULT_TX_TIMEOUT_MS;
    cfg->bootWaitMs = ESP32C5_DEFAULT_BOOT_WAIT_MS;
    cfg->resetPulseMs = ESP32C5_DEFAULT_RESET_PULSE_MS;
    cfg->resetWaitMs = ESP32C5_DEFAULT_RESET_WAIT_MS;
    cfg->readyTimeoutMs = ESP32C5_DEFAULT_READY_TIMEOUT_MS;
    cfg->readyProbeMs = ESP32C5_DEFAULT_READY_PROBE_MS;
    cfg->retryIntervalMs = ESP32C5_DEFAULT_RETRY_INTERVAL_MS;
}

const stEsp32c5TransportInterface *esp32c5GetPlatformTransportInterface(const stEsp32c5Cfg *cfg)
{
    if (!esp32c5PlatformIsValidCfg(cfg)) {
        return NULL;
    }

    return &gEsp32c5TransportInterface;
}

const stEsp32c5ControlInterface *esp32c5GetPlatformControlInterface(eEsp32c5MapType device)
{
    (void)device;
    return &gEsp32c5ControlInterface;
}

bool esp32c5PlatformIsValidCfg(const stEsp32c5Cfg *cfg)
{
    return (cfg != NULL) && (cfg->linkId == DRVUART_WIFI) && (cfg->resetPin == DRVGPIO_WIFI_EN) &&
           (cfg->rxPollChunkSize > 0U);
}

/**************************End of file********************************/
