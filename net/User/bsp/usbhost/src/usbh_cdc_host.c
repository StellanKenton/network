/************************************************************************************
* @file     : usbh_cdc_host.c
* @brief    : USB Host CDC/ACM class implementation for EC800M AT communication.
***********************************************************************************/
#include "usbh_cdc_host.h"

#include <string.h>

#include "usb_bsp.h"
#include "usb_hcd.h"
#include "usbh_hcs.h"
#include "usbh_ioreq.h"
#include "usbh_stdreq.h"

#include "../../../rep/service/log/log.h"
#include "../../../rep/service/rtos/rtos.h"

#define USBH_CDC_LOG_TAG                         "usbCdc"
#define USBH_CDC_INVALID_CHANNEL                 0xFFU
#define USBH_CDC_REQ_SET_LINE_CODING_CODE        0x20U
#define USBH_CDC_REQ_SET_CONTROL_LINE_STATE_CODE 0x22U
#define USBH_CDC_CONTROL_LINE_DTR_RTS            0x0003U
#define USBH_CDC_DEFAULT_ALT_SETTING             0U
#define USBH_CDC_DEFAULT_TIMEOUT_MS              100U
#define USBH_CDC_BULK_ATTR_MASK                  0x03U

static USBH_Status usbhCdcInterfaceInit(USB_OTG_CORE_HANDLE *pdev, void *phost);
static void usbhCdcInterfaceDeInit(USB_OTG_CORE_HANDLE *pdev, void *phost);
static USBH_Status usbhCdcClassRequest(USB_OTG_CORE_HANDLE *pdev, void *phost);
static USBH_Status usbhCdcHandle(USB_OTG_CORE_HANDLE *pdev, void *phost);
static bool usbhCdcFindBulkEndpoints(USBH_HOST *host);
static bool usbhCdcCaptureCandidate(USBH_HOST *host, uint8_t interfaceIndex, stUsbhCdcHostContext *candidate);
static bool usbhCdcOpenBulkChannels(USB_OTG_CORE_HANDLE *pdev, uint8_t devAddr, uint8_t speed);
static void usbhCdcCloseBulkChannels(USB_OTG_CORE_HANDLE *pdev);
static bool usbhCdcSwitchToNextFixedEndpoint(USB_OTG_CORE_HANDLE *pdev);
static USBH_Status usbhCdcSetLineCoding(USB_OTG_CORE_HANDLE *pdev, USBH_HOST *host);
static USBH_Status usbhCdcSetControlLineState(USB_OTG_CORE_HANDLE *pdev, USBH_HOST *host);
static USBH_Status usbhCdcSetInterface(USB_OTG_CORE_HANDLE *pdev, USBH_HOST *host);
static bool usbhCdcFinishClassRequest(USB_OTG_CORE_HANDLE *pdev, USBH_HOST *host);
static void usbhCdcLogTxState(USB_OTG_CORE_HANDLE *pdev, URB_STATE urbState, const char *reason);
static void usbhCdcProbeInEndpoint(USB_OTG_CORE_HANDLE *pdev);
static uint32_t usbhCdcGetTimeout(uint32_t timeoutMs);
static bool usbhCdcIsTimeout(uint32_t startMs, uint32_t timeoutMs);
static void usbhCdcWaitOneMs(void);

static stUsbhCdcHostContext gUsbhCdcContext;
static uint8_t gUsbhCdcLineCodingBuffer[7];
static uint8_t gUsbhCdcTxBuffer[USBH_CDC_QUECTEL_EC800M_AT_BULK_MPS];
static uint32_t gUsbhCdcReadyTickMs;
static uint8_t gUsbhCdcFixedEndpointIndex;
static const uint8_t gUsbhCdcFixedBulkInEp[USBH_CDC_QUECTEL_EC800M_EP_PAIR_COUNT] = {
    USBH_CDC_QUECTEL_EC800M_AT_BULK_IN,
    USBH_CDC_QUECTEL_EC800M_ALT1_BULK_IN,
    USBH_CDC_QUECTEL_EC800M_ALT0_BULK_IN,
    USBH_CDC_QUECTEL_EC800M_ALT2_BULK_IN,
    USBH_CDC_QUECTEL_EC800M_ALT3_BULK_IN,
};
static const uint8_t gUsbhCdcFixedBulkOutEp[USBH_CDC_QUECTEL_EC800M_EP_PAIR_COUNT] = {
    USBH_CDC_QUECTEL_EC800M_AT_BULK_OUT,
    USBH_CDC_QUECTEL_EC800M_ALT1_BULK_OUT,
    USBH_CDC_QUECTEL_EC800M_ALT0_BULK_OUT,
    USBH_CDC_QUECTEL_EC800M_ALT2_BULK_OUT,
    USBH_CDC_QUECTEL_EC800M_ALT3_BULK_OUT,
};

