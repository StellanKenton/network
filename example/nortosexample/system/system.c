/***********************************************************************************
* @file     : system.c
* @brief    : Shared system mode storage.
* @details  : Stores the current bootloader system mode and exposes version text.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "system.h"

#include "../../rep/service/log/log.h"
#include "stm32f10x.h"

#define SYSTEM_MODE_LOG_TAG    "SysMgr"

static System_Mode_EnumDef gSystemMode = E_SYSTEM_INIT_MODE;
static volatile uint32_t gSystemTickMs = 0U;

bool systemIsValidMode(System_Mode_EnumDef mode)
{
    return mode < E_SYSTEM_MODE_MAX;
}

System_Mode_EnumDef systemGetMode(void)
{
    return gSystemMode;
}

bool systemSetMode(System_Mode_EnumDef mode)
{
    System_Mode_EnumDef lOldMode = gSystemMode;

    if (!systemIsValidMode(mode)) {
        return false;
    }

    if (lOldMode == mode) {
        return true;
    }

    gSystemMode = mode;
    LOG_I(SYSTEM_MODE_LOG_TAG,
          "mode changed: %s -> %s",
          systemGetModeString(lOldMode),
          systemGetModeString(mode));
    return true;
}

bool systemTickInit(void)
{
    gSystemTickMs = 0U;

    if (SystemCoreClock == 0U) {
        SystemCoreClockUpdate();
    }

    if (SystemCoreClock == 0U) {
        return false;
    }

    return SysTick_Config(SystemCoreClock / 1000U) == 0U;
}

void systemTickHandler(void)
{
    gSystemTickMs++;
}

uint32_t systemGetTickMs(void)
{
    return gSystemTickMs;
}

void systemDelayMs(uint32_t delayMs)
{
    uint32_t lStartTick = systemGetTickMs();

    while ((systemGetTickMs() - lStartTick) < delayMs) {
    }
}

const char *systemGetModeString(System_Mode_EnumDef mode)
{
    switch (mode) {
        case E_SYSTEM_INIT_MODE:
            return "INIT_MODE";
        case E_SYSTEM_CHECK_MODE:
            return "CHECK_MODE";
        case E_SYSTEM_STANDBY_MODE:
            return "STANDBY_MODE";
        case E_SYSTEM_UPDATE_MODE:
            return "UPDATE_MODE";
        default:
            return "UNKNOWN_MODE";
    }
}

const char *systemGetFirmwareName(void)
{
    return FIRMWARE_NAME;
}

const char *systemGetFirmwareVersion(void)
{
    return FIRMWARE_VERSION;
}

const char *systemGetHardwareVersion(void)
{
    return HARDWARE_VERSION;
}

/**************************End of file********************************/
