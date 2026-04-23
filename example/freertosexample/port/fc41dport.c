/***********************************************************************************
* @file     : fc41dport.c
* @brief    : Project-side FC41D binding implementation.
* @details  : Provides FC41D default assembly, UART transport binding, and reset
*             control hooks for the current project.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "fc41dport.h"

#include <string.h>

#include "main.h"

#include "drvgpio.h"
#include "drvgpio_port.h"
#include "drvuart.h"

#include "../../rep/service/rtos/rtos.h"
#include "drvuart_port.h"

static eDrvStatus fc41dPortTransportInit(uint8_t linkId);
static eDrvStatus fc41dPortTransportWrite(uint8_t linkId, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
static uint16_t fc41dPortTransportGetRxLen(uint8_t linkId);
static eDrvStatus fc41dPortTransportRead(uint8_t linkId, uint8_t *buffer, uint16_t length);
static uint32_t fc41dPortTransportGetTickMs(void);
static void fc41dPortControlInit(uint8_t resetPin);
static void fc41dPortControlSetResetLevel(uint8_t resetPin, bool isActive);

static const stFc41dTransportInterface gFc41dPortTransportInterface = {
    .init = fc41dPortTransportInit,
    .write = fc41dPortTransportWrite,
    .getRxLen = fc41dPortTransportGetRxLen,
    .read = fc41dPortTransportRead,
    .getTickMs = fc41dPortTransportGetTickMs,
};

static const stFc41dControlInterface gFc41dPortControlInterface = {
    .init = fc41dPortControlInit,
    .setResetLevel = fc41dPortControlSetResetLevel,
};

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

static uint32_t fc41dPortTransportGetTickMs(void)
{
    if (repRtosIsSchedulerRunning()) {
        return repRtosGetTickMs();
    }

    return HAL_GetTick();
}

static void fc41dPortControlInit(uint8_t resetPin)
{
    drvGpioInit();
    drvGpioWrite(resetPin, DRVGPIO_PIN_SET);
}

static void fc41dPortControlSetResetLevel(uint8_t resetPin, bool isActive)
{
    drvGpioWrite(resetPin, isActive ? DRVGPIO_PIN_RESET : DRVGPIO_PIN_SET);
}

void fc41dLoadPlatformDefaultCfg(eFc41dMapType device, stFc41dCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->linkId = DRVUART_WIFI;
    cfg->resetPin = DRVGPIO_RESET_WIFI;
}

const stFc41dTransportInterface *fc41dGetPlatformTransportInterface(const stFc41dCfg *cfg)
{
    if (!fc41dPlatformIsValidCfg(cfg)) {
        return NULL;
    }

    return &gFc41dPortTransportInterface;
}

const stFc41dControlInterface *fc41dGetPlatformControlInterface(eFc41dMapType device)
{
    if (device != FC41D_DEV0) {
        return NULL;
    }

    return &gFc41dPortControlInterface;
}

bool fc41dPlatformIsValidCfg(const stFc41dCfg *cfg)
{
    return (cfg != NULL) && (cfg->linkId < DRVUART_MAX) && (cfg->resetPin < DRVGPIO_MAX);
}

/**************************End of file********************************/