USBH_Class_cb_TypeDef USBH_CDC_cb = {
    usbhCdcInterfaceInit,
    usbhCdcInterfaceDeInit,
    usbhCdcClassRequest,
    usbhCdcHandle,
};

void usbhCdcHostReset(void)
{
    memset(&gUsbhCdcContext, 0, sizeof(gUsbhCdcContext));
    gUsbhCdcReadyTickMs = 0U;
    gUsbhCdcContext.bulkInChannel = USBH_CDC_INVALID_CHANNEL;
    gUsbhCdcContext.bulkOutChannel = USBH_CDC_INVALID_CHANNEL;
    gUsbhCdcContext.lineCoding.bitRate = 115200UL;
    gUsbhCdcContext.lineCoding.stopBits = 0U;
    gUsbhCdcContext.lineCoding.parity = 0U;
    gUsbhCdcContext.lineCoding.dataBits = 8U;
    gUsbhCdcFixedEndpointIndex = 0U;
}

bool usbhCdcHostIsReady(void)
{
    uint32_t nowTickMs;

    if (gUsbhCdcContext.skipClassRequest && gUsbhCdcContext.isReady) {
        nowTickMs = repRtosGetTickMs();
        if ((gUsbhCdcReadyTickMs == 0U) || ((uint32_t)(nowTickMs - gUsbhCdcReadyTickMs) < USBH_CDC_QUECTEL_EC800M_READY_DELAY_MS)) {
            return false;
        }
    }

    return gUsbhCdcContext.isReady;
}

eUsbhCdcHostStatus usbhCdcHostTransmit(USB_OTG_CORE_HANDLE *pdev, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    uint32_t startMs;
    URB_STATE urbState;

    if ((pdev == NULL) || (buffer == NULL) || (length == 0U)) {
        return USBH_CDC_HOST_INVALID_PARAM;
    }

    if (length > sizeof(gUsbhCdcTxBuffer)) {
        return USBH_CDC_HOST_INVALID_PARAM;
    }

    if (!gUsbhCdcContext.isReady || (gUsbhCdcContext.bulkOutChannel == USBH_CDC_INVALID_CHANNEL)) {
        return USBH_CDC_HOST_NOT_READY;
    }

    timeoutMs = usbhCdcGetTimeout(timeoutMs);
    usbhCdcProbeInEndpoint(pdev);
    memcpy(gUsbhCdcTxBuffer, buffer, length);
    startMs = repRtosGetTickMs();
    (void)USBH_BulkSendData(pdev, gUsbhCdcTxBuffer, length, gUsbhCdcContext.bulkOutChannel);

    for (;;) {
        urbState = HCD_GetURB_State(pdev, gUsbhCdcContext.bulkOutChannel);
        if (urbState == URB_DONE) {
            return USBH_CDC_HOST_OK;
        }
        if (urbState == URB_NOTREADY) {
            (void)USBH_BulkSendData(pdev, gUsbhCdcTxBuffer, length, gUsbhCdcContext.bulkOutChannel);
        } else if ((urbState == URB_ERROR) || (urbState == URB_STALL)) {
            usbhCdcLogTxState(pdev, urbState, "error");
            (void)usbhCdcSwitchToNextFixedEndpoint(pdev);
            return USBH_CDC_HOST_ERROR;
        }
        if (usbhCdcIsTimeout(startMs, timeoutMs)) {
            usbhCdcLogTxState(pdev, urbState, "timeout");
            (void)usbhCdcSwitchToNextFixedEndpoint(pdev);
            return USBH_CDC_HOST_TIMEOUT;
        }
        usbhCdcWaitOneMs();
    }
}

