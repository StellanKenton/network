/***********************************************************************************
* @file     : bspspi.c
* @brief    : Board-level SPI BSP implementation.
* @details  : Maps logical SPI buses and chip-select lines to STM32 HAL resources.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bspspi.h"

#include <stddef.h>
#include <string.h>

#include "../rep_config.h"
#include "drvgpio.h"
#include "spi.h"

#include "../port/drvgpio_port.h"
#include "../port/drvspi_port.h"

const stBspSpiCsPin gBspSpiBus0CsPin = {
    .pin = DRVGPIO_SPI_CS,
    .isActiveLow = true,
};

static eDrvStatus bspSpiStatusFromHal(HAL_StatusTypeDef halStatus)
{
    switch (halStatus) {
        case HAL_OK:
            return DRV_STATUS_OK;
        case HAL_BUSY:
            return DRV_STATUS_BUSY;
        case HAL_TIMEOUT:
            return DRV_STATUS_TIMEOUT;
        default:
            return DRV_STATUS_ERROR;
    }
}

static SPI_HandleTypeDef *bspSpiGetHandle(uint8_t spi)
{
    if (spi != DRVSPI_BUS0) {
        return NULL;
    }

    return &hspi1;
}

eDrvStatus bspSpiInit(uint8_t spi)
{
    SPI_HandleTypeDef *handle = bspSpiGetHandle(spi);

    if ((handle == NULL) || (handle->Instance == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    return DRV_STATUS_OK;
}

eDrvStatus bspSpiTransfer(uint8_t spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint8_t fillData, uint32_t timeoutMs)
{
    SPI_HandleTypeDef *handle = bspSpiGetHandle(spi);
    HAL_StatusTypeDef halStatus;
    uint8_t fillBuffer[32];
    uint16_t chunkLength;
    uint16_t offset;

    if ((handle == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if ((txBuffer != NULL) && (rxBuffer != NULL)) {
        halStatus = HAL_SPI_TransmitReceive(handle, (uint8_t *)txBuffer, rxBuffer, length, timeoutMs);
        return bspSpiStatusFromHal(halStatus);
    }

    if (txBuffer != NULL) {
        halStatus = HAL_SPI_Transmit(handle, (uint8_t *)txBuffer, length, timeoutMs);
        return bspSpiStatusFromHal(halStatus);
    }

    if (rxBuffer == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    (void)memset(fillBuffer, fillData, sizeof(fillBuffer));
    offset = 0U;
    while (offset < length) {
        chunkLength = (uint16_t)(length - offset);
        if (chunkLength > sizeof(fillBuffer)) {
            chunkLength = sizeof(fillBuffer);
        }

        halStatus = HAL_SPI_TransmitReceive(handle,
                                            fillBuffer,
                                            &rxBuffer[offset],
                                            chunkLength,
                                            timeoutMs);
        if (halStatus != HAL_OK) {
            return bspSpiStatusFromHal(halStatus);
        }

        offset = (uint16_t)(offset + chunkLength);
    }

    return DRV_STATUS_OK;
}

void bspSpiCsInit(void *context)
{
    stBspSpiCsPin *pin = (stBspSpiCsPin *)context;

    if (pin == NULL) {
        return;
    }

    drvGpioWrite(pin->pin, pin->isActiveLow ? DRVGPIO_PIN_SET : DRVGPIO_PIN_RESET);
}

void bspSpiCsWrite(void *context, bool isActive)
{
    stBspSpiCsPin *pin = (stBspSpiCsPin *)context;
    eDrvGpioPinState lState;

    if (pin == NULL) {
        return;
    }

    lState = (isActive == pin->isActiveLow) ? DRVGPIO_PIN_RESET : DRVGPIO_PIN_SET;
    drvGpioWrite(pin->pin, lState);
}
/**************************End of file********************************/
