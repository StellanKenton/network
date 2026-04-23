/***********************************************************************************
* @file     : system_debug.c
* @brief    : System debug and console command implementation.
* @details  : Hosts system console commands and optional module debug command
*             registration for the bootloader system layer.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "system_debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "stm32f10x.h"
#include "../../rep/service/log/log.h"
#include "system.h"
#include "systask.h"

#define SYSTEM_DEBUG_LOG_TAG    "system"
#define SYSTEM_DEBUG_REPLY_BUFFER_SIZE    160U

static int32_t systemDebugReply(uint32_t transport, const char *format, ...)
{
    char lBuffer[SYSTEM_DEBUG_REPLY_BUFFER_SIZE];
    int lLength;
    va_list lArgs;

    if (format == NULL) {
        return 0;
    }

    va_start(lArgs, format);
    lLength = vsnprintf(lBuffer, sizeof(lBuffer), format, lArgs);
    va_end(lArgs);

    if (lLength < 0) {
        return 0;
    }

    if (lLength >= (int)sizeof(lBuffer)) {
        lLength = (int)sizeof(lBuffer) - 1;
        lBuffer[lLength] = '\0';
    }

    if ((lLength == 0) || (lBuffer[lLength - 1] != '\n')) {
        if (lLength >= ((int)sizeof(lBuffer) - 1)) {
            lLength = (int)sizeof(lBuffer) - 2;
        }
        lBuffer[lLength] = '\n';
        lLength++;
        lBuffer[lLength] = '\0';
    }

    return LOG_T(transport, lBuffer, (uint16_t)lLength);
}

static eConsoleCommandResult systemDebugReplyHelp(uint32_t transport)
{
    if (systemDebugReply(transport, "Usage: system <info|mode|tick|reboot|help>") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemDebugHandleInfo(uint32_t transport)
{
    stSystemTaskSnapshot lSnapshot;

    systemTaskGetSnapshot(&lSnapshot);
    if (systemDebugReply(transport,
                         "fw=%s hw=%s mode=%s tick=%lu uptime=%lu heartbeat=%lu",
                         systemGetFirmwareVersion(),
                         systemGetHardwareVersion(),
                         systemGetModeString(systemGetMode()),
                         (unsigned long)systemGetTickMs(),
                         (unsigned long)lSnapshot.uptimeMs,
                         (unsigned long)lSnapshot.heartbeat) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemDebugHandleMode(uint32_t transport)
{
    if (systemDebugReply(transport, "%s", systemGetModeString(systemGetMode())) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemDebugHandleTick(uint32_t transport)
{
    if (systemDebugReply(transport, "%lu", (unsigned long)systemGetTickMs()) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemDebugHandleReboot(uint32_t transport)
{
    (void)systemDebugReply(transport, "rebooting...");
    NVIC_SystemReset();
    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult systemDebugRebootAliasHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    return systemDebugHandleReboot(transport);
}

static eConsoleCommandResult systemDebugConsoleHandler(uint32_t transport, int argc, char *argv[])
{
    if ((argc < 2) || (argv == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "info") == 0) {
        return systemDebugHandleInfo(transport);
    }

    if (strcmp(argv[1], "mode") == 0) {
        return systemDebugHandleMode(transport);
    }

    if (strcmp(argv[1], "tick") == 0) {
        return systemDebugHandleTick(transport);
    }

    if (strcmp(argv[1], "reboot") == 0) {
        return systemDebugHandleReboot(transport);
    }

    if (strcmp(argv[1], "help") == 0) {
        return systemDebugReplyHelp(transport);
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}

static const stConsoleCommand gSystemDebugConsoleCommand = {
    .commandName = "system",
    .helpText = "system <info|mode|tick|reboot|help>",
    .ownerTag = SYSTEM_DEBUG_LOG_TAG,
    .handler = systemDebugConsoleHandler,
};

static const stConsoleCommand gSystemDebugRebootAliasCommand = {
    .commandName = "reboot",
    .helpText = "reboot",
    .ownerTag = SYSTEM_DEBUG_LOG_TAG,
    .handler = systemDebugRebootAliasHandler,
};

bool systemDebugConsoleRegister(void)
{
#if (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1)
    if (!logRegisterConsole(&gSystemDebugConsoleCommand)) {
        return false;
    }

    return logRegisterConsole(&gSystemDebugRebootAliasCommand);
#else
    return true;
#endif
}
/**************************End of file********************************/