eUsbhCdcHostStatus usbhCdcHostReceive(USB_OTG_CORE_HANDLE *pdev, uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs)
{
    uint32_t startMs;
    URB_STATE urbState;

    if (actualLength != NULL) {
        *actualLength = 0U;
    }

    if ((pdev == NULL) || (buffer == NULL) || (length == 0U) || (actualLength == NULL)) {
        return USBH_CDC_HOST_INVALID_PARAM;
    }

    if (!gUsbhCdcContext.isReady || (gUsbhCdcContext.bulkInChannel == USBH_CDC_INVALID_CHANNEL)) {
        return USBH_CDC_HOST_NOT_READY;
    }

    timeoutMs = usbhCdcGetTimeout(timeoutMs);
    startMs = repRtosGetTickMs();
    (void)USBH_BulkReceiveData(pdev, buffer, length, gUsbhCdcContext.bulkInChannel);

    for (;;) {
        urbState = HCD_GetURB_State(pdev, gUsbhCdcContext.bulkInChannel);
        if (urbState == URB_DONE) {
            *actualLength = (uint16_t)HCD_GetXferCnt(pdev, gUsbhCdcContext.bulkInChannel);
            return USBH_CDC_HOST_OK;
        }
        if (urbState == URB_NOTREADY) {
            return USBH_CDC_HOST_OK;
        }
        if ((urbState == URB_ERROR) || (urbState == URB_STALL)) {
            return USBH_CDC_HOST_ERROR;
        }
        if (usbhCdcIsTimeout(startMs, timeoutMs)) {
            return USBH_CDC_HOST_OK;
        }
        usbhCdcWaitOneMs();
    }
}

uint8_t usbhCdcHostGetInEndpoint(void)
{
    return gUsbhCdcContext.bulkInEp;
}

uint8_t usbhCdcHostGetOutEndpoint(void)
{
    return gUsbhCdcContext.bulkOutEp;
}

static USBH_Status usbhCdcInterfaceInit(USB_OTG_CORE_HANDLE *pdev, void *phost)
{
    USBH_HOST *host = (USBH_HOST *)phost;

    if ((pdev == NULL) || (host == NULL)) {
        return USBH_FAIL;
    }

    usbhCdcHostReset();
    if (!usbhCdcFindBulkEndpoints(host)) {
        host->usr_cb->DeviceNotSupported();
        return USBH_NOT_SUPPORTED;
    }

    if (gUsbhCdcContext.skipClassRequest) {
        if (!usbhCdcOpenBulkChannels(pdev, host->device_prop.address, host->device_prop.speed)) {
            return USBH_FAIL;
        }
    }

    LOG_W(USBH_CDC_LOG_TAG,
          "cdc bulk endpoints in=0x%02x out=0x%02x if=%u fixed=%u",
          gUsbhCdcContext.bulkInEp,
          gUsbhCdcContext.bulkOutEp,
          gUsbhCdcContext.dataInterface,
          gUsbhCdcContext.skipClassRequest ? 1U : 0U);
    return USBH_OK;
}

static void usbhCdcInterfaceDeInit(USB_OTG_CORE_HANDLE *pdev, void *phost)
{
    (void)phost;

    if (pdev != NULL) {
        if (gUsbhCdcContext.bulkOutChannel != USBH_CDC_INVALID_CHANNEL) {
            USB_OTG_HC_Halt(pdev, gUsbhCdcContext.bulkOutChannel);
            (void)USBH_Free_Channel(pdev, gUsbhCdcContext.bulkOutChannel);
        }
        if (gUsbhCdcContext.bulkInChannel != USBH_CDC_INVALID_CHANNEL) {
            USB_OTG_HC_Halt(pdev, gUsbhCdcContext.bulkInChannel);
            (void)USBH_Free_Channel(pdev, gUsbhCdcContext.bulkInChannel);
        }
    }

    usbhCdcHostReset();
}

