/***********************************************************************************
* @file     : power.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "power.h"

#include "main.h"
#include "drvadc.h"
#include "system.h"
#include "../port/pca9535_port.h"

static uint16_t powerCalcVoltage(eDrvAdcPortMap channel, uint16_t rawValue);
static uint8_t powerCalcBatteryLevel(uint16_t batteryMv);
static bool powerLedShouldDisplayInMode(eSystemMode mode, bool isCharging);
static bool powerLedIsBlinkOn(void);

static PowerManager gPowerManager = {
    .status = {
        .state = ePOWER_STATE_UNINIT,
        .isShutDownRequested = false,
    },
};

static const stPowerChannelMap gPowerChannelMap[] = {
    {
        .channel = DRVADC_BAT,
        .rawValue = &gPowerManager.raw.battery,
        .voltageValue = &gPowerManager.voltage.batteryMv,
    },
    {
        .channel = DRVADC_DC,
        .rawValue = &gPowerManager.raw.dc,
        .voltageValue = &gPowerManager.voltage.dcMv,
    },
    {
        .channel = DRVADC_5V0,
        .rawValue = &gPowerManager.raw.v5v0,
        .voltageValue = &gPowerManager.voltage.v5v0Mv,
    },
    {
        .channel = DRVADC_3V3,
        .rawValue = &gPowerManager.raw.v3v3,
        .voltageValue = &gPowerManager.voltage.v3v3Mv,
    },
};

static uint16_t powerCalcVoltage(eDrvAdcPortMap channel, uint16_t rawValue)
{
    uint32_t lVoltage = 0U;

    switch (channel) {
        case DRVADC_BAT:
            lVoltage = ((uint32_t)rawValue * 205260U) / 253890U;
            break;

        case DRVADC_DC:
            lVoltage = ((uint32_t)rawValue * 205260U) / 253890U;
            break;

        case DRVADC_5V0:
            lVoltage = ((uint32_t)rawValue * 14850U) / 61425U;
            break;

        case DRVADC_3V3:
            lVoltage = ((uint32_t)rawValue * 13200U) / 40950U;
            break;

        default:
            break;
    }

    return (uint16_t)lVoltage;
}

static uint8_t powerCalcBatteryLevel(uint16_t batteryMv)
{
    if (batteryMv >= 4100U) {
        return 5U;
    }

    if (batteryMv >= 3950U) {
        return 4U;
    }

    if (batteryMv >= 3820U) {
        return 3U;
    }

    if (batteryMv >= 3700U) {
        return 2U;
    }

    if (batteryMv >= 3550U) {
        return 1U;
    }

    return 0U;
}

static bool powerLedShouldDisplayInMode(eSystemMode mode, bool isCharging)
{
    if ((mode == eSYSTEM_INIT_MODE) || (mode == eSYSTEM_POWERUP_SELFCHECK_MODE)) {
        return isCharging;
    }

    return true;
}

static bool powerLedIsBlinkOn(void)
{
    return ((HAL_GetTick() / POWER_LED_BLINK_HALF_PERIOD_MS) & 0x01U) == 0U;
}

bool powerInit(void)
{
    if ((gPowerManager.status.state == ePOWER_STATE_UNINIT) || (gPowerManager.status.state == ePOWER_STATE_FAULT)) {
        gPowerManager.status.state = ePOWER_STATE_READY;
        gPowerManager.status.isShutDownRequested = false;
    }

    return true;
}

uint16_t powerGetVoltage(eDrvAdcPortMap channel)
{
    uint16_t lAdcValue = 0U;

    if (drvAdcReadRaw((uint8_t)channel, &lAdcValue) != DRV_STATUS_OK) {
        return 0U;
    }

    return powerCalcVoltage(channel, lAdcValue);
}

void powerBatteryUpdate(void)
{
    uint8_t lNewLevel = powerCalcBatteryLevel(gPowerManager.voltage.batteryMv);

    if (lNewLevel < gPowerManager.BatLevel) {
        gPowerManager.BatLevel = lNewLevel;
        return;
    }

    if ((lNewLevel > gPowerManager.BatLevel) && (gPowerManager.voltage.dcMv > 4500U)) {
        gPowerManager.BatLevel = lNewLevel;
    }
}

uint8_t powerBatteryGet(void)
{
    return gPowerManager.BatLevel;
}

void powerLedProcess(void)
{
    eSystemMode lMode = systemGetMode();
    uint16_t lDcMv = gPowerManager.voltage.dcMv;
    uint8_t lBatteryLevel = powerBatteryGet();
    bool lIsCharging = lDcMv > POWER_CHARGE_THRESHOLD_MV;
    bool lIsRedOn = false;
    bool lIsGreenOn = false;

    if (!powerLedShouldDisplayInMode(lMode, lIsCharging)) {
        (void)pca9535PortLedPowerShow(false, false, false);
        return;
    }

    if (lIsCharging) {
        if (lBatteryLevel >= POWER_BATTERY_FULL_LEVEL) {
            lIsGreenOn = true;
        } else {
            lIsGreenOn = powerLedIsBlinkOn();
        }
    } else if (lBatteryLevel <= POWER_BATTERY_LOW_LEVEL_MAX) {
        lIsRedOn = true;
    } else {
        lIsGreenOn = true;
    }

    (void)pca9535PortLedPowerShow(lIsRedOn, lIsGreenOn, false);
}

void powerTransRawToVoltage(void)
{
    uint8_t lIndex;
    stDrvAdcData *lAdcData;

    lAdcData = drvAdcGetPlatformData();
    if (lAdcData == NULL) {
        for (lIndex = 0U; lIndex < (uint8_t)(sizeof(gPowerChannelMap) / sizeof(gPowerChannelMap[0])); lIndex++) {
            *gPowerChannelMap[lIndex].rawValue = 0U;
            *gPowerChannelMap[lIndex].voltageValue = 0U;
        }
        return;
    }

    for (lIndex = 0U; lIndex < (uint8_t)(sizeof(gPowerChannelMap) / sizeof(gPowerChannelMap[0])); lIndex++) {
        uint16_t lRawValue = lAdcData[gPowerChannelMap[lIndex].channel].raw;

        *gPowerChannelMap[lIndex].rawValue = lRawValue;
        *gPowerChannelMap[lIndex].voltageValue = powerCalcVoltage(gPowerChannelMap[lIndex].channel, lRawValue);
    }
}

void powerProcess(void)
{
    if ((gPowerManager.status.state != ePOWER_STATE_READY) &&
        (gPowerManager.status.state != ePOWER_STATE_NORMAL) &&
        (gPowerManager.status.state != ePOWER_STATE_STOPPED)) {
        return;
    }

    powerTransRawToVoltage();
    powerBatteryUpdate();
    gPowerManager.status.state = gPowerManager.status.isShutDownRequested ? ePOWER_STATE_STOPPED : ePOWER_STATE_NORMAL;
}

bool powerRequestShutDown(void)
{
    if ((gPowerManager.status.state == ePOWER_STATE_UNINIT) || (gPowerManager.status.state == ePOWER_STATE_FAULT)) {
        return false;
    }

    gPowerManager.status.isShutDownRequested = true;
    return true;
}

bool powerRequestPowerUp(void)
{
    if ((gPowerManager.status.state == ePOWER_STATE_UNINIT) || (gPowerManager.status.state == ePOWER_STATE_FAULT)) {
        return false;
    }

    gPowerManager.status.isShutDownRequested = false;
    return true;
}

const stPowerStatus *powerGetStatus(void)
{
    return &gPowerManager.status;
}

const PowerManager *powerGetManager(void)
{
    return &gPowerManager;
}

/**************************End of file********************************/
