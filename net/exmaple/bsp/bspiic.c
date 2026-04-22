/***********************************************************************************
* @file     : bspiic.c
* @brief    : Board-level hardware IIC BSP implementation.
* @details  : Maps logical IIC buses to the concrete STM32 HAL I2C instances.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bspiic.h"

#include <stddef.h>
#include <string.h>

#include "i2c.h"

#include "../port/drviic_port.h"

static eDrvStatus bspIicStatusFromHal(HAL_StatusTypeDef halStatus)
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

static I2C_HandleTypeDef *bspIicGetHandle(uint8_t iic)
{
    switch ((eDrvIicPortMap)iic) {
        case DRVIIC_BUS0:
            return &hi2c1;
        case DRVIIC_BUS1:
            return &hi2c2;
        default:
            return NULL;
    }
}

eDrvStatus bspIicInit(uint8_t iic)
{
    I2C_HandleTypeDef *handle = bspIicGetHandle(iic);

    if ((handle == NULL) || (handle->Instance == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    return DRV_STATUS_OK;
}

eDrvStatus bspIicRecoverBus(uint8_t iic)
{
    I2C_HandleTypeDef *handle = bspIicGetHandle(iic);
    HAL_StatusTypeDef halStatus;

    if ((handle == NULL) || (handle->Instance == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    halStatus = HAL_I2C_DeInit(handle);
    if (halStatus != HAL_OK) {
        return bspIicStatusFromHal(halStatus);
    }

    halStatus = HAL_I2C_Init(handle);
    return bspIicStatusFromHal(halStatus);
}

eDrvStatus bspIicTransfer(uint8_t iic, const stDrvIicTransfer *transfer, uint32_t timeoutMs)
{
    I2C_HandleTypeDef *handle = bspIicGetHandle(iic);
    uint16_t address;
    HAL_StatusTypeDef halStatus;

    if ((handle == NULL) || (transfer == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    address = (uint16_t)(transfer->address << 1U);

    if ((transfer->readLength > 0U) && (transfer->writeLength > 0U) && (transfer->secondWriteLength == 0U)) {
        uint16_t memAddSize = I2C_MEMADD_SIZE_8BIT;
        uint16_t memAddress = transfer->writeBuffer[0];

        if ((transfer->writeBuffer == NULL) || (transfer->writeLength > 2U)) {
            return DRV_STATUS_INVALID_PARAM;
        }

        if (transfer->writeLength == 2U) {
            memAddSize = I2C_MEMADD_SIZE_16BIT;
            memAddress = (uint16_t)(((uint16_t)transfer->writeBuffer[0] << 8U) | transfer->writeBuffer[1]);
        }

        halStatus = HAL_I2C_Mem_Read(handle,
                                     address,
                                     memAddress,
                                     memAddSize,
                                     transfer->readBuffer,
                                     transfer->readLength,
                                     timeoutMs);
        return bspIicStatusFromHal(halStatus);
    }

    if (transfer->secondWriteLength > 0U) {
        uint8_t buffer[64];
        uint16_t totalLength = (uint16_t)(transfer->writeLength + transfer->secondWriteLength);

        if ((transfer->writeBuffer == NULL) || (transfer->secondWriteBuffer == NULL) || (totalLength > sizeof(buffer))) {
            return DRV_STATUS_INVALID_PARAM;
        }

        (void)memcpy(buffer, transfer->writeBuffer, transfer->writeLength);
        (void)memcpy(&buffer[transfer->writeLength], transfer->secondWriteBuffer, transfer->secondWriteLength);
        halStatus = HAL_I2C_Master_Transmit(handle, address, buffer, totalLength, timeoutMs);
        return bspIicStatusFromHal(halStatus);
    }

    if (transfer->writeLength > 0U) {
        halStatus = HAL_I2C_Master_Transmit(handle,
                                            address,
                                            (uint8_t *)transfer->writeBuffer,
                                            transfer->writeLength,
                                            timeoutMs);
        if ((halStatus != HAL_OK) || (transfer->readLength == 0U)) {
            return bspIicStatusFromHal(halStatus);
        }
    }

    if (transfer->readLength > 0U) {
        halStatus = HAL_I2C_Master_Receive(handle,
                                           address,
                                           transfer->readBuffer,
                                           transfer->readLength,
                                           timeoutMs);
        return bspIicStatusFromHal(halStatus);
    }

    return DRV_STATUS_OK;
}
/**************************End of file********************************/
