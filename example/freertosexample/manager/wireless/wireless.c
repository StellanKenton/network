/***********************************************************************************
* @file     : wireless.c
* @brief    : Project-side FC41D wireless manager.
* @details  : Provides BLE info query and non-blocking periodic service on top of
*             the FC41D project binding.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "wireless.h"

#include <string.h>

#include "drvuart.h"

#include "../../../rep/service/log/log.h"
#include "../../../rep/service/rtos/rtos.h"
#include "../../port/drvuart_port.h"

static eWirelessState gWirelessState = eWIRELESS_STATE_INIT;
static bool gWirelessInitDone = false;
static bool gWirelessReadyLogged = false;
static bool gWirelessMacLogged = false;
static bool gWirelessBleConnected = false;

static const char gWirelessBleName[] = "rumi";
static const char gWirelessBleServiceUuid[] = "FE60";
static const char gWirelessBleRxCharUuid[] = "FE61";
static const char gWirelessBleTxCharUuid[] = "FE62";

static void wirelessUpdateState(void);

static void wirelessUpdateState(void)
{
    const stFc41dState *lpState;
    char lMacAddress[FC41D_MAC_ADDRESS_TEXT_MAX_LENGTH + 1U];

    lpState = fc41dGetState(WIRELESS_FC41D_DEVICE);
    if (lpState == NULL) {
        if (gWirelessInitDone) {
            gWirelessState = eWIRELESS_STATE_ERROR;
        }
        return;
    }

    if (lpState->runState == FC41D_RUN_ERROR) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return;
    }

    gWirelessState = lpState->isReady ? eWIRELESS_STATE_NORMAL : eWIRELESS_STATE_INIT;

    if (lpState->isReady && !gWirelessReadyLogged) {
        LOG_I(WIRELESS_LOG_TAG, "fc41d ready");
        gWirelessReadyLogged = true;
    }

    if (lpState->hasMacAddress && !gWirelessMacLogged &&
        fc41dGetCachedMac(WIRELESS_FC41D_DEVICE, lMacAddress, (uint16_t)sizeof(lMacAddress))) {
        LOG_I(WIRELESS_LOG_TAG, "ble mac: %s", lMacAddress);
        gWirelessMacLogged = true;
    }

    if (lpState->isBleConnected != gWirelessBleConnected) {
        gWirelessBleConnected = lpState->isBleConnected;
        LOG_I(WIRELESS_LOG_TAG, "ble %s", gWirelessBleConnected ? "connected" : "disconnected");
    }
}

bool wirelessInit(void)
{
    stFc41dCfg lCfg;
    stFc41dBleCfg lBleCfg;

    if (gWirelessInitDone) {
        return gWirelessState != eWIRELESS_STATE_ERROR;
    }

    if (drvUartInit(DRVUART_WIFI) != DRV_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        LOG_E(WIRELESS_LOG_TAG, "wifi uart init fail");
        return false;
    }

    if (fc41dGetDefCfg(WIRELESS_FC41D_DEVICE, &lCfg) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    if (fc41dSetCfg(WIRELESS_FC41D_DEVICE, &lCfg) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    if (fc41dGetDefBleCfg(WIRELESS_FC41D_DEVICE, &lBleCfg) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    (void)memcpy(lBleCfg.name, gWirelessBleName, sizeof(gWirelessBleName));
    (void)memcpy(lBleCfg.serviceUuid, gWirelessBleServiceUuid, sizeof(gWirelessBleServiceUuid));
    (void)memcpy(lBleCfg.rxCharUuid, gWirelessBleRxCharUuid, sizeof(gWirelessBleRxCharUuid));
    (void)memcpy(lBleCfg.txCharUuid, gWirelessBleTxCharUuid, sizeof(gWirelessBleTxCharUuid));

    if (fc41dSetBleCfg(WIRELESS_FC41D_DEVICE, &lBleCfg) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    if (fc41dInit(WIRELESS_FC41D_DEVICE) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    if (fc41dStart(WIRELESS_FC41D_DEVICE, FC41D_ROLE_BLE_PERIPHERAL) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    gWirelessReadyLogged = false;
    gWirelessMacLogged = false;
    gWirelessBleConnected = false;
    gWirelessInitDone = true;
    gWirelessState = eWIRELESS_STATE_INIT;
    wirelessUpdateState();
    LOG_I(WIRELESS_LOG_TAG, "fc41d init ok ble=%s", gWirelessBleName);
    return true;
}

void wirelessProcess(void)
{
    eFc41dStatus lStatus;

    if (!gWirelessInitDone) {
        return;
    }

    lStatus = fc41dProcess(WIRELESS_FC41D_DEVICE, repRtosGetTickMs());
    if (lStatus != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        LOG_W(WIRELESS_LOG_TAG, "fc41d process fail status=%d", (int)lStatus);
        return;
    }

    wirelessUpdateState();
}

const eWirelessState *wirelessGetStatus(void)
{
    return &gWirelessState;
}

/**************************End of file********************************/
