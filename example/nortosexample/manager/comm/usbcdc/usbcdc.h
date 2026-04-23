/***********************************************************************************
* @file     : usbcdc.h
* @brief    : USB CDC communication manager declarations.
* @details  : Provides a simple line-based USB CDC command channel for bootloader
*             bring-up and field diagnostics.
* @author   : GitHub Copilot
* @date     : 2026-04-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_USBCDC_H
#define CPRSENSORBOOT_USBCDC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USBCDC_MANAGER_RX_CHUNK_SIZE       64U
#define USBCDC_MANAGER_USB_TIMEOUT_MS      10U
#define USBCDC_MANAGER_RECOVERY_DELAY_MS   5000U

typedef struct stUsbCdcManagerState {
    bool isInitialized;
    bool wasUsbConfigured;
    bool wasUsbSuspended;
    bool isUsbRecoveryPending;
    uint32_t usbRecoveryTick;
    uint16_t rxLength;
    uint8_t rxBuffer[USBCDC_MANAGER_RX_CHUNK_SIZE];
} stUsbCdcManagerState;

void usbCdcManagerReset(void);
bool usbCdcManagerInit(void);
void usbCdcManagerProcess(void);
bool usbCdcManagerWrite(const uint8_t *buffer, uint16_t length);
bool usbCdcManagerRead(uint8_t *buffer, uint16_t bufferSize, uint16_t *actualLength);
uint16_t usbCdcManagerGetRxLength(void);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_USBCDC_H
/**************************End of file********************************/
