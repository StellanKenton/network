/************************************************************************************
* @file     : usbhost_ec800.c
* @brief    : EC800M USB host bridge implementation.
***********************************************************************************/
#include "usbhost_ec800.h"

#include <string.h>

#include "usb_bsp.h"
#include "usb_hcd.h"
#include "usb_hcd_int.h"
#include "usbh_cdc_host.h"

#include "../../../rep/service/log/log.h"
#include "../../../rep/service/rtos/rtos.h"

#define USBHOST_EC800_LOG_TAG                  "usbHost"

static void usbHostUsrInit(void);
static void usbHostUsrDeInit(void);
static void usbHostUsrDeviceAttached(void);
static void usbHostUsrResetDevice(void);
static void usbHostUsrDeviceDisconnected(void);
static void usbHostUsrOverCurrentDetected(void);
static void usbHostUsrDeviceSpeedDetected(uint8_t deviceSpeed);
static void usbHostUsrDeviceDescAvailable(void *deviceDesc);
static void usbHostUsrDeviceAddressAssigned(void);
static void usbHostUsrConfigurationDescAvailable(USBH_CfgDesc_TypeDef *cfgDesc,
                                                 USBH_InterfaceDesc_TypeDef *interfaceDesc,
                                                 USBH_EpDesc_TypeDef *endpointDesc);
static void usbHostUsrManufacturerString(void *manufacturerString);
static void usbHostUsrProductString(void *productString);
static void usbHostUsrSerialNumString(void *serialNumString);
static void usbHostUsrEnumerationDone(void);
static USBH_USR_Status usbHostUsrUserInput(void);
static int usbHostUsrApplication(void);
static void usbHostUsrDeviceNotSupported(void);
static void usbHostUsrUnrecoveredError(void);
static eDrvStatus usbHostEc800MapCdcStatus(eUsbhCdcHostStatus status);
static void usbHostEc800DelayUs(uint32_t usec);
static void usbHostEc800LogReadyState(void);

static USB_OTG_CORE_HANDLE gUsbHostCore;
static USBH_HOST gUsbHost;
static bool gUsbHostInitialized;
static bool gUsbHostStarted;
static bool gUsbHostEnumerated;
static uint32_t gUsbHostNextPortLogTick;

static USBH_Usr_cb_TypeDef gUsbHostUsrCb = {
    usbHostUsrInit,
    usbHostUsrDeInit,
    usbHostUsrDeviceAttached,
    usbHostUsrResetDevice,
    usbHostUsrDeviceDisconnected,
    usbHostUsrOverCurrentDetected,
    usbHostUsrDeviceSpeedDetected,
    usbHostUsrDeviceDescAvailable,
    usbHostUsrDeviceAddressAssigned,
    usbHostUsrConfigurationDescAvailable,
    usbHostUsrManufacturerString,
    usbHostUsrProductString,
    usbHostUsrSerialNumString,
    usbHostUsrEnumerationDone,
    usbHostUsrUserInput,
    usbHostUsrApplication,
    usbHostUsrDeviceNotSupported,
    usbHostUsrUnrecoveredError,
};

eDrvStatus usbHostEc800Init(void)
{
    if (gUsbHostInitialized) {
        return DRV_STATUS_OK;
    }

    memset(&gUsbHostCore, 0, sizeof(gUsbHostCore));
    memset(&gUsbHost, 0, sizeof(gUsbHost));
    usbhCdcHostReset();
    gUsbHostInitialized = true;
    return DRV_STATUS_OK;
}

eDrvStatus usbHostEc800Start(void)
{
    if (!gUsbHostInitialized) {
        return DRV_STATUS_NOT_READY;
    }

    if (gUsbHostStarted) {
        return DRV_STATUS_OK;
    }

    USBH_Init(&gUsbHostCore, USB_OTG_FS_CORE_ID, &gUsbHost, &USBH_CDC_cb, &gUsbHostUsrCb);
    gUsbHostStarted = true;
    return DRV_STATUS_OK;
}

eDrvStatus usbHostEc800Stop(void)
{
    if (!gUsbHostInitialized) {
        return DRV_STATUS_NOT_READY;
    }

    (void)USBH_DeInit(&gUsbHostCore, &gUsbHost);
    usbhCdcHostReset();
    gUsbHostStarted = false;
    gUsbHostEnumerated = false;
    return DRV_STATUS_OK;
}

void usbHostEc800Process(void)
{
    if (gUsbHostStarted) {
        USBH_Process(&gUsbHostCore, &gUsbHost);
    }
}

void usbHostEc800HandleIrq(void)
{
    if (gUsbHostStarted) {
        USBH_OTG_ISR_Handler(&gUsbHostCore);
    }
}