static USBH_Status usbhCdcClassRequest(USB_OTG_CORE_HANDLE *pdev, void *phost)
{
    USBH_HOST *host = (USBH_HOST *)phost;
    USBH_Status status;

    if ((pdev == NULL) || (host == NULL)) {
        return USBH_FAIL;
    }

    if (gUsbhCdcContext.skipClassRequest) {
        gUsbhCdcContext.requestState = USBH_CDC_REQ_DONE;
        gUsbhCdcContext.isReady = true;
        gUsbhCdcReadyTickMs = repRtosGetTickMs();
        LOG_W(USBH_CDC_LOG_TAG, "skip ec800m cdc class request, transport ready delay");
        return USBH_OK;
    }

    switch (gUsbhCdcContext.requestState) {
        case USBH_CDC_REQ_SET_LINE_CODING:
            status = usbhCdcSetLineCoding(pdev, host);
            if ((status == USBH_OK) || (status == USBH_NOT_SUPPORTED)) {
                gUsbhCdcContext.requestState = USBH_CDC_REQ_SET_CONTROL_LINE_STATE;
                status = USBH_BUSY;
            }
            return status;
        case USBH_CDC_REQ_SET_CONTROL_LINE_STATE:
            status = usbhCdcSetControlLineState(pdev, host);
            if ((status == USBH_OK) || (status == USBH_NOT_SUPPORTED) ||
                ((status == USBH_FAIL) &&
                 (host->device_prop.Dev_Desc.idVendor == USBH_CDC_QUECTEL_EC800M_VID) &&
                 (host->device_prop.Dev_Desc.idProduct == USBH_CDC_QUECTEL_EC800M_PID))) {
                gUsbhCdcContext.requestState = USBH_CDC_REQ_SET_INTERFACE;
                return USBH_BUSY;
            }
            return status;
        case USBH_CDC_REQ_SET_INTERFACE:
            status = usbhCdcSetInterface(pdev, host);
            if ((status == USBH_OK) || (status == USBH_NOT_SUPPORTED) ||
                ((status == USBH_FAIL) &&
                 (host->device_prop.Dev_Desc.idVendor == USBH_CDC_QUECTEL_EC800M_VID) &&
                 (host->device_prop.Dev_Desc.idProduct == USBH_CDC_QUECTEL_EC800M_PID))) {
                if (!usbhCdcFinishClassRequest(pdev, host)) {
                    return USBH_FAIL;
                }
                return USBH_OK;
            }
            return status;
        case USBH_CDC_REQ_DONE:
            gUsbhCdcContext.isReady = true;
            return USBH_OK;
        default:
            return USBH_FAIL;
    }
}

static USBH_Status usbhCdcHandle(USB_OTG_CORE_HANDLE *pdev, void *phost)
{
    (void)pdev;
    (void)phost;
    return USBH_OK;
}

