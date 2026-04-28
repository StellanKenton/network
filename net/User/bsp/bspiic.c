/***********************************************************************************
* @file     : bspiic.c
* @brief    : Board software IIC BSP implementation.
**********************************************************************************/
#include "bspiic.h"

#include <stdbool.h>
#include <stddef.h>

#include "../../SYSTEM/sys/sys.h"
#include "../port/drviic_port.h"

#define BSPIIC_SCL_PORT             GPIOD
#define BSPIIC_SCL_PIN              GPIO_Pin_11
#define BSPIIC_SDA_PORT             GPIOD
#define BSPIIC_SDA_PIN              GPIO_Pin_12
#define BSPIIC_INT_PIN              GPIO_Pin_13
#define BSPIIC_DELAY_LOOP_COUNT     160U

static void bspIicDelay(void)
{
    volatile uint32_t lLoop;

    for (lLoop = 0U; lLoop < BSPIIC_DELAY_LOOP_COUNT; lLoop++) {
    }
}

static void bspIicWriteScl(bool level)
{
    if (level) {
        GPIO_SetBits(BSPIIC_SCL_PORT, BSPIIC_SCL_PIN);
    } else {
        GPIO_ResetBits(BSPIIC_SCL_PORT, BSPIIC_SCL_PIN);
    }
}

static void bspIicWriteSda(bool level)
{
    if (level) {
        GPIO_SetBits(BSPIIC_SDA_PORT, BSPIIC_SDA_PIN);
    } else {
        GPIO_ResetBits(BSPIIC_SDA_PORT, BSPIIC_SDA_PIN);
    }
}

static bool bspIicReadSda(void)
{
    return GPIO_ReadInputDataBit(BSPIIC_SDA_PORT, BSPIIC_SDA_PIN) != Bit_RESET;
}

static void bspIicStart(void)
{
    bspIicWriteSda(true);
    bspIicWriteScl(true);
    bspIicDelay();
    bspIicWriteSda(false);
    bspIicDelay();
    bspIicWriteScl(false);
}

static void bspIicStop(void)
{
    bspIicWriteSda(false);
    bspIicDelay();
    bspIicWriteScl(true);
    bspIicDelay();
    bspIicWriteSda(true);
    bspIicDelay();
}

static eDrvStatus bspIicWriteByte(uint8_t data)
{
    uint8_t lMask;
    bool lAck;

    for (lMask = 0x80U; lMask != 0U; lMask >>= 1U) {
        bspIicWriteSda((data & lMask) != 0U);
        bspIicDelay();
        bspIicWriteScl(true);
        bspIicDelay();
        bspIicWriteScl(false);
    }

    bspIicWriteSda(true);
    bspIicDelay();
    bspIicWriteScl(true);
    bspIicDelay();
    lAck = !bspIicReadSda();
    bspIicWriteScl(false);
    return lAck ? DRV_STATUS_OK : DRV_STATUS_NACK;
}

static uint8_t bspIicReadByte(bool ack)
{
    uint8_t lMask;
    uint8_t lData = 0U;

    bspIicWriteSda(true);
    for (lMask = 0x80U; lMask != 0U; lMask >>= 1U) {
        bspIicDelay();
        bspIicWriteScl(true);
        bspIicDelay();
        if (bspIicReadSda()) {
            lData |= lMask;
        }
        bspIicWriteScl(false);
    }

    bspIicWriteSda(!ack);
    bspIicDelay();
    bspIicWriteScl(true);
    bspIicDelay();
    bspIicWriteScl(false);
    bspIicWriteSda(true);
    return lData;
}

static eDrvStatus bspIicWriteBuffer(const uint8_t *buffer, uint16_t length)
{
    uint16_t lIndex;
    eDrvStatus lStatus;

    for (lIndex = 0U; lIndex < length; lIndex++) {
        lStatus = bspIicWriteByte(buffer[lIndex]);
        if (lStatus != DRV_STATUS_OK) {
            return lStatus;
        }
    }

    return DRV_STATUS_OK;
}

eDrvStatus bspIicInit(uint8_t iic)
{
    GPIO_InitTypeDef lGpioInit;

    if (iic != DRVIIC_TCA9535) {
        return DRV_STATUS_INVALID_PARAM;
    }

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    GPIO_StructInit(&lGpioInit);
    lGpioInit.GPIO_Pin = BSPIIC_SCL_PIN | BSPIIC_SDA_PIN;
    lGpioInit.GPIO_Mode = GPIO_Mode_OUT;
    lGpioInit.GPIO_OType = GPIO_OType_OD;
    lGpioInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
    lGpioInit.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOD, &lGpioInit);

    GPIO_StructInit(&lGpioInit);
    lGpioInit.GPIO_Pin = BSPIIC_INT_PIN;
    lGpioInit.GPIO_Mode = GPIO_Mode_IN;
    lGpioInit.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOD, &lGpioInit);

    bspIicWriteSda(true);
    bspIicWriteScl(true);
    return bspIicRecoverBus(iic);
}

eDrvStatus bspIicTransfer(uint8_t iic, const stDrvIicTransfer *transfer, uint32_t timeoutMs)
{
    eDrvStatus lStatus;
    uint16_t lIndex;

    (void)timeoutMs;

    if ((iic != DRVIIC_TCA9535) || (transfer == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    bspIicStart();
    lStatus = bspIicWriteByte((uint8_t)(transfer->address << 1U));
    if (lStatus == DRV_STATUS_OK) {
        lStatus = bspIicWriteBuffer(transfer->writeBuffer, transfer->writeLength);
    }
    if (lStatus == DRV_STATUS_OK) {
        lStatus = bspIicWriteBuffer(transfer->secondWriteBuffer, transfer->secondWriteLength);
    }
    if ((lStatus == DRV_STATUS_OK) && (transfer->readLength > 0U)) {
        bspIicStart();
        lStatus = bspIicWriteByte((uint8_t)((transfer->address << 1U) | 0x01U));
        for (lIndex = 0U; (lStatus == DRV_STATUS_OK) && (lIndex < transfer->readLength); lIndex++) {
            transfer->readBuffer[lIndex] = bspIicReadByte((uint8_t)(lIndex + 1U) < transfer->readLength);
        }
    }

    bspIicStop();
    return lStatus;
}

eDrvStatus bspIicRecoverBus(uint8_t iic)
{
    uint8_t lIndex;

    if (iic != DRVIIC_TCA9535) {
        return DRV_STATUS_INVALID_PARAM;
    }

    bspIicWriteSda(true);
    for (lIndex = 0U; lIndex < 9U; lIndex++) {
        bspIicWriteScl(true);
        bspIicDelay();
        bspIicWriteScl(false);
        bspIicDelay();
    }
    bspIicStop();
    return DRV_STATUS_OK;
}

/**************************End of file********************************/
