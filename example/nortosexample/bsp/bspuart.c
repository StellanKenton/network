/***********************************************************************************
* @file     : bspuart.c
* @brief    : Board-specific UART binding for drvuart.
* @details  : Configures USART1 DMA transport and UART4 WiFi transport.
* @author   : GitHub Copilot
* @date     : 2026-04-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bspuart.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "misc.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"

#include "../system/system.h"

uint8_t gBspUartRxStorageUart1[DRVUART_RECVLEN_UART1];
uint8_t gBspUartRxStorageWifi[DRVUART_RECVLEN_WIFI];

struct stBspUartConfig {
    USART_TypeDef *uart;
    DMA_Channel_TypeDef *txDmaChannel;
    DMA_Channel_TypeDef *rxDmaChannel;
    uint32_t uartClock;
    bool isUartClockOnApb2;
    uint32_t gpioClock;
    uint32_t dmaClock;
    GPIO_TypeDef *txPort;
    uint16_t txPin;
    GPIO_TypeDef *rxPort;
    uint16_t rxPin;
    IRQn_Type txDmaIrq;
    IRQn_Type rxDmaIrq;
    IRQn_Type uartIrq;
    bool useTxDma;
    bool useRxDma;
    uint32_t baudRate;
    uint8_t *rxStorage;
    uint16_t rxCapacity;
};

struct stBspUartState {
    volatile bool isInitialized;
    volatile bool isTxDmaBusy;
    volatile uint16_t rxReadIndex;
    volatile uint16_t rxWriteIndex;
    uint8_t txStorage[BSPUART_TX_DMA_BUFFER_SIZE];
};

static const struct stBspUartConfig gBspUartConfig[DRVUART_MAX] = {
    [DRVUART_UART1] = {
        .uart = USART1,
        .txDmaChannel = DMA1_Channel4,
        .rxDmaChannel = DMA1_Channel5,
        .uartClock = RCC_APB2Periph_USART1,
        .isUartClockOnApb2 = true,
        .gpioClock = RCC_APB2Periph_GPIOA,
        .dmaClock = RCC_AHBPeriph_DMA1,
        .txPort = GPIOA,
        .txPin = GPIO_Pin_9,
        .rxPort = GPIOA,
        .rxPin = GPIO_Pin_10,
        .txDmaIrq = DMA1_Channel4_IRQn,
        .rxDmaIrq = DMA1_Channel5_IRQn,
        .uartIrq = DMA1_Channel4_IRQn,
        .useTxDma = true,
        .useRxDma = true,
        .baudRate = BSPUART_UART1_BAUDRATE,
        .rxStorage = gBspUartRxStorageUart1,
        .rxCapacity = (uint16_t)sizeof(gBspUartRxStorageUart1),
    },
    [DRVUART_WIFI] = {
        .uart = UART4,
        .txDmaChannel = NULL,
        .rxDmaChannel = NULL,
        .uartClock = RCC_APB1Periph_UART4,
        .isUartClockOnApb2 = false,
        .gpioClock = RCC_APB2Periph_GPIOC,
        .dmaClock = 0U,
        .txPort = GPIOC,
        .txPin = GPIO_Pin_10,
        .rxPort = GPIOC,
        .rxPin = GPIO_Pin_11,
        .txDmaIrq = DMA1_Channel4_IRQn,
        .rxDmaIrq = DMA1_Channel5_IRQn,
        .uartIrq = UART4_IRQn,
        .useTxDma = false,
        .useRxDma = false,
        .baudRate = BSPUART_WIFI_BAUDRATE,
        .rxStorage = gBspUartRxStorageWifi,
        .rxCapacity = (uint16_t)sizeof(gBspUartRxStorageWifi),
    },
};

static struct stBspUartState gBspUartState[DRVUART_MAX];

static const struct stBspUartConfig *bspUartGetConfig(uint8_t uart)
{
    if (uart >= DRVUART_MAX) {
        return NULL;
    }

    if ((gBspUartConfig[uart].uart == NULL) ||
        (gBspUartConfig[uart].rxStorage == NULL) ||
        (gBspUartConfig[uart].rxCapacity == 0U)) {
        return NULL;
    }

    if (gBspUartConfig[uart].useTxDma && (gBspUartConfig[uart].txDmaChannel == NULL)) {
        return NULL;
    }

    if (gBspUartConfig[uart].useRxDma && (gBspUartConfig[uart].rxDmaChannel == NULL)) {
        return NULL;
    }

    return &gBspUartConfig[uart];
}

static struct stBspUartState *bspUartGetState(uint8_t uart)
{
    if (uart >= DRVUART_MAX) {
        return NULL;
    }

    return &gBspUartState[uart];
}

static void bspUartConfigureGpio(const struct stBspUartConfig *config)
{
    GPIO_InitTypeDef lGpioInit;

    GPIO_StructInit(&lGpioInit);
    lGpioInit.GPIO_Speed = GPIO_Speed_50MHz;

    lGpioInit.GPIO_Pin = config->txPin;
    lGpioInit.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(config->txPort, &lGpioInit);

    lGpioInit.GPIO_Pin = config->rxPin;
    lGpioInit.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(config->rxPort, &lGpioInit);
}

static void bspUartConfigureInstance(const struct stBspUartConfig *config)
{
    USART_InitTypeDef lUartInit;

    USART_StructInit(&lUartInit);
    lUartInit.USART_BaudRate = config->baudRate;
    lUartInit.USART_WordLength = USART_WordLength_8b;
    lUartInit.USART_StopBits = USART_StopBits_1;
    lUartInit.USART_Parity = USART_Parity_No;
    lUartInit.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    lUartInit.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_DeInit(config->uart);
    USART_Init(config->uart, &lUartInit);
    USART_Cmd(config->uart, ENABLE);
}

static void bspUartConfigureIrq(const struct stBspUartConfig *config)
{
    NVIC_InitTypeDef lNvicInit;

    USART_ITConfig(config->uart, USART_IT_RXNE, ENABLE);

    lNvicInit.NVIC_IRQChannel = config->uartIrq;
    lNvicInit.NVIC_IRQChannelPreemptionPriority = 1U;
    lNvicInit.NVIC_IRQChannelSubPriority = 2U;
    lNvicInit.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&lNvicInit);
}

static void bspUartConfigureRxDma(const struct stBspUartConfig *config)
{
    DMA_InitTypeDef lDmaInit;

    DMA_DeInit(config->rxDmaChannel);
    DMA_StructInit(&lDmaInit);
    lDmaInit.DMA_PeripheralBaseAddr = (uint32_t)&config->uart->DR;
    lDmaInit.DMA_MemoryBaseAddr = (uint32_t)config->rxStorage;
    lDmaInit.DMA_DIR = DMA_DIR_PeripheralSRC;
    lDmaInit.DMA_BufferSize = config->rxCapacity;
    lDmaInit.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    lDmaInit.DMA_MemoryInc = DMA_MemoryInc_Enable;
    lDmaInit.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    lDmaInit.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    lDmaInit.DMA_Mode = DMA_Mode_Circular;
    lDmaInit.DMA_Priority = DMA_Priority_VeryHigh;
    lDmaInit.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(config->rxDmaChannel, &lDmaInit);
    DMA_ClearFlag(DMA1_FLAG_GL5);
    DMA_Cmd(config->rxDmaChannel, ENABLE);
    USART_DMACmd(config->uart, USART_DMAReq_Rx, ENABLE);
}

static void bspUartConfigureTxDma(const struct stBspUartConfig *config)
{
    DMA_InitTypeDef lDmaInit;
    NVIC_InitTypeDef lNvicInit;

    DMA_DeInit(config->txDmaChannel);
    DMA_StructInit(&lDmaInit);
    lDmaInit.DMA_PeripheralBaseAddr = (uint32_t)&config->uart->DR;
    lDmaInit.DMA_MemoryBaseAddr = 0U;
    lDmaInit.DMA_DIR = DMA_DIR_PeripheralDST;
    lDmaInit.DMA_BufferSize = 0U;
    lDmaInit.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    lDmaInit.DMA_MemoryInc = DMA_MemoryInc_Enable;
    lDmaInit.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    lDmaInit.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    lDmaInit.DMA_Mode = DMA_Mode_Normal;
    lDmaInit.DMA_Priority = DMA_Priority_High;
    lDmaInit.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(config->txDmaChannel, &lDmaInit);
    DMA_ITConfig(config->txDmaChannel, DMA_IT_TC | DMA_IT_TE, ENABLE);
    DMA_ClearFlag(DMA1_FLAG_GL4);

    lNvicInit.NVIC_IRQChannel = config->txDmaIrq;
    lNvicInit.NVIC_IRQChannelPreemptionPriority = 1U;
    lNvicInit.NVIC_IRQChannelSubPriority = 0U;
    lNvicInit.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&lNvicInit);

    lNvicInit.NVIC_IRQChannel = config->rxDmaIrq;
    lNvicInit.NVIC_IRQChannelPreemptionPriority = 1U;
    lNvicInit.NVIC_IRQChannelSubPriority = 1U;
    lNvicInit.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&lNvicInit);
}

static void bspUartStopTxDma(const struct stBspUartConfig *config, struct stBspUartState *state)
{
    USART_DMACmd(config->uart, USART_DMAReq_Tx, DISABLE);
    DMA_Cmd(config->txDmaChannel, DISABLE);
    while ((config->txDmaChannel->CCR & DMA_CCR4_EN) != 0U) {
    }
    DMA_ClearFlag(DMA1_FLAG_GL4);
    state->isTxDmaBusy = false;
}

static uint16_t bspUartGetRxWriteIndex(const struct stBspUartConfig *config)
{
    uint16_t lWriteIndex;

    if (!config->useRxDma) {
        struct stBspUartState *lState;

        lState = bspUartGetState((uint8_t)(config - gBspUartConfig));
        if (lState == NULL) {
            return 0U;
        }

        return lState->rxWriteIndex;
    }

    lWriteIndex = (uint16_t)(config->rxCapacity - DMA_GetCurrDataCounter(config->rxDmaChannel));
    if (lWriteIndex >= config->rxCapacity) {
        lWriteIndex = 0U;
    }

    return lWriteIndex;
}

static uint16_t bspUartGetPendingLength(const struct stBspUartConfig *config, const struct stBspUartState *state)
{
    uint16_t lWriteIndex;

    lWriteIndex = bspUartGetRxWriteIndex(config);
    if (lWriteIndex >= state->rxReadIndex) {
        return (uint16_t)(lWriteIndex - state->rxReadIndex);
    }

    return (uint16_t)(config->rxCapacity - state->rxReadIndex + lWriteIndex);
}

static eDrvStatus bspUartStartTxDma(uint8_t uart, const uint8_t *buffer, uint16_t length)
{
    const struct stBspUartConfig *lConfig;
    struct stBspUartState *lState;

    lConfig = bspUartGetConfig(uart);
    lState = bspUartGetState(uart);
    if ((lConfig == NULL) || (lState == NULL) || !lState->isInitialized) {
        return DRV_STATUS_NOT_READY;
    }

    if (!lConfig->useTxDma) {
        return DRV_STATUS_UNSUPPORTED;
    }

    if ((buffer == NULL) || (length == 0U) || (length > BSPUART_TX_DMA_BUFFER_SIZE)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (lState->isTxDmaBusy) {
        return DRV_STATUS_BUSY;
    }

    (void)memcpy(lState->txStorage, buffer, length);

    DMA_ClearFlag(DMA1_FLAG_GL4);
    DMA_Cmd(lConfig->txDmaChannel, DISABLE);
    while ((lConfig->txDmaChannel->CCR & DMA_CCR4_EN) != 0U) {
    }
    lConfig->txDmaChannel->CMAR = (uint32_t)lState->txStorage;
    lConfig->txDmaChannel->CNDTR = length;
    USART_ClearFlag(lConfig->uart, USART_FLAG_TC);
    lState->isTxDmaBusy = true;
    USART_DMACmd(lConfig->uart, USART_DMAReq_Tx, ENABLE);
    DMA_Cmd(lConfig->txDmaChannel, ENABLE);

    return DRV_STATUS_OK;
}

static eDrvStatus bspUartTransmitBlocking(const struct stBspUartConfig *config, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    uint16_t lIndex;
    uint32_t lStartTick;

    if ((buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lStartTick = systemGetTickMs();
    for (lIndex = 0U; lIndex < length; lIndex++) {
        while (USART_GetFlagStatus(config->uart, USART_FLAG_TXE) == RESET) {
            if ((timeoutMs > 0U) && ((systemGetTickMs() - lStartTick) >= timeoutMs)) {
                return DRV_STATUS_TIMEOUT;
            }
        }

        USART_SendData(config->uart, buffer[lIndex]);
    }

    while (USART_GetFlagStatus(config->uart, USART_FLAG_TC) == RESET) {
        if ((timeoutMs > 0U) && ((systemGetTickMs() - lStartTick) >= timeoutMs)) {
            return DRV_STATUS_TIMEOUT;
        }
    }

    return DRV_STATUS_OK;
}

eDrvStatus bspUartInit(uint8_t uart)
{
    const struct stBspUartConfig *lConfig;
    struct stBspUartState *lState;

    lConfig = bspUartGetConfig(uart);
    lState = bspUartGetState(uart);
    if ((lConfig == NULL) || (lState == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | lConfig->gpioClock, ENABLE);
    if (lConfig->isUartClockOnApb2) {
        RCC_APB2PeriphClockCmd(lConfig->uartClock, ENABLE);
    } else {
        RCC_APB1PeriphClockCmd(lConfig->uartClock, ENABLE);
    }
    if (lConfig->dmaClock != 0U) {
        RCC_AHBPeriphClockCmd(lConfig->dmaClock, ENABLE);
    }

    (void)memset(lState, 0, sizeof(*lState));
    (void)memset(lConfig->rxStorage, 0, lConfig->rxCapacity);

    bspUartConfigureGpio(lConfig);
    bspUartConfigureInstance(lConfig);
    if (lConfig->useTxDma) {
        bspUartConfigureTxDma(lConfig);
    }
    if (lConfig->useRxDma) {
        bspUartConfigureRxDma(lConfig);
    } else {
        bspUartConfigureIrq(lConfig);
    }

    lState->isInitialized = true;
    return DRV_STATUS_OK;
}

eDrvStatus bspUartTransmit(uint8_t uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    const struct stBspUartConfig *lConfig;
    struct stBspUartState *lState;
    uint32_t lStartTick;
    eDrvStatus lStatus;

    lConfig = bspUartGetConfig(uart);
    lState = bspUartGetState(uart);
    if ((lConfig == NULL) || (lState == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!lConfig->useTxDma) {
        return bspUartTransmitBlocking(lConfig, buffer, length, timeoutMs);
    }

    lStatus = bspUartStartTxDma(uart, buffer, length);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStartTick = systemGetTickMs();
    while (lState->isTxDmaBusy) {
        if ((timeoutMs > 0U) && ((systemGetTickMs() - lStartTick) >= timeoutMs)) {
            bspUartStopTxDma(lConfig, lState);
            return DRV_STATUS_TIMEOUT;
        }
    }

    while (USART_GetFlagStatus(lConfig->uart, USART_FLAG_TC) == RESET) {
        if ((timeoutMs > 0U) && ((systemGetTickMs() - lStartTick) >= timeoutMs)) {
            bspUartStopTxDma(lConfig, lState);
            return DRV_STATUS_TIMEOUT;
        }
    }

    return DRV_STATUS_OK;
}

eDrvStatus bspUartTransmitIt(uint8_t uart, const uint8_t *buffer, uint16_t length)
{
    (void)uart;
    (void)buffer;
    (void)length;

    return DRV_STATUS_UNSUPPORTED;
}

eDrvStatus bspUartTransmitDma(uint8_t uart, const uint8_t *buffer, uint16_t length)
{
    return bspUartStartTxDma(uart, buffer, length);
}

uint16_t bspUartGetDataLen(uint8_t uart)
{
    const struct stBspUartConfig *lConfig;
    struct stBspUartState *lState;

    lConfig = bspUartGetConfig(uart);
    lState = bspUartGetState(uart);
    if ((lConfig == NULL) || (lState == NULL) || !lState->isInitialized) {
        return 0U;
    }

    return bspUartGetPendingLength(lConfig, lState);
}

eDrvStatus bspUartReceive(uint8_t uart, uint8_t *buffer, uint16_t length)
{
    const struct stBspUartConfig *lConfig;
    struct stBspUartState *lState;
    uint16_t lFirstCopyLength;

    lConfig = bspUartGetConfig(uart);
    lState = bspUartGetState(uart);
    if ((lConfig == NULL) || (lState == NULL) || !lState->isInitialized) {
        return DRV_STATUS_NOT_READY;
    }

    if ((buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (length > bspUartGetPendingLength(lConfig, lState)) {
        return DRV_STATUS_NOT_READY;
    }

    lFirstCopyLength = (uint16_t)(lConfig->rxCapacity - lState->rxReadIndex);
    if (lFirstCopyLength > length) {
        lFirstCopyLength = length;
    }

    (void)memcpy(buffer, &lConfig->rxStorage[lState->rxReadIndex], lFirstCopyLength);
    if (length > lFirstCopyLength) {
        (void)memcpy(&buffer[lFirstCopyLength],
                     lConfig->rxStorage,
                     (size_t)(length - lFirstCopyLength));
    }

    lState->rxReadIndex = (uint16_t)((lState->rxReadIndex + length) % lConfig->rxCapacity);
    return DRV_STATUS_OK;
}

void bspUartHandleTxDmaIrq(uint8_t uart)
{
    const struct stBspUartConfig *lConfig;
    struct stBspUartState *lState;

    lConfig = bspUartGetConfig(uart);
    lState = bspUartGetState(uart);
    if ((lConfig == NULL) || (lState == NULL) || !lConfig->useTxDma) {
        return;
    }

    if (DMA_GetITStatus(DMA1_IT_TC4) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_TC4);
        bspUartStopTxDma(lConfig, lState);
    }

    if (DMA_GetITStatus(DMA1_IT_TE4) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_TE4);
        bspUartStopTxDma(lConfig, lState);
    }
}

void bspUartHandleRxDmaIrq(uint8_t uart)
{
    const struct stBspUartConfig *lConfig;

    lConfig = bspUartGetConfig(uart);
    if ((lConfig == NULL) || !lConfig->useRxDma) {
        return;
    }

    if (DMA_GetITStatus(DMA1_IT_TC5) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_TC5);
    }

    if (DMA_GetITStatus(DMA1_IT_HT5) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_HT5);
    }

    if (DMA_GetITStatus(DMA1_IT_TE5) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_TE5);
    }
}

void bspUartHandleIrq(uint8_t uart)
{
    const struct stBspUartConfig *lConfig;
    struct stBspUartState *lState;
    uint16_t lNextWriteIndex;
    uint8_t lData;

    lConfig = bspUartGetConfig(uart);
    lState = bspUartGetState(uart);
    if ((lConfig == NULL) || (lState == NULL) || !lState->isInitialized || lConfig->useRxDma) {
        return;
    }

    if (USART_GetITStatus(lConfig->uart, USART_IT_RXNE) != RESET) {
        lData = (uint8_t)USART_ReceiveData(lConfig->uart);
        lNextWriteIndex = (uint16_t)((lState->rxWriteIndex + 1U) % lConfig->rxCapacity);
        if (lNextWriteIndex != lState->rxReadIndex) {
            lConfig->rxStorage[lState->rxWriteIndex] = lData;
            lState->rxWriteIndex = lNextWriteIndex;
        }
    }
}

 /**************************End of file********************************/
