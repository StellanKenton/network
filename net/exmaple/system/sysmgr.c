/***********************************************************************************
* @file     : sysmgr.c
* @brief    : Project-side system mode manager.
* @details  : Dispatches the current system mode and performs one-time BSP startup.
* @author   : rumi 
* @date     : 2026/04/11
* @version  : v0.0.1
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "sysmgr.h"

#include "../../Core/Inc/adc.h"
#include "../../Core/Inc/dma.h"
#include "../../Core/Inc/gpio.h"
#include "../../Core/Inc/i2c.h"
#include "../../Core/Inc/iwdg.h"
#include "../../Core/Inc/rtc.h"
#include "../../Core/Inc/spi.h"
#include "../../Core/Inc/tim.h"
#include "../../Core/Inc/usart.h"

#include "drvgpio.h"
#include "../../rep/service/console/log.h"

#include "../manager/power/power.h"
#include "../manager/selfcheck/selfcheck.h"
#include "../manager/wireless/wireless.h"
#include "../port/pca9535_port.h"
#include "../port/tm1651_port.h"
#include "systask.h"
#include "system.h"
#include "../manager/comm/frameprocess/frameprocess.h"

#define SYSTEM_LOG_TAG "systemManager"

static bool gSystemInitModeCompleted = false;
static bool gSystemBspInitCompleted = false;

/**
* @brief : Run the generated STM32 BSP initialization sequence.
* @param : None
* @return: None
**/
static void systemInitBsp(void)
{
    if (gSystemBspInitCompleted) {
        return;
    }

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_I2C1_Init();
    MX_I2C2_Init();
    MX_RTC_Init();
    MX_SPI1_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    MX_TIM7_Init();
    MX_UART4_Init();
    MX_USART2_UART_Init();

    HAL_UART_MspInit(&huart4);
    HAL_UART_MspInit(&huart2);
    gSystemBspInitCompleted = true;
}

/**
* @brief : Run one-time BSP and basic manager initialization.
* @param : None
* @return: true when the required initialization steps succeed.
**/
static bool systemModuleInit(void)
{
    bool lIsReady = true;

    drvGpioInit();

    lIsReady = selfCheckInit() && lIsReady;
    LOG_I(SYSTEM_LOG_TAG, "selfcheck init %s", lIsReady ? "ok" : "fail");

    if (pca9535PortInit() == DRV_STATUS_OK) {
        selfCheckSetExpanderResult(true);
        (void)pca9535PortLedOff();
        LOG_I(SYSTEM_LOG_TAG, "pca9535 init ok");
    } else {
        selfCheckSetExpanderResult(false);
        lIsReady = false;
        LOG_E(SYSTEM_LOG_TAG, "pca9535 init fail");
    }

    if (tm1651PortInit() == DRV_STATUS_OK) {
        selfCheckSetDisplayResult(true);
        (void)tm1651PortClearDisplay();
        LOG_I(SYSTEM_LOG_TAG, "tm1651 init ok");
    } else {
        selfCheckSetDisplayResult(false);
        lIsReady = false;
        LOG_E(SYSTEM_LOG_TAG, "tm1651 init fail");
    }

    if (powerInit()) {
        selfCheckSetPowerResult(true);
        LOG_I(SYSTEM_LOG_TAG, "power init ok");
    } else {
        selfCheckSetPowerResult(false);
        lIsReady = false;
        LOG_E(SYSTEM_LOG_TAG, "power init fail");
    }

    if (wirelessInit()) {
        LOG_I(SYSTEM_LOG_TAG, "wireless init ok");

        if (frmProcInit(FRAME_PROC0) == FRM_PROC_STATUS_OK) {
            LOG_I(SYSTEM_LOG_TAG, "frameprocess init ok");
        } else {
            lIsReady = false;
            LOG_E(SYSTEM_LOG_TAG, "frameprocess init fail");
        }
    } else {
        lIsReady = false;
        LOG_E(SYSTEM_LOG_TAG, "wireless init fail");
    }

    return lIsReady;
}

/**
* @brief : Handle the system init mode.
* @param : None
* @return: None
**/
static void systemInitMode(void)
{
    if (gSystemInitModeCompleted) {
        return;
    }
    LOG_I(SYSTEM_LOG_TAG, "&&&&&&&&&&&&&&&&& SYSTEM POWER UP &&&&&&&&&&&&&&&&&");
    LOG_I(SYSTEM_LOG_TAG, "System initialized.");
    LOG_I(SYSTEM_LOG_TAG, "Firmware: %s, Version: %s, Hardware: %s", FIRMWARE_NAME, FIRMWARE_VERSION, HARDWARE_VERSION);
    
    systemInitBsp();
    
    if (!systemModuleInit()) {
        LOG_E(SYSTEM_LOG_TAG, "system init mode blocked");
        return;
    }

    gSystemInitModeCompleted = true;
    LOG_I(SYSTEM_LOG_TAG, "switch to powerup selfcheck mode");
    systemSetMode(eSYSTEM_POWERUP_SELFCHECK_MODE);
}

/**
* @brief : Handle the power-up self-check mode.
* @param : None
* @return: None
**/
static void systemPowerupSelfCheckMode(void)
{
    static bool lSelfCheckCompleted = false;

    // jump
    lSelfCheckCompleted = true;

    if(lSelfCheckCompleted) {
        (void)systaskCreateWorkerTasks();
        systemSetMode(eSYSTEM_STANDBY_MODE);
    }
}

/**
* @brief : Handle the standby mode.
* @param : None
* @return: None
**/
static void systemStandbyMode(void)
{
    systemSetMode(eSYSTEM_NORMAL_MODE);
}

/**
* @brief : Handle the normal mode.
* @param : None
* @return: None
**/
static void systemNormalMode(void)
{
    
}

/**
* @brief : Handle the manual self-check mode.
* @param : None
* @return: None
**/
static void systemSelfCheckMode(void)
{
}

/**
* @brief : Handle the update mode.
* @param : None
* @return: None
**/
static void systemUpdateMode(void)
{
}

/**
* @brief : Handle the diagnostic mode.
* @param : None
* @return: None
**/
static void systemDiagnosticMode(void)
{
}

/**
* @brief : Handle the end-of-line mode.
* @param : None
* @return: None
**/
static void systemEolMode(void)
{
}

/**
* @brief : Dispatch the system mode handler.
* @param : None
* @return: None
**/
void systemManagerRun(void)
{
    switch (systemGetMode()) {
        case eSYSTEM_INIT_MODE:
            systemInitMode();
            break;
        case eSYSTEM_POWERUP_SELFCHECK_MODE:
            systemPowerupSelfCheckMode();
            break;
        case eSYSTEM_STANDBY_MODE:
            systemStandbyMode();
            break;
        case eSYSTEM_NORMAL_MODE:
            systemNormalMode();
            break;
        case eSYSTEM_SELF_CHECK_MODE:
            systemSelfCheckMode();
            break;
        case eSYSTEM_UPDATE_MODE:
            systemUpdateMode();
            break;
        case eSYSTEM_DIAGNOSTIC_MODE:
            systemDiagnosticMode();
            break;
        case eSYSTEM_EOL_MODE:
            systemEolMode();
            break;
        default:
            break;
    }
}

/**************************End of file********************************/