static bool usbhCdcFindBulkEndpoints(USBH_HOST *host)
{
    uint8_t interfaceIndex;
    stUsbhCdcHostContext firstCandidate;
    stUsbhCdcHostContext preferredCandidate;
    bool hasFirstCandidate = false;
    bool hasPreferredCandidate = false;

    memset(&firstCandidate, 0, sizeof(firstCandidate));
    memset(&preferredCandidate, 0, sizeof(preferredCandidate));

    for (interfaceIndex = 0U;
         (interfaceIndex < host->device_prop.Cfg_Desc.bNumInterfaces) && (interfaceIndex < USBH_MAX_NUM_INTERFACES);
         interfaceIndex++) {
        USBH_InterfaceDesc_TypeDef *interfaceDesc = &host->device_prop.Itf_Desc[interfaceIndex];
        stUsbhCdcHostContext candidate;

        if (!usbhCdcCaptureCandidate(host, interfaceIndex, &candidate)) {
            continue;
        }

          LOG_W(USBH_CDC_LOG_TAG,
              "candidate if=%u class=0x%02x sub=0x%02x proto=0x%02x in=0x%02x out=0x%02x",
              interfaceDesc->bInterfaceNumber,
              interfaceDesc->bInterfaceClass,
              interfaceDesc->bInterfaceSubClass,
              interfaceDesc->bInterfaceProtocol,
              candidate.bulkInEp,
              candidate.bulkOutEp);

        if (!hasFirstCandidate) {
            firstCandidate = candidate;
            hasFirstCandidate = true;
        }

        if ((interfaceDesc->bInterfaceClass == USBH_CDC_DATA_CLASS_CODE) ||
            ((interfaceDesc->bInterfaceClass == USBH_CDC_VENDOR_CLASS_CODE) &&
             (interfaceDesc->bInterfaceNumber == USBH_CDC_QUECTEL_EC800M_AT_INTERFACE))) {
            preferredCandidate = candidate;
            hasPreferredCandidate = true;
        }
    }

    if (hasPreferredCandidate) {
        gUsbhCdcContext = preferredCandidate;
        return true;
    }

    if (hasFirstCandidate) {
        gUsbhCdcContext = firstCandidate;
        return true;
    }

    LOG_W(USBH_CDC_LOG_TAG,
          "no cdc bulk pair vid=0x%04x pid=0x%04x ifs=%u",
          host->device_prop.Dev_Desc.idVendor,
          host->device_prop.Dev_Desc.idProduct,
          host->device_prop.Cfg_Desc.bNumInterfaces);

    if ((host->device_prop.Dev_Desc.idVendor == USBH_CDC_QUECTEL_EC800M_VID) &&
        (host->device_prop.Dev_Desc.idProduct == USBH_CDC_QUECTEL_EC800M_PID)) {
        if (gUsbhCdcFixedEndpointIndex >= USBH_CDC_QUECTEL_EC800M_EP_PAIR_COUNT) {
            gUsbhCdcFixedEndpointIndex = 0U;
        }
        gUsbhCdcContext.bulkInEp = gUsbhCdcFixedBulkInEp[gUsbhCdcFixedEndpointIndex];
        gUsbhCdcContext.bulkOutEp = gUsbhCdcFixedBulkOutEp[gUsbhCdcFixedEndpointIndex];
        gUsbhCdcContext.bulkInMps = USBH_CDC_QUECTEL_EC800M_AT_BULK_MPS;
        gUsbhCdcContext.bulkOutMps = USBH_CDC_QUECTEL_EC800M_AT_BULK_MPS;
        gUsbhCdcContext.dataInterface = (uint8_t)(USBH_CDC_QUECTEL_EC800M_AT_INTERFACE + gUsbhCdcFixedEndpointIndex);
        gUsbhCdcContext.skipClassRequest = false;
        LOG_W(USBH_CDC_LOG_TAG,
              "use ec800m fixed endpoints idx=%u in=0x%02x out=0x%02x if=%u",
              gUsbhCdcFixedEndpointIndex,
              gUsbhCdcContext.bulkInEp,
              gUsbhCdcContext.bulkOutEp,
              gUsbhCdcContext.dataInterface);
        gUsbhCdcFixedEndpointIndex++;
        return true;
    }

    return false;
}

static bool usbhCdcCaptureCandidate(USBH_HOST *host, uint8_t interfaceIndex, stUsbhCdcHostContext *candidate)
{
    USBH_InterfaceDesc_TypeDef *interfaceDesc = &host->device_prop.Itf_Desc[interfaceIndex];
    uint8_t endpointIndex;
    bool classLooksLikeData = (interfaceDesc->bInterfaceClass == USBH_CDC_DATA_CLASS_CODE) ||
                              (interfaceDesc->bInterfaceClass == USBH_CDC_CLASS_CODE) ||
                              (interfaceDesc->bInterfaceClass == USBH_CDC_VENDOR_CLASS_CODE);

    if ((candidate == NULL) || !classLooksLikeData) {
        return false;
    }

    memset(candidate, 0, sizeof(*candidate));
    candidate->bulkInChannel = USBH_CDC_INVALID_CHANNEL;
    candidate->bulkOutChannel = USBH_CDC_INVALID_CHANNEL;
    candidate->lineCoding = gUsbhCdcContext.lineCoding;
    candidate->dataInterface = interfaceDesc->bInterfaceNumber;
    candidate->skipClassRequest = false;

    for (endpointIndex = 0U;
         (endpointIndex < interfaceDesc->bNumEndpoints) && (endpointIndex < USBH_MAX_NUM_ENDPOINTS);
         endpointIndex++) {
        USBH_EpDesc_TypeDef *endpointDesc = &host->device_prop.Ep_Desc[interfaceIndex][endpointIndex];
        if ((endpointDesc->bmAttributes & USBH_CDC_BULK_ATTR_MASK) != USB_EP_TYPE_BULK) {
            continue;
        }
        if ((endpointDesc->bEndpointAddress & USB_EP_DIR_IN) != 0U) {
            candidate->bulkInEp = endpointDesc->bEndpointAddress;
            candidate->bulkInMps = endpointDesc->wMaxPacketSize;
        } else {
            candidate->bulkOutEp = endpointDesc->bEndpointAddress;
            candidate->bulkOutMps = endpointDesc->wMaxPacketSize;
        }
    }

    if ((candidate->bulkInEp == 0U) || (candidate->bulkOutEp == 0U)) {
        return false;
    }

    candidate->bulkInMps = (candidate->bulkInMps != 0U) ? candidate->bulkInMps : USBH_CDC_MPS_SIZE;
    candidate->bulkOutMps = (candidate->bulkOutMps != 0U) ? candidate->bulkOutMps : USBH_CDC_MPS_SIZE;
    return true;
}

