/***********************************************************************************
* @file     : sysmgr.c
* @brief    : System manager implementation.
* @details  : Bridges the main loop, cooperative scheduler, watchdog, and RTT
*             console entry points.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "sysmgr.h"

#include "../../rep/driver/drvusb/drvusb.h"
#include "../../rep/service/log/log.h"
#include "../bsp/bsprtt.h"
#include "../port/drvusb_port.h"
#include "system_debug.h"
#include "systask.h"

static stSystemManagerState gSystemManagerState;

static void systemManagerTraceRaw(const char *text)
{
    uint16_t lLength;

    if (text == NULL) {
        return;
    }

    lLength = logGetTextLength(text);
    if (lLength == 0U) {
        return;
    }

    bspRttLogInit();
    (void)bspRttLogWrite((const uint8_t *)text, lLength);
}

bool systemManagerInit(void)
{
    uint32_t lNowTick;

    systemManagerTraceRaw("boot: systemManagerInit enter\r\n");

    if(!systemTickInit()) {
        systemManagerTraceRaw("boot: system tick init failed\r\n");
        LOG_E(SYSTEM_MANAGER_LOG_TAG, "system tick init failed");
        return false;
    }

    lNowTick = systemGetTickMs();
    
    gSystemManagerState.isConsoleReady = false;
    gSystemManagerState.isWatchdogActive = false;
    gSystemManagerState.watchdogTick = 0U;
    gSystemManagerState.lastBackgroundTaskTick = lNowTick;
    gSystemManagerState.lastFsmTick = lNowTick;
    gSystemManagerState.lastConsoleAnnounceTick = 0U;
    gSystemManagerState.consoleAnnounceCount = 0U;

    if (!systemTaskSchedulerInit()) {
        systemManagerTraceRaw("boot: task scheduler init failed\r\n");
        LOG_E(SYSTEM_MANAGER_LOG_TAG, "system task scheduler init failed");
        return false;
    }

    if (!systemSetMode(E_SYSTEM_INIT_MODE)) {
        systemManagerTraceRaw("boot: set init mode failed\r\n");
        return false;
    }

    if (!logInit()) {
        systemManagerTraceRaw("boot: log init failed\r\n");
        return false;
    }

    if (!systemDebugConsoleRegister()) {
        systemManagerTraceRaw("boot: console register failed\r\n");
        return false;
    }
    systemManagerTraceRaw("boot: systemManagerInit done\r\n");

    gSystemManagerState.isConsoleReady = true;
    gSystemManagerState.isInitialized = true;
    LOG_T("BootLoader Running\r\n");
    LOG_I(SYSTEM_MANAGER_LOG_TAG, "log and console ready on RTT");
    LOG_I(SYSTEM_MANAGER_LOG_TAG, "usb cdc ready on dev%u", (unsigned int)DRVUSB_DEV0);
    return true;
}

static void systemManagerProcessInitMode(void)
{
    LOG_I(SYSTEM_MANAGER_LOG_TAG, "&&&&&&&&&&&&&&&&& BOOTLOADER POWER UP &&&&&&&&&&&&&&&&&");
    LOG_I(SYSTEM_MANAGER_LOG_TAG, "Firmware: %s, Version: %s", FIRMWARE_NAME, FIRMWARE_VERSION);
    (void)systemSetMode(E_SYSTEM_CHECK_MODE);
}

static void systemManagerProcessCheckMode(void)
{
    LOG_I(SYSTEM_MANAGER_LOG_TAG, "bootloader checking jump flag");
    (void)systemSetMode(E_SYSTEM_STANDBY_MODE);
}

static void systemManagerProcessStandbyMode(void)
{
    (void)systemSetMode(E_SYSTEM_UPDATE_MODE);
}

static void systemManagerProcessUpdateMode(void)
{
    systemTaskSchedulerProcess();
}

static void systemManagerFSM(void)
{
    switch (systemGetMode()) {
        case E_SYSTEM_INIT_MODE:
            systemManagerProcessInitMode();
            break;
        case E_SYSTEM_CHECK_MODE:
            systemManagerProcessCheckMode();
            break;
        case E_SYSTEM_STANDBY_MODE:
            systemManagerProcessStandbyMode();
            break;
        case E_SYSTEM_UPDATE_MODE:
            systemManagerProcessUpdateMode();
            break;
        default:
            break;
    }
}


void systemBackGroundTask(void)
{
    if (gSystemManagerState.isConsoleReady) {
        ConsoleBackGournd();
    }
}

void systemManagerProcess(void)
{
    uint32_t lNowTick;

    if (!gSystemManagerState.isInitialized ) {
        return;
    }

    lNowTick = systemGetTickMs();
    // BackGround task runs every 5ms
    if ((lNowTick - gSystemManagerState.lastBackgroundTaskTick) >= SYSTEM_BACKGROUND_TASK_INTERVAL_MS) {
        gSystemManagerState.lastBackgroundTaskTick = lNowTick;
        systemBackGroundTask();
    }
    // System FSM runs every 1ms
    if ((lNowTick - gSystemManagerState.lastFsmTick) >= SYSTEM_FSM_INTERVAL_MS) {
        gSystemManagerState.lastFsmTick = lNowTick;
        systemManagerFSM();
    }
}
/**************************End of file********************************/
