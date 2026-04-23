/***********************************************************************************
* @file     : bspusb.c
* @brief    : Board-specific USB CDC device implementation.
* @details  : Provides CDC data transport on STM32F103 USB FS for drvusb.
* @author   : GitHub Copilot
* @date     : 2026-04-17
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bspusb.h"

#include "hw_config.h"
#include "usb_conf.h"
#include "usb_lib.h"
#include "usb_pwr.h"
#include "../../rep/service/log/log.h"
#include "../../rep/tools/ringbuffer/ringbuffer.h"
#include "../system/system.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

#define BSP_USB_LOG_TAG    "BspUsb"

typedef struct stBspUsbContext {
    bool isInitialized;
    bool isStarted;
    bool isConfigured;
    bool isCableConnected;
    volatile bool isDataTxBusy;
    volatile bool isCmdTxBusy;
    stRingBuffer rxRing;
    uint8_t rxStorage[BSPUSB_CDC_RX_BUFFER_SIZE];
    uint8_t rxPacketBuffer[BSPUSB_CDC_DATA_MAX_PACKET_SIZE];
} stBspUsbContext;

typedef struct stBspUsbEndpointInfo {
    uint8_t endpointIndex;
    uint16_t packetSize;
    volatile bool *txBusyFlag;
    bool isTxEndpoint;
} stBspUsbEndpointInfo;

static stBspUsbContext gBspUsbContext;

static bool bspUsbIsValidId(uint8_t usb)
{
    return usb == BSPUSB_DEVICE_ID;
}

static void bspUsbBoardPreparePins(void)
{
    GPIO_InitTypeDef lGpioInit;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_StructInit(&lGpioInit);
    lGpioInit.GPIO_Speed = GPIO_Speed_50MHz;

    lGpioInit.GPIO_Pin = GPIO_Pin_8;
    lGpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &lGpioInit);
    GPIO_ResetBits(GPIOA, GPIO_Pin_8);

    lGpioInit.GPIO_Pin = GPIO_Pin_11;
    lGpioInit.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &lGpioInit);

    lGpioInit.GPIO_Pin = GPIO_Pin_12;
    lGpioInit.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &lGpioInit);
}

void bspUsbForceDisconnectEarly(void)
{
    bspUsbBoardPreparePins();
}

static bool bspUsbIsTimeout(uint32_t startTick, uint32_t timeoutMs)
{
    return (timeoutMs > 0U) && ((systemGetTickMs() - startTick) >= timeoutMs);
}

static eDrvStatus bspUsbGetEndpointInfo(uint8_t endpointAddress, stBspUsbEndpointInfo *info)
{
    if (info == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    switch (endpointAddress) {
        case BSPUSB_CDC_DATA_IN_EP:
            info->endpointIndex = ENDP1;
            info->packetSize = BSPUSB_CDC_DATA_MAX_PACKET_SIZE;
            info->txBusyFlag = &gBspUsbContext.isDataTxBusy;
            info->isTxEndpoint = true;
            return DRV_STATUS_OK;
        case BSPUSB_CDC_CMD_EP:
            info->endpointIndex = ENDP2;
            info->packetSize = BSPUSB_CDC_CMD_PACKET_SIZE;
            info->txBusyFlag = &gBspUsbContext.isCmdTxBusy;
            info->isTxEndpoint = true;
            return DRV_STATUS_OK;
        case BSPUSB_CDC_DATA_OUT_EP:
            info->endpointIndex = ENDP3;
            info->packetSize = BSPUSB_CDC_DATA_MAX_PACKET_SIZE;
            info->txBusyFlag = NULL;
            info->isTxEndpoint = false;
            return DRV_STATUS_OK;
        default:
            return DRV_STATUS_UNSUPPORTED;
    }
}

static eDrvStatus bspUsbSetEndpointState(uint8_t endpointAddress, bool isReady)
{
    stBspUsbEndpointInfo lEndpointInfo;
    eDrvStatus lStatus;

    lStatus = bspUsbGetEndpointInfo(endpointAddress, &lEndpointInfo);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    if (lEndpointInfo.isTxEndpoint) {
        SetEPTxStatus(lEndpointInfo.endpointIndex, isReady ? EP_TX_NAK : EP_TX_DIS);
    }
    else {
        SetEPRxStatus(lEndpointInfo.endpointIndex, isReady ? EP_RX_VALID : EP_RX_DIS);
    }

    return DRV_STATUS_OK;
}

static eDrvStatus bspUsbTransmitInternal(uint8_t endpointAddress, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    uint16_t lOffset = 0U;
    uint32_t lStartTick;
    stBspUsbEndpointInfo lEndpointInfo;
    eDrvStatus lStatus;

    if ((buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gBspUsbContext.isConfigured) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = bspUsbGetEndpointInfo(endpointAddress, &lEndpointInfo);
    if ((lStatus != DRV_STATUS_OK) || !lEndpointInfo.isTxEndpoint || (lEndpointInfo.txBusyFlag == NULL)) {
        return (lStatus == DRV_STATUS_OK) ? DRV_STATUS_UNSUPPORTED : lStatus;
    }

    lStartTick = systemGetTickMs();
    while (lOffset < length) {
        uint16_t lChunkLength = (uint16_t)(length - lOffset);

        if (lChunkLength > lEndpointInfo.packetSize) {
            lChunkLength = lEndpointInfo.packetSize;
        }

        while (*(lEndpointInfo.txBusyFlag)) {
            if (bspUsbIsTimeout(lStartTick, timeoutMs)) {
                return DRV_STATUS_TIMEOUT;
            }
        }

        *(lEndpointInfo.txBusyFlag) = true;
        (void)USB_SIL_Write(endpointAddress, (uint8_t *)&buffer[lOffset], lChunkLength);
        SetEPTxCount(lEndpointInfo.endpointIndex, lChunkLength);
        SetEPTxStatus(lEndpointInfo.endpointIndex, EP_TX_VALID);
        lOffset = (uint16_t)(lOffset + lChunkLength);

        while (*(lEndpointInfo.txBusyFlag)) {
            if (bspUsbIsTimeout(lStartTick, timeoutMs)) {
                (void)bspUsbSetEndpointState(endpointAddress, true);
                *(lEndpointInfo.txBusyFlag) = false;
                return DRV_STATUS_TIMEOUT;
            }
        }
    }

    return DRV_STATUS_OK;
}

void bspUsbNotifyConfigured(bool isConfigured)
{
    if (isConfigured && !gBspUsbContext.isConfigured) {
        LOG_I(BSP_USB_LOG_TAG, "usb connected");
    }
    else if (!isConfigured && gBspUsbContext.isConfigured) {
        LOG_I(BSP_USB_LOG_TAG, "usb disconnected");
    }

    gBspUsbContext.isConfigured = isConfigured;
}

void bspUsbNotifyCableState(bool isConnected)
{
    gBspUsbContext.isCableConnected = isConnected;
}

void bspUsbResetTransferState(void)
{
    gBspUsbContext.isConfigured = false;
    gBspUsbContext.isDataTxBusy = false;
    gBspUsbContext.isCmdTxBusy = false;
    (void)ringBufferReset(&gBspUsbContext.rxRing);
}

void bspUsbHandleDataOut(void)
{
    uint32_t lReadLength;

    lReadLength = USB_SIL_Read(BSPUSB_CDC_DATA_OUT_EP, gBspUsbContext.rxPacketBuffer);
    if (lReadLength > 0U) {
        (void)ringBufferWriteOverwrite(&gBspUsbContext.rxRing,
                                       gBspUsbContext.rxPacketBuffer,
                                       lReadLength);
    }
}

void bspUsbHandleDataIn(uint8_t endpointAddress)
{
    stBspUsbEndpointInfo lEndpointInfo;

    if (bspUsbGetEndpointInfo(endpointAddress, &lEndpointInfo) == DRV_STATUS_OK) {
        if (lEndpointInfo.txBusyFlag != NULL) {
            *(lEndpointInfo.txBusyFlag) = false;
        }
    }
}

eDrvStatus bspUsbInit(uint8_t usb)
{
    if (!bspUsbIsValidId(usb)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (gBspUsbContext.isInitialized) {
        return DRV_STATUS_OK;
    }

    bspUsbBoardPreparePins();
    systemDelayMs(20U);
    if (ringBufferInit(&gBspUsbContext.rxRing,
                       gBspUsbContext.rxStorage,
                       sizeof(gBspUsbContext.rxStorage)) != RINGBUFFER_OK) {
        return DRV_STATUS_ERROR;
    }

    bspUsbResetTransferState();
    gBspUsbContext.isCableConnected = false;
    gBspUsbContext.isStarted = false;
    gBspUsbContext.isInitialized = true;
    return DRV_STATUS_OK;
}

eDrvStatus bspUsbStart(uint8_t usb)
{
    if (!bspUsbIsValidId(usb)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gBspUsbContext.isInitialized) {
        return DRV_STATUS_NOT_READY;
    }

    if (gBspUsbContext.isStarted) {
        return DRV_STATUS_OK;
    }

    bspUsbResetTransferState();
    Set_USBClock();
    USB_Interrupts_Config();
    USB_Init();
    gBspUsbContext.isStarted = true;
    return DRV_STATUS_OK;
}

eDrvStatus bspUsbStop(uint8_t usb)
{
    if (!bspUsbIsValidId(usb)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gBspUsbContext.isStarted) {
        return DRV_STATUS_OK;
    }

    NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
    NVIC_DisableIRQ(USBWakeUp_IRQn);
    (void)PowerOff();
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_USB, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_USB, DISABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, DISABLE);
    bspUsbBoardPreparePins();
    bspUsbResetTransferState();
    gBspUsbContext.isStarted = false;
    gBspUsbContext.isCableConnected = false;
    return DRV_STATUS_OK;
}

eDrvStatus bspUsbSetConnect(uint8_t usb, bool isConnect)
{
    if (!bspUsbIsValidId(usb)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (isConnect) {
        USB_Cable_Config(DISABLE);
        systemDelayMs(100U);
        USB_Cable_Config(ENABLE);
    }
    else {
        USB_Cable_Config(DISABLE);
    }

    return DRV_STATUS_OK;
}

eDrvStatus bspUsbOpenEndpoint(uint8_t usb, const stDrvUsbEndpointConfig *config)
{
    if (!bspUsbIsValidId(usb) || (config == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return bspUsbSetEndpointState(config->endpointAddress, true);
}

eDrvStatus bspUsbCloseEndpoint(uint8_t usb, uint8_t endpointAddress)
{
    if (!bspUsbIsValidId(usb)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return bspUsbSetEndpointState(endpointAddress, false);
}

eDrvStatus bspUsbFlushEndpoint(uint8_t usb, uint8_t endpointAddress)
{
    if (!bspUsbIsValidId(usb)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (endpointAddress == BSPUSB_CDC_DATA_OUT_EP) {
        (void)ringBufferReset(&gBspUsbContext.rxRing);
        return DRV_STATUS_OK;
    }

    return bspUsbSetEndpointState(endpointAddress, true);
}

eDrvStatus bspUsbTransmit(uint8_t usb, uint8_t endpointAddress, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    if (!bspUsbIsValidId(usb)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return bspUsbTransmitInternal(endpointAddress, buffer, length, timeoutMs);
}

eDrvStatus bspUsbReceive(uint8_t usb, uint8_t endpointAddress, uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs)
{
    uint32_t lStartTick;
    uint32_t lUsedLength;
    uint32_t lReadLength;

    if (!bspUsbIsValidId(usb) || (endpointAddress != BSPUSB_CDC_DATA_OUT_EP) ||
        (buffer == NULL) || (actualLength == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gBspUsbContext.isConfigured) {
        return DRV_STATUS_NOT_READY;
    }

    lStartTick = systemGetTickMs();
    lUsedLength = ringBufferGetUsed(&gBspUsbContext.rxRing);
    while (lUsedLength == 0U) {
        if (bspUsbIsTimeout(lStartTick, timeoutMs)) {
            *actualLength = 0U;
            return DRV_STATUS_TIMEOUT;
        }
        lUsedLength = ringBufferGetUsed(&gBspUsbContext.rxRing);
    }

    lReadLength = (lUsedLength > length) ? length : lUsedLength;
    *actualLength = (uint16_t)ringBufferRead(&gBspUsbContext.rxRing, buffer, lReadLength);
    return (*actualLength > 0U) ? DRV_STATUS_OK : DRV_STATUS_TIMEOUT;
}

bool bspUsbIsConnected(uint8_t usb)
{
    return bspUsbIsValidId(usb) && gBspUsbContext.isCableConnected;
}

bool bspUsbIsConfigured(uint8_t usb)
{
    return bspUsbIsValidId(usb) && gBspUsbContext.isConfigured && (bDeviceState == CONFIGURED);
}

bool bspUsbIsSuspended(uint8_t usb)
{
    return bspUsbIsValidId(usb) && gBspUsbContext.isConfigured && (bDeviceState == SUSPENDED);
}

eDrvUsbSpeed bspUsbGetSpeed(uint8_t usb)
{
    if (!bspUsbIsValidId(usb)) {
        return DRVUSB_SPEED_UNKNOWN;
    }

    return DRVUSB_SPEED_FULL;
}

uint32_t bspUsbCdcGetRxLength(void)
{
    return ringBufferGetUsed(&gBspUsbContext.rxRing);
}

eDrvStatus bspUsbCdcWrite(const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    return bspUsbTransmit(BSPUSB_DEVICE_ID, BSPUSB_CDC_DATA_IN_EP, buffer, length, timeoutMs);
}

eDrvStatus bspUsbCdcRead(uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs)
{
    return bspUsbReceive(BSPUSB_DEVICE_ID,
                         BSPUSB_CDC_DATA_OUT_EP,
                         buffer,
                         length,
                         actualLength,
                         timeoutMs);
}

/**************************End of file********************************/
