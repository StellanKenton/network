/***********************************************************************************
* @file     : usbcdc.c
* @brief    : USB CDC communication manager implementation.
* @details  : Polls the board USB CDC channel, parses line-based commands, and
*             replies through the same virtual COM port.
* @author   : GitHub Copilot
* @date     : 2026-04-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "usbcdc.h"

#include <stddef.h>
#include <string.h>

#include "../../../rep/service/log/log.h"
#include "../../../rep/driver/drvusb/drvusb.h"
#include "../../bsp/bspusb.h"
#include "../../port/drvusb_port.h"
#include "../../system/system.h"
#include "../../system/systask.h"

#define USBCDC_MANAGER_LOG_TAG             "UsbCdc"
/*
 * Recovery delay must be long enough for a normal host-driven re-enumeration
 * (unplug/replug or suspend/resume) to complete on its own. A typical Windows
 * CDC enumeration finishes in 200-500 ms after bus reset. If we tear the USB
 * peripheral down too early, we abort the in-progress enumeration and the
 * host falls into "Descriptor Request Failed" on subsequent plugs. Only fall
 * back to the hard Stop/Start/Connect restart when the device truly stalls.
 * Windows full-speed CDC enumeration typically takes 1-3 s; 5 s covers even
 * slow systems while still recovering from genuine stalls.
 */
/**********************************************************************************/

static stUsbCdcManagerState gUsbCdcManagerState;

static bool usbCdcManagerRecoverUsb(void);

void usbCdcManagerReset(void)
{
    (void)memset(&gUsbCdcManagerState, 0, sizeof(gUsbCdcManagerState));
}

bool usbCdcManagerInit(void)
{
    usbCdcManagerReset();
    gUsbCdcManagerState.isInitialized = true;
    LOG_I(USBCDC_MANAGER_LOG_TAG, "usb cdc manager ready");
    return true;
}

bool usbCdcManagerWrite(const uint8_t *buffer, uint16_t length)
{
    if ((buffer == NULL) || (length == 0U)) {
        return false;
    }

    if (!drvUsbIsConfigured(DRVUSB_DEV0)) {
        return false;
    }

    return bspUsbCdcWrite(buffer, length, USBCDC_MANAGER_USB_TIMEOUT_MS) == DRV_STATUS_OK;
}

bool usbCdcManagerRead(uint8_t *buffer, uint16_t bufferSize, uint16_t *actualLength)
{
    uint16_t lCopyLength;

    if ((buffer == NULL) || (actualLength == NULL) || (bufferSize == 0U)) {
        return false;
    }

    *actualLength = 0U;
    if (gUsbCdcManagerState.rxLength == 0U) {
        return true;
    }

    lCopyLength = gUsbCdcManagerState.rxLength;
    if (lCopyLength > bufferSize) {
        lCopyLength = bufferSize;
    }

    (void)memcpy(buffer, gUsbCdcManagerState.rxBuffer, lCopyLength);
    *actualLength = lCopyLength;
    gUsbCdcManagerState.rxLength = 0U;
    return true;
}

uint16_t usbCdcManagerGetRxLength(void)
{
    return gUsbCdcManagerState.rxLength;
}

static bool usbCdcManagerRecoverUsb(void)
{
    if (drvUsbStop(DRVUSB_DEV0) != DRV_STATUS_OK) {
        LOG_W(USBCDC_MANAGER_LOG_TAG, "usb recovery stop failed");
        return false;
    }

    if (drvUsbStart(DRVUSB_DEV0) != DRV_STATUS_OK) {
        LOG_W(USBCDC_MANAGER_LOG_TAG, "usb recovery start failed");
        return false;
    }

    if (drvUsbConnect(DRVUSB_DEV0) != DRV_STATUS_OK) {
        LOG_W(USBCDC_MANAGER_LOG_TAG, "usb recovery connect failed");
        return false;
    }

    LOG_W(USBCDC_MANAGER_LOG_TAG, "usb recovery restart triggered");
    return true;
}

void usbCdcManagerProcess(void)
{
    bool lIsConfigured;
    bool lIsSuspended;
    uint32_t lNowTick;
    uint16_t lActualLength = 0U;
    if (!gUsbCdcManagerState.isInitialized) {
        return;
    }

    lNowTick = systemGetTickMs();
    lIsConfigured = drvUsbIsConfigured(DRVUSB_DEV0);
    lIsSuspended = bspUsbIsSuspended(DRVUSB_DEV0);

    if (gUsbCdcManagerState.isUsbRecoveryPending) {
        if (lIsConfigured) {
            gUsbCdcManagerState.isUsbRecoveryPending = false;
        } else if ((lNowTick - gUsbCdcManagerState.usbRecoveryTick) >= USBCDC_MANAGER_RECOVERY_DELAY_MS) {
            gUsbCdcManagerState.isUsbRecoveryPending = false;
            if (usbCdcManagerRecoverUsb()) {
                gUsbCdcManagerState.wasUsbConfigured = false;
                gUsbCdcManagerState.wasUsbSuspended = false;
                gUsbCdcManagerState.rxLength = 0U;
            }
            return;
        }
    }

    if (lIsSuspended) {
        if (gUsbCdcManagerState.wasUsbConfigured && !gUsbCdcManagerState.wasUsbSuspended) {
            LOG_I(USBCDC_MANAGER_LOG_TAG, "usb host suspended");
        }
        gUsbCdcManagerState.wasUsbSuspended = true;
        return;
    }

    if (!lIsConfigured) {
        if (gUsbCdcManagerState.wasUsbConfigured || gUsbCdcManagerState.wasUsbSuspended) {
            LOG_I(USBCDC_MANAGER_LOG_TAG, "usb host disconnected");
        }
        if ((gUsbCdcManagerState.wasUsbConfigured || gUsbCdcManagerState.wasUsbSuspended) &&
            !gUsbCdcManagerState.isUsbRecoveryPending) {
            gUsbCdcManagerState.isUsbRecoveryPending = true;
            gUsbCdcManagerState.usbRecoveryTick = lNowTick;
        }
        gUsbCdcManagerState.wasUsbConfigured = false;
        gUsbCdcManagerState.wasUsbSuspended = false;
        gUsbCdcManagerState.rxLength = 0U;
        return;
    }

    if (gUsbCdcManagerState.wasUsbSuspended) {
        gUsbCdcManagerState.wasUsbSuspended = false;
        LOG_I(USBCDC_MANAGER_LOG_TAG, "usb host resumed");
    }

    if (!gUsbCdcManagerState.wasUsbConfigured) {
        gUsbCdcManagerState.wasUsbConfigured = true;
        LOG_I(USBCDC_MANAGER_LOG_TAG, "usb host configured");
    }

    if (bspUsbCdcGetRxLength() == 0U) {
        return;
    }

    if (bspUsbCdcRead(gUsbCdcManagerState.rxBuffer,
                      sizeof(gUsbCdcManagerState.rxBuffer),
                      &lActualLength,
                      1U) != DRV_STATUS_OK) {
        return;
    }

    gUsbCdcManagerState.rxLength = lActualLength;
}
/**************************End of file********************************/
