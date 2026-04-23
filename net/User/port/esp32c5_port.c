/***********************************************************************************
* @file     : esp32c5_port.c
* @brief    : Project-side ESP32-C5 module binding.
**********************************************************************************/
#include "esp32c5_port.h"

#include "../../rep/driver/drvgpio/drvgpio.h"
#include "drvgpio_port.h"
#include "../../rep/driver/drvuart/drvuart.h"
#include "drvuart_port.h"

#include "../../rep/service/rtos/rtos.h"

static eDrvStatus esp32c5PortTransportInit(uint8_t linkId);
static eDrvStatus esp32c5PortTransportWrite(uint8_t linkId, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
static uint16_t esp32c5PortTransportGetRxLen(uint8_t linkId);
static eDrvStatus esp32c5PortTransportRead(uint8_t linkId, uint8_t *buffer, uint16_t length);
static uint32_t esp32c5PortGetTickMs(void);
static void esp32c5PortControlInit(uint8_t resetPin);
static void esp32c5PortSetResetLevel(uint8_t resetPin, bool isActive);

static const stEsp32c5TransportInterface gEsp32c5PortTransportInterface = {
    esp32c5PortTransportInit,
    esp32c5PortTransportWrite,
    esp32c5PortTransportGetRxLen,
    esp32c5PortTransportRead,
    esp32c5PortGetTickMs,
};

static const stEsp32c5ControlInterface gEsp32c5PortControlInterface = {
    esp32c5PortControlInit,
    esp32c5PortSetResetLevel,
};

static eDrvStatus esp32c5PortTransportInit(uint8_t linkId)
{
    drvGpioInit();
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
    drvGpioWrite(resetPin, DRVGPIO_PIN_SET);
}

static void esp32c5PortSetResetLevel(uint8_t resetPin, bool isActive)
{
    drvGpioWrite(resetPin, isActive ? DRVGPIO_PIN_RESET : DRVGPIO_PIN_SET);
}

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
    cfg->bootWaitMs = 1200U;
    cfg->resetPulseMs = 100U;
    cfg->resetWaitMs = 600U;
    cfg->readyTimeoutMs = 5000U;
    cfg->readySettleMs = 200U;
    cfg->retryIntervalMs = ESP32C5_DEFAULT_RETRY_INTERVAL_MS;
}

const stEsp32c5TransportInterface *esp32c5GetPlatformTransportInterface(const stEsp32c5Cfg *cfg)
{
    return esp32c5PlatformIsValidCfg(cfg) ? &gEsp32c5PortTransportInterface : NULL;
}

const stEsp32c5ControlInterface *esp32c5GetPlatformControlInterface(eEsp32c5MapType device)
{
    (void)device;
    return &gEsp32c5PortControlInterface;
}

bool esp32c5PlatformIsValidCfg(const stEsp32c5Cfg *cfg)
{
    if (cfg == NULL) {
        return false;
    }

    return (cfg->linkId == DRVUART_WIFI) && (cfg->resetPin == DRVGPIO_WIFI_EN);
}
/**************************End of file********************************/
