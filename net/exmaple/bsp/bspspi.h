/************************************************************************************
* @file     : bspspi.h
* @brief    : Board-level SPI BSP declarations.
* @details  : Provides STM32 HAL-backed SPI hooks and board chip-select resources.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_BSPSPI_H
#define REBUILDCPR_BSPSPI_H

#include <stdbool.h>
#include <stdint.h>

#include "drvspi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stBspSpiCsPin {
    uint8_t pin;
    bool isActiveLow;
} stBspSpiCsPin;

extern const stBspSpiCsPin gBspSpiBus0CsPin;

eDrvStatus bspSpiInit(uint8_t spi);
eDrvStatus bspSpiTransfer(uint8_t spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint8_t fillData, uint32_t timeoutMs);
void bspSpiCsInit(void *context);
void bspSpiCsWrite(void *context, bool isActive);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
