/***********************************************************************************
* @file     : system.c
* @brief    : Project-side system mode storage.
* @details  : Provides the build entry expected by the current project files.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "system.h"

static eSystemMode gSystemMode = eSYSTEM_INIT_MODE;

/**
* @brief : Check whether the provided system mode is valid.
* @param : mode - target system mode.
* @return: true when the mode is within the supported range.
**/
bool systemIsValidMode(eSystemMode mode)
{
    return mode < eSYSTEM_MODE_MAX;
}

/**
* @brief : Get current system mode.
* @param : None
* @return: Current system mode.
**/
eSystemMode systemGetMode(void)
{
    return gSystemMode;
}

/**
* @brief : Set system mode.
* @param : mode - target system mode.
* @return: None
**/
void systemSetMode(eSystemMode mode)
{
    if (!systemIsValidMode(mode)) {
        return;
    }

    if (gSystemMode == mode) {
        return;
    }

    gSystemMode = mode;
}

/**
* @brief : Get string representation of system mode.
* @param : mode - system mode enum value.
* @return: String representation of the system mode.
**/
const char *systemGetModeString(eSystemMode mode)
{
    switch (mode) {
        case eSYSTEM_INIT_MODE:
            return "INIT_MODE";
        case eSYSTEM_POWERUP_SELFCHECK_MODE:
            return "POWERUP_SELFCHECK_MODE";
        case eSYSTEM_STANDBY_MODE:
            return "STANDBY_MODE";
        case eSYSTEM_NORMAL_MODE:
            return "NORMAL_MODE";
        case eSYSTEM_SELF_CHECK_MODE:
            return "SELF_CHECK_MODE";
        case eSYSTEM_UPDATE_MODE:
            return "UPDATE_MODE";
        case eSYSTEM_DIAGNOSTIC_MODE:
            return "DIAGNOSTIC_MODE";
        case eSYSTEM_EOL_MODE:
            return "EOL_MODE";
        default:
            return "UNKNOWN_MODE";
    }
}

/**
* @brief : Get firmware name string.
* @param : None
* @return: Firmware name string.
**/
const char *systemGetFirmwareName(void)
{
    return FIRMWARE_NAME;
}

/**
* @brief : Get firmware version string.
* @param : None
* @return: Firmware version string.
**/
const char *systemGetFirmwareVersion(void)
{
    return FIRMWARE_VERSION;
}

/**
* @brief : Get hardware version string.
* @param : None
* @return: Hardware version string.
**/
const char *systemGetHardwareVersion(void)
{
    return HARDWARE_VERSION;
}

/**************************End of file********************************/
