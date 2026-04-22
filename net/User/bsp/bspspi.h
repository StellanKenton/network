/************************************************************************************
* @file     : bspspi.h
* @brief    : Board SPI BSP declarations.
***********************************************************************************/
#ifndef NETWORK_APP_BSP_SPI_H
#define NETWORK_APP_BSP_SPI_H

#include <stdint.h>

#include "../../rep/driver/drvspi/drvspi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stBspSpiCsPin {
	uint8_t pin;
	uint8_t isActiveLow;
} stBspSpiCsPin;

extern const stBspSpiCsPin gBspSpiFlashCsPin;

eDrvStatus bspSpiInit(uint8_t spi);
eDrvStatus bspSpiTransfer(uint8_t spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint8_t fillData, uint32_t timeoutMs);
void bspSpiCsInit(void *context);
void bspSpiCsWrite(void *context, bool isActive);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/