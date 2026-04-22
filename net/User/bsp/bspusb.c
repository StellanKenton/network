/***********************************************************************************
* @file     : bspusb.c
* @brief    : Board USB BSP implementation.
**********************************************************************************/
#include "bspusb.h"

#include "../../SYSTEM/sys/sys.h"

static uint8_t gBspUsbInitialized[DRVUSB_MAX];

eDrvStatus bspUsbInit(uint8_t usb)
{
	GPIO_InitTypeDef lGpioInit;

	if (usb != DRVUSB_DEV0) {
		return DRV_STATUS_INVALID_PARAM;
	}

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_OTG_FS, ENABLE);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource11, GPIO_AF_OTG_FS);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource12, GPIO_AF_OTG_FS);

	GPIO_StructInit(&lGpioInit);
	lGpioInit.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12;
	lGpioInit.GPIO_Mode = GPIO_Mode_AF;
	lGpioInit.GPIO_OType = GPIO_OType_PP;
	lGpioInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
	lGpioInit.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOA, &lGpioInit);

	gBspUsbInitialized[usb] = 1U;
	return DRV_STATUS_OK;
}

eDrvStatus bspUsbStart(uint8_t usb)
{
	if ((usb >= DRVUSB_MAX) || (gBspUsbInitialized[usb] == 0U)) {
		return DRV_STATUS_NOT_READY;
	}

	return DRV_STATUS_UNSUPPORTED;
}

eDrvStatus bspUsbStop(uint8_t usb)
{
	if ((usb >= DRVUSB_MAX) || (gBspUsbInitialized[usb] == 0U)) {
		return DRV_STATUS_NOT_READY;
	}

	return DRV_STATUS_UNSUPPORTED;
}

eDrvStatus bspUsbSetConnect(uint8_t usb, bool isConnect)
{
	(void)usb;
	(void)isConnect;
	return DRV_STATUS_UNSUPPORTED;
}

eDrvStatus bspUsbOpenEndpoint(uint8_t usb, const stDrvUsbEndpointConfig *config)
{
	(void)usb;
	(void)config;
	return DRV_STATUS_UNSUPPORTED;
}

eDrvStatus bspUsbCloseEndpoint(uint8_t usb, uint8_t endpointAddress)
{
	(void)usb;
	(void)endpointAddress;
	return DRV_STATUS_UNSUPPORTED;
}

eDrvStatus bspUsbFlushEndpoint(uint8_t usb, uint8_t endpointAddress)
{
	(void)usb;
	(void)endpointAddress;
	return DRV_STATUS_UNSUPPORTED;
}

eDrvStatus bspUsbTransmit(uint8_t usb, uint8_t endpointAddress, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
	(void)usb;
	(void)endpointAddress;
	(void)buffer;
	(void)length;
	(void)timeoutMs;
	return DRV_STATUS_UNSUPPORTED;
}

eDrvStatus bspUsbReceive(uint8_t usb, uint8_t endpointAddress, uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs)
{
	(void)usb;
	(void)endpointAddress;
	(void)buffer;
	(void)length;
	(void)timeoutMs;
	if (actualLength != NULL) {
		*actualLength = 0U;
	}
	return DRV_STATUS_UNSUPPORTED;
}

bool bspUsbIsConnected(uint8_t usb)
{
	(void)usb;
	return false;
}

bool bspUsbIsConfigured(uint8_t usb)
{
	(void)usb;
	return false;
}

eDrvUsbSpeed bspUsbGetSpeed(uint8_t usb)
{
	(void)usb;
	return DRVUSB_SPEED_UNKNOWN;
}

/**************************End of file********************************/