static bool usbhCdcOpenBulkChannels(USB_OTG_CORE_HANDLE *pdev, uint8_t devAddr, uint8_t speed)
{
    if (pdev == NULL) {
        return false;
    }

    gUsbhCdcContext.bulkOutChannel = USBH_Alloc_Channel(pdev, gUsbhCdcContext.bulkOutEp);
    gUsbhCdcContext.bulkInChannel = USBH_Alloc_Channel(pdev, gUsbhCdcContext.bulkInEp);
    if ((gUsbhCdcContext.bulkOutChannel == USBH_CDC_INVALID_CHANNEL) ||
        (gUsbhCdcContext.bulkInChannel == USBH_CDC_INVALID_CHANNEL)) {
        return false;
    }

    (void)USBH_Open_Channel(pdev,
                            gUsbhCdcContext.bulkOutChannel,
                            devAddr,
                            speed,
                            EP_TYPE_BULK,
                            gUsbhCdcContext.bulkOutMps);
    (void)USBH_Open_Channel(pdev,
                            gUsbhCdcContext.bulkInChannel,
                            devAddr,
                            speed,
                            EP_TYPE_BULK,
                            gUsbhCdcContext.bulkInMps);
    return true;
}

static void usbhCdcCloseBulkChannels(USB_OTG_CORE_HANDLE *pdev)
{
    if (pdev == NULL) {
        return;
    }

    if (gUsbhCdcContext.bulkOutChannel != USBH_CDC_INVALID_CHANNEL) {
        USB_OTG_HC_Halt(pdev, gUsbhCdcContext.bulkOutChannel);
        (void)USBH_Free_Channel(pdev, gUsbhCdcContext.bulkOutChannel);
        gUsbhCdcContext.bulkOutChannel = USBH_CDC_INVALID_CHANNEL;
    }
    if (gUsbhCdcContext.bulkInChannel != USBH_CDC_INVALID_CHANNEL) {
        USB_OTG_HC_Halt(pdev, gUsbhCdcContext.bulkInChannel);
        (void)USBH_Free_Channel(pdev, gUsbhCdcContext.bulkInChannel);
        gUsbhCdcContext.bulkInChannel = USBH_CDC_INVALID_CHANNEL;
    }
}

static bool usbhCdcSwitchToNextFixedEndpoint(USB_OTG_CORE_HANDLE *pdev)
{
    uint8_t devAddr;
    uint8_t speed;

    if ((pdev == NULL) || !gUsbhCdcContext.skipClassRequest ||
        (gUsbhCdcFixedEndpointIndex >= USBH_CDC_QUECTEL_EC800M_EP_PAIR_COUNT)) {
        return false;
    }

    devAddr = (gUsbhCdcContext.bulkOutChannel != USBH_CDC_INVALID_CHANNEL) ?
              pdev->host.hc[gUsbhCdcContext.bulkOutChannel].dev_addr : USBH_DEVICE_ADDRESS;
    speed = (gUsbhCdcContext.bulkOutChannel != USBH_CDC_INVALID_CHANNEL) ?
            pdev->host.hc[gUsbhCdcContext.bulkOutChannel].speed : HPRT0_PRTSPD_FULL_SPEED;

    usbhCdcCloseBulkChannels(pdev);

    gUsbhCdcContext.bulkInEp = gUsbhCdcFixedBulkInEp[gUsbhCdcFixedEndpointIndex];
    gUsbhCdcContext.bulkOutEp = gUsbhCdcFixedBulkOutEp[gUsbhCdcFixedEndpointIndex];
    gUsbhCdcContext.bulkInMps = USBH_CDC_QUECTEL_EC800M_AT_BULK_MPS;
    gUsbhCdcContext.bulkOutMps = USBH_CDC_QUECTEL_EC800M_AT_BULK_MPS;
    gUsbhCdcContext.dataInterface = (uint8_t)(USBH_CDC_QUECTEL_EC800M_AT_INTERFACE + gUsbhCdcFixedEndpointIndex);
    if (!usbhCdcOpenBulkChannels(pdev, devAddr, speed)) {
        return false;
    }

    LOG_W(USBH_CDC_LOG_TAG,
          "switch ec800m endpoints idx=%u in=0x%02x out=0x%02x if=%u",
          gUsbhCdcFixedEndpointIndex,
          gUsbhCdcContext.bulkInEp,
          gUsbhCdcContext.bulkOutEp,
          gUsbhCdcContext.dataInterface);
    gUsbhCdcFixedEndpointIndex++;
    return true;
}