eDrvStatus usbHostEc800Transmit(uint8_t endpointAddress, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    eDrvStatus status;

    (void)endpointAddress;
    usbHostEc800Process();
    status = usbHostEc800MapCdcStatus(usbhCdcHostTransmit(&gUsbHostCore, buffer, length, timeoutMs));
    if (status != DRV_STATUS_OK) {
        LOG_W(USBHOST_EC800_LOG_TAG, "tx status=%d len=%u", (int)status, length);
    }
    return status;
}

eDrvStatus usbHostEc800Receive(uint8_t endpointAddress, uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs)
{
    (void)endpointAddress;
    usbHostEc800Process();
    return usbHostEc800MapCdcStatus(usbhCdcHostReceive(&gUsbHostCore, buffer, length, actualLength, timeoutMs));
}

bool usbHostEc800IsConnected(void)
{
    usbHostEc800Process();
    return gUsbHostStarted && (HCD_IsDeviceConnected(&gUsbHostCore) != 0U);
}

bool usbHostEc800IsConfigured(void)
{
    usbHostEc800Process();
    if (gUsbHostStarted && !(gUsbHostEnumerated && usbhCdcHostIsReady())) {
        usbHostEc800LogReadyState();
    }
    return gUsbHostStarted && gUsbHostEnumerated && usbhCdcHostIsReady();
}

eDrvUsbSpeed usbHostEc800GetSpeed(void)
{
    if (!gUsbHostStarted || (HCD_IsDeviceConnected(&gUsbHostCore) == 0U)) {
        return DRVUSB_SPEED_UNKNOWN;
    }

    switch (HCD_GetCurrentSpeed(&gUsbHostCore)) {
        case HPRT0_PRTSPD_LOW_SPEED:
            return DRVUSB_SPEED_LOW;
        case HPRT0_PRTSPD_FULL_SPEED:
            return DRVUSB_SPEED_FULL;
        case HPRT0_PRTSPD_HIGH_SPEED:
            return DRVUSB_SPEED_HIGH;
        default:
            return DRVUSB_SPEED_UNKNOWN;
    }
}

void USB_OTG_BSP_Init(USB_OTG_CORE_HANDLE *pdev)
{
    GPIO_InitTypeDef gpioInit;

    (void)pdev;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_OTG_FS, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource11, GPIO_AF_OTG_FS);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource12, GPIO_AF_OTG_FS);

    GPIO_StructInit(&gpioInit);
    gpioInit.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12;
    gpioInit.GPIO_Mode = GPIO_Mode_AF;
    gpioInit.GPIO_OType = GPIO_OType_PP;
    gpioInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
    gpioInit.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOA, &gpioInit);

}

void USB_OTG_BSP_uDelay(const uint32_t usec)
{
    usbHostEc800DelayUs(usec);
}

void USB_OTG_BSP_mDelay(const uint32_t msec)
{
    if (repRtosIsSchedulerRunning()) {
        (void)repRtosDelayMs(msec);
    } else {
        usbHostEc800DelayUs(msec * 1000UL);
    }
}

void USB_OTG_BSP_EnableInterrupt(USB_OTG_CORE_HANDLE *pdev)
{
    NVIC_InitTypeDef nvicInit;

    (void)pdev;
    nvicInit.NVIC_IRQChannel = OTG_FS_IRQn;
    nvicInit.NVIC_IRQChannelPreemptionPriority = 5U;
    nvicInit.NVIC_IRQChannelSubPriority = 0U;
    nvicInit.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvicInit);
}

void USB_OTG_BSP_ConfigVBUS(USB_OTG_CORE_HANDLE *pdev)
{
    (void)pdev;
}

void USB_OTG_BSP_DriveVBUS(USB_OTG_CORE_HANDLE *pdev, uint8_t state)
{
    (void)pdev;
    (void)state;
}

static void usbHostUsrInit(void)
{
    LOG_W(USBHOST_EC800_LOG_TAG, "host init");
}

static void usbHostUsrDeInit(void)
{
    gUsbHostEnumerated = false;
}

static void usbHostUsrDeviceAttached(void)
{
    LOG_W(USBHOST_EC800_LOG_TAG, "device attached");
}

static void usbHostUsrResetDevice(void)
{
    LOG_W(USBHOST_EC800_LOG_TAG, "device reset");
}

static void usbHostUsrDeviceDisconnected(void)
{
    LOG_W(USBHOST_EC800_LOG_TAG, "device disconnected");
    gUsbHostEnumerated = false;
    usbhCdcHostReset();
}

static void usbHostUsrOverCurrentDetected(void)
{
    LOG_E(USBHOST_EC800_LOG_TAG, "over current");
}

static void usbHostUsrDeviceSpeedDetected(uint8_t deviceSpeed)
{
    LOG_W(USBHOST_EC800_LOG_TAG, "speed=%u", deviceSpeed);
}

