/************************************************************************************
* @file     : usbh_cdc_host.h
* @brief    : USB Host CDC/ACM class interface for EC800M AT communication.
***********************************************************************************/
#ifndef NETWORK_USBH_CDC_HOST_H
#define NETWORK_USBH_CDC_HOST_H

#include <stdbool.h>
#include <stdint.h>

#include "usbh_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USBH_CDC_CLASS_CODE                      0x02U
#define USBH_CDC_DATA_CLASS_CODE                 0x0AU
#define USBH_CDC_VENDOR_CLASS_CODE               0xFFU
#define USBH_CDC_QUECTEL_EC800M_VID             0x2C7CU
#define USBH_CDC_QUECTEL_EC800M_PID             0x6002U
#define USBH_CDC_QUECTEL_EC800M_AT_INTERFACE    2U
#define USBH_CDC_QUECTEL_EC800M_EP_PAIR_COUNT   5U
#define USBH_CDC_QUECTEL_EC800M_AT_BULK_IN      0x83U
#define USBH_CDC_QUECTEL_EC800M_AT_BULK_OUT     0x03U
#define USBH_CDC_QUECTEL_EC800M_ALT0_BULK_IN    0x82U
#define USBH_CDC_QUECTEL_EC800M_ALT0_BULK_OUT   0x02U
#define USBH_CDC_QUECTEL_EC800M_ALT1_BULK_IN    0x84U
#define USBH_CDC_QUECTEL_EC800M_ALT1_BULK_OUT   0x04U
#define USBH_CDC_QUECTEL_EC800M_ALT2_BULK_IN    0x85U
#define USBH_CDC_QUECTEL_EC800M_ALT2_BULK_OUT   0x05U
#define USBH_CDC_QUECTEL_EC800M_ALT3_BULK_IN    0x81U
#define USBH_CDC_QUECTEL_EC800M_ALT3_BULK_OUT   0x01U
#define USBH_CDC_QUECTEL_EC800M_AT_BULK_MPS     64U
#define USBH_CDC_QUECTEL_EC800M_READY_DELAY_MS  2000U

typedef enum eUsbhCdcHostStatus {
    USBH_CDC_HOST_OK = 0,
    USBH_CDC_HOST_INVALID_PARAM,
    USBH_CDC_HOST_NOT_READY,
    USBH_CDC_HOST_BUSY,
    USBH_CDC_HOST_TIMEOUT,
    USBH_CDC_HOST_ERROR,
} eUsbhCdcHostStatus;

typedef enum eUsbhCdcRequestState {
    USBH_CDC_REQ_SET_LINE_CODING = 0,
    USBH_CDC_REQ_SET_CONTROL_LINE_STATE,
    USBH_CDC_REQ_DONE,
} eUsbhCdcRequestState;

typedef struct stUsbhCdcLineCoding {
    uint32_t bitRate;
    uint8_t stopBits;
    uint8_t parity;
    uint8_t dataBits;
} stUsbhCdcLineCoding;

typedef struct stUsbhCdcHostContext {
    bool isReady;
    uint8_t dataInterface;
    uint8_t bulkInEp;
    uint8_t bulkOutEp;
    uint16_t bulkInMps;
    uint16_t bulkOutMps;
    uint8_t bulkInChannel;
    uint8_t bulkOutChannel;
    bool skipClassRequest;
    eUsbhCdcRequestState requestState;
    stUsbhCdcLineCoding lineCoding;
} stUsbhCdcHostContext;

extern USBH_Class_cb_TypeDef USBH_CDC_cb;

void usbhCdcHostReset(void);
bool usbhCdcHostIsReady(void);
eUsbhCdcHostStatus usbhCdcHostTransmit(USB_OTG_CORE_HANDLE *pdev, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eUsbhCdcHostStatus usbhCdcHostReceive(USB_OTG_CORE_HANDLE *pdev, uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs);
uint8_t usbhCdcHostGetInEndpoint(void);
uint8_t usbhCdcHostGetOutEndpoint(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