static USBH_Status usbhCdcSetLineCoding(USB_OTG_CORE_HANDLE *pdev, USBH_HOST *host)
{
    gUsbhCdcLineCodingBuffer[0] = (uint8_t)(gUsbhCdcContext.lineCoding.bitRate & 0xFFU);
    gUsbhCdcLineCodingBuffer[1] = (uint8_t)((gUsbhCdcContext.lineCoding.bitRate >> 8) & 0xFFU);
    gUsbhCdcLineCodingBuffer[2] = (uint8_t)((gUsbhCdcContext.lineCoding.bitRate >> 16) & 0xFFU);
    gUsbhCdcLineCodingBuffer[3] = (uint8_t)((gUsbhCdcContext.lineCoding.bitRate >> 24) & 0xFFU);
    gUsbhCdcLineCodingBuffer[4] = gUsbhCdcContext.lineCoding.stopBits;
    gUsbhCdcLineCodingBuffer[5] = gUsbhCdcContext.lineCoding.parity;
    gUsbhCdcLineCodingBuffer[6] = gUsbhCdcContext.lineCoding.dataBits;

    host->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
    host->Control.setup.b.bRequest = USBH_CDC_REQ_SET_LINE_CODING_CODE;
    host->Control.setup.b.wValue.w = 0U;
    host->Control.setup.b.wIndex.w = gUsbhCdcContext.dataInterface;
    host->Control.setup.b.wLength.w = sizeof(gUsbhCdcLineCodingBuffer);
    return USBH_CtlReq(pdev, host, gUsbhCdcLineCodingBuffer, sizeof(gUsbhCdcLineCodingBuffer));
}

static USBH_Status usbhCdcSetControlLineState(USB_OTG_CORE_HANDLE *pdev, USBH_HOST *host)
{
    host->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
    host->Control.setup.b.bRequest = USBH_CDC_REQ_SET_CONTROL_LINE_STATE_CODE;
    host->Control.setup.b.wValue.w = USBH_CDC_CONTROL_LINE_DTR_RTS;
    host->Control.setup.b.wIndex.w = gUsbhCdcContext.dataInterface;
    host->Control.setup.b.wLength.w = 0U;
    return USBH_CtlReq(pdev, host, NULL, 0U);
}

static USBH_Status usbhCdcSetInterface(USB_OTG_CORE_HANDLE *pdev, USBH_HOST *host)
{
    return USBH_SetInterface(pdev, host, gUsbhCdcContext.dataInterface, USBH_CDC_DEFAULT_ALT_SETTING);
}

static bool usbhCdcFinishClassRequest(USB_OTG_CORE_HANDLE *pdev, USBH_HOST *host)
{
    USB_OTG_HC_Halt(pdev, host->Control.hc_num_out);
    USB_OTG_HC_Halt(pdev, host->Control.hc_num_in);
    (void)USBH_Free_Channel(pdev, host->Control.hc_num_out);
    (void)USBH_Free_Channel(pdev, host->Control.hc_num_in);
    if (!usbhCdcOpenBulkChannels(pdev, host->device_prop.address, host->device_prop.speed)) {
        return false;
    }
    gUsbhCdcContext.requestState = USBH_CDC_REQ_DONE;
    gUsbhCdcContext.isReady = true;
    return true;
}

