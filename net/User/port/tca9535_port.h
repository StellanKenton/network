/************************************************************************************
* @file     : tca9535_port.h
* @brief    : TCA9535 project port interface.
***********************************************************************************/
#ifndef NETWORK_APP_PORT_TCA9535_PORT_H
#define NETWORK_APP_PORT_TCA9535_PORT_H

#include <stdbool.h>

#include "../../rep/rep.h"

#define TCA9535_USB_SEL1_MASK          0x0002U
#define TCA9535_USB_SEL2_MASK          0x0004U
#define TCA9535_USB_SEL3_MASK          0x0010U
#define TCA9535_CELLULAR_POWER_MASK    0x0100U
#define TCA9535_CELLULAR_MASK          (TCA9535_USB_SEL1_MASK | TCA9535_USB_SEL2_MASK | TCA9535_USB_SEL3_MASK | TCA9535_CELLULAR_POWER_MASK)

#ifdef __cplusplus
extern "C" {
#endif

eDrvStatus tca9535PortInit(void);
uint8_t tca9535PortGetAddress(void);
eDrvStatus tca9535PortRouteCellularUsb(void);
eDrvStatus tca9535PortSetCellularPower(bool enable);
eDrvStatus tca9535PortApplyCellularStartupOutput(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