static void usbHostUsrDeviceDescAvailable(void *deviceDesc)
{
    USBH_DevDesc_TypeDef *desc = (USBH_DevDesc_TypeDef *)deviceDesc;

    if (desc != NULL) {
        LOG_W(USBHOST_EC800_LOG_TAG, "vid=0x%04x pid=0x%04x class=0x%02x", desc->idVendor, desc->idProduct, desc->bDeviceClass);
    }
}

static void usbHostUsrDeviceAddressAssigned(void)
{
}

static void usbHostUsrConfigurationDescAvailable(USBH_CfgDesc_TypeDef *cfgDesc,
                                                 USBH_InterfaceDesc_TypeDef *interfaceDesc,
                                                 USBH_EpDesc_TypeDef *endpointDesc)
{
    (void)endpointDesc;
    if ((cfgDesc != NULL) && (interfaceDesc != NULL)) {
          LOG_W(USBHOST_EC800_LOG_TAG,
              "cfg val=%u ifs=%u attr=0x%02x maxPower=%umA firstClass=0x%02x eps=%u",
              cfgDesc->bConfigurationValue,
              cfgDesc->bNumInterfaces,
              cfgDesc->bmAttributes,
              (uint16_t)cfgDesc->bMaxPower * 2U,
              interfaceDesc->bInterfaceClass,
              interfaceDesc->bNumEndpoints);
    }
}

static void usbHostUsrManufacturerString(void *manufacturerString)
{
    (void)manufacturerString;
}

static void usbHostUsrProductString(void *productString)
{
    (void)productString;
}

static void usbHostUsrSerialNumString(void *serialNumString)
{
    (void)serialNumString;
}

static void usbHostUsrEnumerationDone(void)
{
    LOG_W(USBHOST_EC800_LOG_TAG, "enumeration done");
    gUsbHostEnumerated = true;
}

static USBH_USR_Status usbHostUsrUserInput(void)
{
    return USBH_USR_RESP_OK;
}

static int usbHostUsrApplication(void)
{
    return 0;
}

static void usbHostUsrDeviceNotSupported(void)
{
    LOG_W(USBHOST_EC800_LOG_TAG, "device not supported");
}

static void usbHostUsrUnrecoveredError(void)
{
    LOG_E(USBHOST_EC800_LOG_TAG, "unrecovered error");
}

static eDrvStatus usbHostEc800MapCdcStatus(eUsbhCdcHostStatus status)
{
    switch (status) {
        case USBH_CDC_HOST_OK:
            return DRV_STATUS_OK;
        case USBH_CDC_HOST_INVALID_PARAM:
            return DRV_STATUS_INVALID_PARAM;
        case USBH_CDC_HOST_NOT_READY:
            return DRV_STATUS_NOT_READY;
        case USBH_CDC_HOST_BUSY:
            return DRV_STATUS_BUSY;
        case USBH_CDC_HOST_TIMEOUT:
            return DRV_STATUS_TIMEOUT;
        default:
            return DRV_STATUS_ERROR;
    }
}

static void usbHostEc800DelayUs(uint32_t usec)
{
    volatile uint32_t count;
    volatile uint32_t cycles = (SystemCoreClock / 1000000UL / 5UL) * usec;

    for (count = 0U; count < cycles; count++) {
    }
}

static void usbHostEc800LogReadyState(void)
{
    uint32_t nowTick = repRtosGetTickMs();
    uint32_t hprt;
    uint16_t gpioIdr;

    if ((gUsbHostNextPortLogTick != 0U) && ((uint32_t)(nowTick - gUsbHostNextPortLogTick) > 0x7FFFFFFFUL)) {
        return;
    }

    gUsbHostNextPortLogTick = nowTick + 1000U;
    hprt = USB_OTG_READ_REG32(gUsbHostCore.regs.HPRT0);
    gpioIdr = GPIO_ReadInputData(GPIOA);
    LOG_W(USBHOST_EC800_LOG_TAG,
          "wait conn=%u enumDone=%u cdc=%u hprt=0x%08x dp=%u dm=%u state=%u enum=%u in=0x%02x out=0x%02x",
          (unsigned int)HCD_IsDeviceConnected(&gUsbHostCore),
          gUsbHostEnumerated ? 1U : 0U,
          usbhCdcHostIsReady() ? 1U : 0U,
          (unsigned int)hprt,
          ((gpioIdr & GPIO_Pin_12) != 0U) ? 1U : 0U,
          ((gpioIdr & GPIO_Pin_11) != 0U) ? 1U : 0U,
          (unsigned int)gUsbHost.gState,
          (unsigned int)gUsbHost.EnumState,
          (unsigned int)usbhCdcHostGetInEndpoint(),
          (unsigned int)usbhCdcHostGetOutEndpoint());
}

/**************************End of file********************************/