static void usbhCdcLogTxState(USB_OTG_CORE_HANDLE *pdev, URB_STATE urbState, const char *reason)
{
    static uint8_t logCount;
    uint8_t channel;
    uint32_t hcint;
    uint32_t hcintmsk;
    uint32_t hcchar;
    uint32_t hctsiz;
    uint32_t hprt;
    uint32_t gintsts;
    uint32_t haint;

    if ((pdev == NULL) || (logCount >= 12U)) {
        return;
    }

    channel = gUsbhCdcContext.bulkOutChannel;
    if (channel == USBH_CDC_INVALID_CHANNEL) {
        return;
    }

    logCount++;
    hcint = USB_OTG_READ_REG32(&pdev->regs.HC_REGS[channel]->HCINT);
    hcintmsk = USB_OTG_READ_REG32(&pdev->regs.HC_REGS[channel]->HCINTMSK);
    hcchar = USB_OTG_READ_REG32(&pdev->regs.HC_REGS[channel]->HCCHAR);
    hctsiz = USB_OTG_READ_REG32(&pdev->regs.HC_REGS[channel]->HCTSIZ);
    hprt = USB_OTG_READ_REG32(pdev->regs.HPRT0);
    gintsts = USB_OTG_READ_REG32(&pdev->regs.GREGS->GINTSTS);
    haint = USB_OTG_READ_REG32(&pdev->regs.HREGS->HAINT);

    LOG_W(USBH_CDC_LOG_TAG,
          "tx %s ch=%u ep=0x%02x urb=%u hc=%u err=%u cnt=%lu hcint=0x%08lx msk=0x%08lx hcchar=0x%08lx hctsiz=0x%08lx hprt=0x%08lx gint=0x%08lx haint=0x%08lx",
          reason,
          channel,
          gUsbhCdcContext.bulkOutEp,
          (unsigned int)urbState,
          (unsigned int)HCD_GetHCState(pdev, channel),
          (unsigned int)pdev->host.ErrCnt[channel],
          (unsigned long)HCD_GetXferCnt(pdev, channel),
          (unsigned long)hcint,
          (unsigned long)hcintmsk,
          (unsigned long)hcchar,
          (unsigned long)hctsiz,
          (unsigned long)hprt,
          (unsigned long)gintsts,
          (unsigned long)haint);
}

static void usbhCdcProbeInEndpoint(USB_OTG_CORE_HANDLE *pdev)
{
    static bool probed;
    uint8_t probeByte;
    uint32_t startMs;
    URB_STATE urbState;

    if (probed || (pdev == NULL) || (gUsbhCdcContext.bulkInChannel == USBH_CDC_INVALID_CHANNEL)) {
        return;
    }

    probed = true;
    probeByte = 0U;
    startMs = repRtosGetTickMs();
    (void)USBH_BulkReceiveData(pdev, &probeByte, 1U, gUsbhCdcContext.bulkInChannel);
    do {
        urbState = HCD_GetURB_State(pdev, gUsbhCdcContext.bulkInChannel);
        if ((urbState == URB_DONE) || (urbState == URB_NOTREADY) ||
            (urbState == URB_ERROR) || (urbState == URB_STALL)) {
            break;
        }
        usbhCdcWaitOneMs();
    } while (!usbhCdcIsTimeout(startMs, 20U));

    LOG_W(USBH_CDC_LOG_TAG,
          "probe in ch=%u ep=0x%02x urb=%u hc=%u cnt=%lu",
          gUsbhCdcContext.bulkInChannel,
          gUsbhCdcContext.bulkInEp,
          (unsigned int)urbState,
          (unsigned int)HCD_GetHCState(pdev, gUsbhCdcContext.bulkInChannel),
          (unsigned long)HCD_GetXferCnt(pdev, gUsbhCdcContext.bulkInChannel));
}

static uint32_t usbhCdcGetTimeout(uint32_t timeoutMs)
{
    return (timeoutMs > 0U) ? timeoutMs : USBH_CDC_DEFAULT_TIMEOUT_MS;
}

static bool usbhCdcIsTimeout(uint32_t startMs, uint32_t timeoutMs)
{
    return (uint32_t)(repRtosGetTickMs() - startMs) >= timeoutMs;
}

static void usbhCdcWaitOneMs(void)
{
    if (repRtosIsSchedulerRunning()) {
        (void)repRtosDelayMs(1U);
    } else {
        USB_OTG_BSP_mDelay(1U);
    }
}

/**************************End of file********************************/
