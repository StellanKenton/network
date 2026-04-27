/***********************************************************************************
* @file     : bspusb.c
* @brief    : Board USB BSP implementation.
**********************************************************************************/
#include "bspusb.h"

#include "../port/drvusb_port.h"
#include "usbhost/inc/usbhost_ec800.h"

static uint8_t gBspUsbInitialized[DRVUSB_MAX];

eDrvStatus bspUsbInit(uint8_t usb)
{
	if (usb != DRVUSB_CELLULAR) {
		return DRV_STATUS_INVALID_PARAM;
	}

	if (usbHostEc800Init() != DRV_STATUS_OK) {
		return DRV_STATUS_ERROR;
	}
	gBspUsbInitialized[usb] = 1U;
	return DRV_STATUS_OK;
}

eDrvStatus bspUsbStart(uint8_t usb)
{
	if ((usb >= DRVUSB_MAX) || (gBspUsbInitialized[usb] == 0U)) {
		return DRV_STATUS_NOT_READY;
	}

	return usbHostEc800Start();
}

eDrvStatus bspUsbStop(uint8_t usb)
{
	if ((usb >= DRVUSB_MAX) || (gBspUsbInitialized[usb] == 0U)) {
		return DRV_STATUS_NOT_READY;
	}

	return usbHostEc800Stop();
}

eDrvStatus bspUsbSetConnect(uint8_t usb, bool isConnect)
{
	(void)usb;
	(void)isConnect;
	return DRV_STATUS_OK;
}

eDrvStatus bspUsbOpenEndpoint(uint8_t usb, const stDrvUsbEndpointConfig *config)
{
	(void)usb;
	(void)config;
	return DRV_STATUS_OK;
}

eDrvStatus bspUsbCloseEndpoint(uint8_t usb, uint8_t endpointAddress)
{
	(void)usb;
	(void)endpointAddress;
	return DRV_STATUS_OK;
}

eDrvStatus bspUsbFlushEndpoint(uint8_t usb, uint8_t endpointAddress)
{
	(void)usb;
	(void)endpointAddress;
	return DRV_STATUS_OK;
}

eDrvStatus bspUsbTransmit(uint8_t usb, uint8_t endpointAddress, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
	if ((usb >= DRVUSB_MAX) || (gBspUsbInitialized[usb] == 0U)) {
		return DRV_STATUS_NOT_READY;
	}

	return usbHostEc800Transmit(endpointAddress, buffer, length, timeoutMs);
}

eDrvStatus bspUsbReceive(uint8_t usb, uint8_t endpointAddress, uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs)
{
	if (actualLength != NULL) {
		*actualLength = 0U;
	}
	if ((usb >= DRVUSB_MAX) || (gBspUsbInitialized[usb] == 0U)) {
		return DRV_STATUS_NOT_READY;
	}

	return usbHostEc800Receive(endpointAddress, buffer, length, actualLength, timeoutMs);
}

bool bspUsbIsConnected(uint8_t usb)
{
	if ((usb >= DRVUSB_MAX) || (gBspUsbInitialized[usb] == 0U)) {
		return false;
	}

	return usbHostEc800IsConnected();
}

bool bspUsbIsConfigured(uint8_t usb)
{
	if ((usb >= DRVUSB_MAX) || (gBspUsbInitialized[usb] == 0U)) {
		return false;
	}

	return usbHostEc800IsConfigured();
}

eDrvUsbSpeed bspUsbGetSpeed(uint8_t usb)
{
	if ((usb >= DRVUSB_MAX) || (gBspUsbInitialized[usb] == 0U)) {
		return DRVUSB_SPEED_UNKNOWN;
	}

	return usbHostEc800GetSpeed();
}

void bspUsbHandleIrq(void)
{
	usbHostEc800HandleIrq();
}

/**************************End of file********************************/
