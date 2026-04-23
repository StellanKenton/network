/***********************************************************************************
* @file     : bspusb.c
* @brief    : Board-level USB BSP implementation.
* @details  : Maps the reusable drvusb interface onto the STM32 USB device stack.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bspusb.h"

#include <stdbool.h>
#include <stddef.h>

#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "usbd_core.h"
#include "ringbuffer.h"
#include "rtos.h"

#include "../port/drvusb_port.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

static stRingBuffer gBspUsbRxRingBuffer[DRVUSB_MAX];
static uint8_t gBspUsbRxStorageFs[DRVUSB_PORT_RX_STORAGE_FS];
static bool gBspUsbInitialized[DRVUSB_MAX];
static bool gBspUsbStarted[DRVUSB_MAX];

static eDrvStatus bspUsbStatusFromHal(HAL_StatusTypeDef halStatus)
{
    switch (halStatus) {
        case HAL_OK:
            return DRV_STATUS_OK;
        case HAL_BUSY:
            return DRV_STATUS_BUSY;
        case HAL_TIMEOUT:
            return DRV_STATUS_TIMEOUT;
        default:
            return DRV_STATUS_ERROR;
    }
}

static eDrvStatus bspUsbStatusFromUsbd(USBD_StatusTypeDef usbStatus)
{
    switch (usbStatus) {
        case USBD_OK:
            return DRV_STATUS_OK;
        case USBD_BUSY:
            return DRV_STATUS_BUSY;
        case USBD_FAIL:
        default:
            return DRV_STATUS_ERROR;
    }
}

static USBD_HandleTypeDef *bspUsbGetDevice(uint8_t usb)
{
    if (usb != DRVUSB_DEV0) {
        return NULL;
    }

    return &hUsbDeviceFS;
}

static PCD_HandleTypeDef *bspUsbGetPcd(uint8_t usb)
{
    USBD_HandleTypeDef *device = bspUsbGetDevice(usb);

    if ((device == NULL) || (device->pData == NULL)) {
        return NULL;
    }

    return (PCD_HandleTypeDef *)device->pData;
}

static bool bspUsbIsCdcEndpoint(uint8_t endpointAddress)
{
    return (endpointAddress == DRVUSB_PORT_CDC_DATA_IN_EP) ||
           (endpointAddress == DRVUSB_PORT_CDC_DATA_OUT_EP) ||
           (endpointAddress == DRVUSB_PORT_CDC_CMD_EP);
}

static uint8_t bspUsbGetEndpointType(eDrvUsbEndpointType type)
{
    switch (type) {
        case DRVUSB_ENDPOINT_TYPE_CONTROL:
            return USBD_EP_TYPE_CTRL;
        case DRVUSB_ENDPOINT_TYPE_ISOCHRONOUS:
            return USBD_EP_TYPE_ISOC;
        case DRVUSB_ENDPOINT_TYPE_BULK:
            return USBD_EP_TYPE_BULK;
        case DRVUSB_ENDPOINT_TYPE_INTERRUPT:
        default:
            return USBD_EP_TYPE_INTR;
    }
}

eDrvStatus bspUsbInit(uint8_t usb)
{
    USBD_HandleTypeDef *device = bspUsbGetDevice(usb);

    if (device == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (gBspUsbInitialized[usb]) {
        return DRV_STATUS_OK;
    }

    if (ringBufferInit(&gBspUsbRxRingBuffer[usb], gBspUsbRxStorageFs, sizeof(gBspUsbRxStorageFs)) != RINGBUFFER_OK) {
        return DRV_STATUS_ERROR;
    }

    MX_USB_DEVICE_Init();
    if (device->pData == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    gBspUsbInitialized[usb] = true;
    gBspUsbStarted[usb] = true;
    return DRV_STATUS_OK;
}

eDrvStatus bspUsbStart(uint8_t usb)
{
    USBD_HandleTypeDef *device = bspUsbGetDevice(usb);
    eDrvStatus status;

    if (device == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    status = bspUsbInit(usb);
    if (status != DRV_STATUS_OK) {
        return status;
    }

    if (gBspUsbStarted[usb]) {
        return DRV_STATUS_OK;
    }

    status = bspUsbStatusFromUsbd(USBD_Start(device));
    if (status == DRV_STATUS_OK) {
        gBspUsbStarted[usb] = true;
    }
    return status;
}

eDrvStatus bspUsbStop(uint8_t usb)
{
    USBD_HandleTypeDef *device = bspUsbGetDevice(usb);
    eDrvStatus status;

    if ((device == NULL) || !gBspUsbInitialized[usb]) {
        return DRV_STATUS_NOT_READY;
    }

    status = bspUsbStatusFromUsbd(USBD_Stop(device));
    if (status == DRV_STATUS_OK) {
        gBspUsbStarted[usb] = false;
    }
    return status;
}

eDrvStatus bspUsbSetConnect(uint8_t usb, bool isConnect)
{
    PCD_HandleTypeDef *pcd = bspUsbGetPcd(usb);

    if ((usb >= DRVUSB_MAX) || (pcd == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    if (isConnect) {
        return bspUsbStatusFromHal(HAL_PCD_DevConnect(pcd));
    }

    return bspUsbStatusFromHal(HAL_PCD_DevDisconnect(pcd));
}

eDrvStatus bspUsbOpenEndpoint(uint8_t usb, const stDrvUsbEndpointConfig *config)
{
    PCD_HandleTypeDef *pcd = bspUsbGetPcd(usb);

    if ((pcd == NULL) || (config == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (bspUsbIsCdcEndpoint(config->endpointAddress)) {
        return DRV_STATUS_OK;
    }

    return bspUsbStatusFromHal(HAL_PCD_EP_Open(pcd,
                                               config->endpointAddress,
                                               config->maxPacketSize,
                                               bspUsbGetEndpointType(config->type)));
}

eDrvStatus bspUsbCloseEndpoint(uint8_t usb, uint8_t endpointAddress)
{
    PCD_HandleTypeDef *pcd = bspUsbGetPcd(usb);

    if (pcd == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    if (bspUsbIsCdcEndpoint(endpointAddress)) {
        return DRV_STATUS_OK;
    }

    return bspUsbStatusFromHal(HAL_PCD_EP_Close(pcd, endpointAddress));
}

eDrvStatus bspUsbFlushEndpoint(uint8_t usb, uint8_t endpointAddress)
{
    PCD_HandleTypeDef *pcd = bspUsbGetPcd(usb);

    if (pcd == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    return bspUsbStatusFromHal(HAL_PCD_EP_Flush(pcd, endpointAddress));
}

eDrvStatus bspUsbTransmit(uint8_t usb, uint8_t endpointAddress, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    uint32_t startTick;
    uint8_t usbStatus;

    if ((usb != DRVUSB_DEV0) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (endpointAddress != DRVUSB_PORT_CDC_DATA_IN_EP) {
        return DRV_STATUS_UNSUPPORTED;
    }

    if (!gBspUsbStarted[usb] || (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)) {
        return DRV_STATUS_NOT_READY;
    }

    startTick = HAL_GetTick();
    do {
        usbStatus = CDC_Transmit_FS((uint8_t *)buffer, length);
        if (usbStatus == USBD_OK) {
            return DRV_STATUS_OK;
        }

        if (usbStatus != USBD_BUSY) {
            return bspUsbStatusFromUsbd((USBD_StatusTypeDef)usbStatus);
        }

        (void)repRtosDelayMs(1U);
    } while ((HAL_GetTick() - startTick) < timeoutMs);

    return DRV_STATUS_TIMEOUT;
}

eDrvStatus bspUsbReceive(uint8_t usb, uint8_t endpointAddress, uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs)
{
    uint32_t startTick;
    uint32_t availableLength;
    uint32_t readLength;

    if (actualLength != NULL) {
        *actualLength = 0U;
    }

    if ((usb != DRVUSB_DEV0) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (endpointAddress != DRVUSB_PORT_CDC_DATA_OUT_EP) {
        return DRV_STATUS_UNSUPPORTED;
    }

    if (!gBspUsbInitialized[usb]) {
        return DRV_STATUS_NOT_READY;
    }

    startTick = HAL_GetTick();
    do {
        availableLength = ringBufferGetUsed(&gBspUsbRxRingBuffer[usb]);
        if (availableLength > 0U) {
            readLength = availableLength;
            if (readLength > length) {
                readLength = length;
            }

            readLength = ringBufferRead(&gBspUsbRxRingBuffer[usb], buffer, readLength);
            if (actualLength != NULL) {
                *actualLength = (uint16_t)readLength;
            }
            return DRV_STATUS_OK;
        }

        if (timeoutMs == 0U) {
            break;
        }

        (void)repRtosDelayMs(1U);
    } while ((HAL_GetTick() - startTick) < timeoutMs);

    return DRV_STATUS_TIMEOUT;
}

bool bspUsbIsConnected(uint8_t usb)
{
    USBD_HandleTypeDef *device = bspUsbGetDevice(usb);

    if ((device == NULL) || !gBspUsbStarted[usb]) {
        return false;
    }

    return (device->dev_state == USBD_STATE_ADDRESSED) ||
           (device->dev_state == USBD_STATE_CONFIGURED) ||
           (device->dev_state == USBD_STATE_SUSPENDED);
}

bool bspUsbIsConfigured(uint8_t usb)
{
    USBD_HandleTypeDef *device = bspUsbGetDevice(usb);

    return (device != NULL) && (device->dev_state == USBD_STATE_CONFIGURED);
}

eDrvUsbSpeed bspUsbGetSpeed(uint8_t usb)
{
    PCD_HandleTypeDef *pcd = bspUsbGetPcd(usb);

    if (pcd == NULL) {
        return DRVUSB_SPEED_UNKNOWN;
    }

    switch (pcd->Init.speed) {
        case PCD_SPEED_FULL:
            return DRVUSB_SPEED_FULL;
        default:
            return DRVUSB_SPEED_UNKNOWN;
    }
}

void usbdCdcIfUserReceiveCallback(const uint8_t *buffer, uint32_t length)
{
    if ((buffer == NULL) || (length == 0U)) {
        return;
    }

    (void)ringBufferWrite(&gBspUsbRxRingBuffer[DRVUSB_DEV0], buffer, length);
}
/**************************End of file********************************/
