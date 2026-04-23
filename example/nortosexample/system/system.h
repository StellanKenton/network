/************************************************************************************
* @file     : system.h
* @brief    : Shared system mode and version declarations.
* @details  : Keeps public firmware version strings and current mode accessors in
*             one place for the bootloader system layer.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_SYSTEM_H
#define CPRSENSORBOOT_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYSTEM_STRINGIFY_IMPL(value)    #value
#define SYSTEM_STRINGIFY(value)         SYSTEM_STRINGIFY_IMPL(value)

#define FIRMWARE_NAME                   "CPRSensorBoot"

#define FW_VER_MAJOR                    1
#define FW_VER_MINOR                    0
#define FW_VER_PATCH                    0

#define HW_VER_MAJOR                    1
#define HW_VER_MINOR                    0
#define HW_VER_PATCH                    0

#define FIRMWARE_VERSION                "V" SYSTEM_STRINGIFY(FW_VER_MAJOR) "." SYSTEM_STRINGIFY(FW_VER_MINOR) "." SYSTEM_STRINGIFY(FW_VER_PATCH)
#define HARDWARE_VERSION                "v" SYSTEM_STRINGIFY(HW_VER_MAJOR) "." SYSTEM_STRINGIFY(HW_VER_MINOR) "." SYSTEM_STRINGIFY(HW_VER_PATCH)



typedef enum {
    E_SYSTEM_INIT_MODE = 0,    // initial state before mode is set
    E_SYSTEM_CHECK_MODE,
    E_SYSTEM_STANDBY_MODE,
    E_SYSTEM_UPDATE_MODE,
    E_SYSTEM_MODE_MAX,
} System_Mode_EnumDef;

bool systemIsValidMode(System_Mode_EnumDef mode);
System_Mode_EnumDef systemGetMode(void);
bool systemSetMode(System_Mode_EnumDef mode);
bool systemTickInit(void);
void systemTickHandler(void);
uint32_t systemGetTickMs(void);
void systemDelayMs(uint32_t delayMs);
const char *systemGetModeString(System_Mode_EnumDef mode);
const char *systemGetFirmwareName(void);
const char *systemGetFirmwareVersion(void);
const char *systemGetHardwareVersion(void);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_SYSTEM_H
/**************************End of file********************************/
