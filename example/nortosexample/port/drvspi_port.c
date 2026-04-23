/***********************************************************************************
* @file     : drvspi_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvspi_port.h"

#include "drvspi.h"

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_spi.h"

#include "../system/system.h"

static eDrvStatus bspSpiInit(uint8_t spi);
static eDrvStatus bspSpiTransfer(uint8_t spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint8_t fillData, uint32_t timeoutMs);
static void bspSpiCsInit(void *context);
static void bspSpiCsWrite(void *context, bool isActive);
static bool drvSpiPortWaitFlag(uint16_t flag, FlagStatus state, uint32_t timeoutMs);

stDrvSpiBspInterface gDrvSpiBspInterface[DRVSPI_MAX] = {
    [DRVSPI_BUS0] = {
        .init = bspSpiInit,
        .transfer = bspSpiTransfer,
        .defaultTimeoutMs = DRVSPI_DEFAULT_TIMEOUT_MS,
        .csControl = {
            .init = bspSpiCsInit,
            .write = bspSpiCsWrite,
            .context = NULL,
        },
    },
};

static eDrvStatus bspSpiInit(uint8_t spi)
{
    GPIO_InitTypeDef lGpioInit;
    SPI_InitTypeDef lSpiInit;

    if (spi != DRVSPI_BUS0) {
        return DRV_STATUS_INVALID_PARAM;
    }

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    GPIO_StructInit(&lGpioInit);
    lGpioInit.GPIO_Speed = GPIO_Speed_50MHz;

    lGpioInit.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    lGpioInit.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &lGpioInit);

    lGpioInit.GPIO_Pin = GPIO_Pin_6;
    lGpioInit.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &lGpioInit);

    bspSpiCsInit(NULL);

    SPI_I2S_DeInit(SPI1);
    SPI_StructInit(&lSpiInit);
    lSpiInit.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    lSpiInit.SPI_Mode = SPI_Mode_Master;
    lSpiInit.SPI_DataSize = SPI_DataSize_8b;
    lSpiInit.SPI_CPOL = SPI_CPOL_Low;
    lSpiInit.SPI_CPHA = SPI_CPHA_1Edge;
    lSpiInit.SPI_NSS = SPI_NSS_Soft;
    lSpiInit.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    lSpiInit.SPI_FirstBit = SPI_FirstBit_MSB;
    lSpiInit.SPI_CRCPolynomial = 7U;
    SPI_Init(SPI1, &lSpiInit);
    SPI_Cmd(SPI1, ENABLE);
    return DRV_STATUS_OK;
}

static eDrvStatus bspSpiTransfer(uint8_t spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint8_t fillData, uint32_t timeoutMs)
{
    uint16_t lIndex;

    if (spi != DRVSPI_BUS0) {
        return DRV_STATUS_INVALID_PARAM;
    }

    for (lIndex = 0U; lIndex < length; ++lIndex) {
        uint8_t lWriteValue;

        if (!drvSpiPortWaitFlag(SPI_I2S_FLAG_TXE, SET, timeoutMs)) {
            return DRV_STATUS_TIMEOUT;
        }

        lWriteValue = (txBuffer != NULL) ? txBuffer[lIndex] : fillData;
        SPI_I2S_SendData(SPI1, lWriteValue);

        if (!drvSpiPortWaitFlag(SPI_I2S_FLAG_RXNE, SET, timeoutMs)) {
            return DRV_STATUS_TIMEOUT;
        }

        lWriteValue = (uint8_t)SPI_I2S_ReceiveData(SPI1);
        if (rxBuffer != NULL) {
            rxBuffer[lIndex] = lWriteValue;
        }
    }

    if (!drvSpiPortWaitFlag(SPI_I2S_FLAG_BSY, RESET, timeoutMs)) {
        return DRV_STATUS_TIMEOUT;
    }

    return DRV_STATUS_OK;
}

static void bspSpiCsInit(void *context)
{
    GPIO_InitTypeDef lGpioInit;

    (void)context;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_StructInit(&lGpioInit);
    lGpioInit.GPIO_Pin = GPIO_Pin_4;
    lGpioInit.GPIO_Speed = GPIO_Speed_50MHz;
    lGpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &lGpioInit);
    GPIO_SetBits(GPIOA, GPIO_Pin_4);
}

static void bspSpiCsWrite(void *context, bool isActive)
{
    (void)context;
    GPIO_WriteBit(GPIOA, GPIO_Pin_4, isActive ? Bit_RESET : Bit_SET);
}

static bool drvSpiPortWaitFlag(uint16_t flag, FlagStatus state, uint32_t timeoutMs)
{
    uint32_t lStartTick;

    lStartTick = systemGetTickMs();
    while (SPI_I2S_GetFlagStatus(SPI1, flag) != state) {
        if ((timeoutMs > 0U) && ((systemGetTickMs() - lStartTick) >= timeoutMs)) {
            return false;
        }
    }

    return true;
}

const stDrvSpiBspInterface *drvSpiGetPlatformBspInterfaces(void)
{
    return gDrvSpiBspInterface;
}

/**************************End of file********************************/
