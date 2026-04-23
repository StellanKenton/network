/***********************************************************************************
* @file     : bspuart.c
* @brief    : Board-level UART BSP implementation.
* @details  : Maps logical UARTs to STM32 HAL UART instances and raw RX storage.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bspuart.h"

#include <string.h>
#include <stddef.h>

#include "usart.h"
#include "../../rep/service/rtos/rtos.h"

#include "../port/drvuart_port.h"

static uint8_t gBspUartRxStorageWifi[DRVUART_RECVLEN_WIFI];
static uint8_t gBspUartRxStorageAudio[DRVUART_RECVLEN_AUDIO];

typedef struct stBspUartRxContext {
    UART_HandleTypeDef *handle;
    uint8_t *storage;
    uint16_t capacity;
    volatile uint16_t head;
    volatile uint16_t tail;
    uint8_t *dmaBuffer;
    uint16_t dmaCapacity;
    volatile uint16_t pendingLength;
    volatile uint8_t pendingRx;
    volatile uint8_t txBusy;
} stBspUartRxContext;

static uint8_t gBspUartRxDmaWifi[DRVUART_RECVLEN_WIFI];
static uint8_t gBspUartRxDmaAudio[DRVUART_RECVLEN_AUDIO];

static stBspUartRxContext gBspUartRxContext[DRVUART_MAX] = {
    [DRVUART_WIFI] = {
        .handle = &huart4,
        .storage = gBspUartRxStorageWifi,
        .capacity = (uint16_t)sizeof(gBspUartRxStorageWifi),
        .dmaBuffer = gBspUartRxDmaWifi,
        .dmaCapacity = (uint16_t)sizeof(gBspUartRxDmaWifi),
    },
    [DRVUART_AUDIO] = {
        .handle = &huart2,
        .storage = gBspUartRxStorageAudio,
        .capacity = (uint16_t)sizeof(gBspUartRxStorageAudio),
        .dmaBuffer = gBspUartRxDmaAudio,
        .dmaCapacity = (uint16_t)sizeof(gBspUartRxDmaAudio),
    },
};

static uint32_t bspUartEnterCritical(void)
{
    uint32_t lPrimask = __get_PRIMASK();

    __disable_irq();
    return lPrimask;
}

static void bspUartExitCritical(uint32_t primask)
{
    if (primask == 0U) {
        __enable_irq();
    }
}

static eDrvStatus bspUartStatusFromHal(HAL_StatusTypeDef halStatus)
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

static uint32_t bspUartGetTickMs(void)
{
    if (repRtosIsSchedulerRunning()) {
        return repRtosGetTickMs();
    }

    return HAL_GetTick();
}

static stBspUartRxContext *bspUartGetContext(uint8_t uart)
{
    if (uart >= DRVUART_MAX) {
        return NULL;
    }

    return &gBspUartRxContext[uart];
}

static stBspUartRxContext *bspUartGetContextByHandle(UART_HandleTypeDef *handle)
{
    uint8_t lIndex;

    if (handle == NULL) {
        return NULL;
    }

    for (lIndex = 0U; lIndex < DRVUART_MAX; lIndex++) {
        if (gBspUartRxContext[lIndex].handle == handle) {
            return &gBspUartRxContext[lIndex];
        }
    }

    return NULL;
}

static uint16_t bspUartGetUsedLocked(const stBspUartRxContext *context)
{
    if (context->head >= context->tail) {
        return (uint16_t)(context->head - context->tail);
    }

    return (uint16_t)(context->capacity - context->tail + context->head);
}

static eDrvStatus bspUartStartRxDma(stBspUartRxContext *context)
{
    if ((context == NULL) || (context->handle == NULL) || (context->handle->Instance == NULL) || (context->dmaBuffer == NULL) || (context->dmaCapacity == 0U)) {
        return DRV_STATUS_NOT_READY;
    }

    __HAL_UART_CLEAR_IDLEFLAG(context->handle);
    __HAL_UART_CLEAR_OREFLAG(context->handle);

    if (HAL_UARTEx_ReceiveToIdle_DMA(context->handle, context->dmaBuffer, context->dmaCapacity) != HAL_OK) {
        return DRV_STATUS_ERROR;
    }

    if (context->handle->hdmarx != NULL) {
        __HAL_DMA_DISABLE_IT(context->handle->hdmarx, DMA_IT_HT);
    }

    return DRV_STATUS_OK;
}

static eDrvStatus bspUartStartTxDma(stBspUartRxContext *context, const uint8_t *buffer, uint16_t length)
{
    HAL_StatusTypeDef lHalStatus;
    uint32_t lPrimask;

    if ((context == NULL) || (context->handle == NULL) || (context->handle->Instance == NULL) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lPrimask = bspUartEnterCritical();
    if (context->txBusy != 0U) {
        bspUartExitCritical(lPrimask);
        return DRV_STATUS_BUSY;
    }
    context->txBusy = 1U;
    bspUartExitCritical(lPrimask);

    lHalStatus = HAL_UART_Transmit_DMA(context->handle, (uint8_t *)buffer, length);
    if (lHalStatus != HAL_OK) {
        lPrimask = bspUartEnterCritical();
        context->txBusy = 0U;
        bspUartExitCritical(lPrimask);
    }

    return bspUartStatusFromHal(lHalStatus);
}

static eDrvStatus bspUartWaitTxDone(stBspUartRxContext *context, uint32_t timeoutMs)
{
    uint32_t lStartTick;

    if (context == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lStartTick = bspUartGetTickMs();
    while (context->txBusy != 0U) {
        if (timeoutMs == 0U) {
            continue;
        }

        if ((uint32_t)(bspUartGetTickMs() - lStartTick) >= timeoutMs) {
            (void)HAL_UART_AbortTransmit(context->handle);
            context->txBusy = 0U;
            return DRV_STATUS_TIMEOUT;
        }
    }

    return DRV_STATUS_OK;
}

static void bspUartWriteRawData(stBspUartRxContext *context, const uint8_t *buffer, uint16_t length)
{
    uint16_t lRemaining = length;

    if ((context == NULL) || (buffer == NULL) || (length == 0U) || (context->capacity == 0U)) {
        return;
    }

    while (lRemaining > 0U) {
        uint16_t lUsed = bspUartGetUsedLocked(context);
        uint16_t lFree = (uint16_t)(context->capacity - lUsed);
        uint16_t lChunkToEnd;
        uint16_t lChunk;

        if (lFree == 0U) {
            break;
        }

        lChunkToEnd = (uint16_t)(context->capacity - context->head);
        lChunk = lRemaining;
        if (lChunk > lFree) {
            lChunk = lFree;
        }
        if (lChunk > lChunkToEnd) {
            lChunk = lChunkToEnd;
        }

        memcpy(&context->storage[context->head], &buffer[length - lRemaining], lChunk);
        context->head = (uint16_t)((context->head + lChunk) % context->capacity);
        lRemaining = (uint16_t)(lRemaining - lChunk);
    }
}

eDrvStatus bspUartInit(uint8_t uart)
{
    stBspUartRxContext *context = bspUartGetContext(uart);

    if ((context == NULL) || (context->handle == NULL) || (context->handle->Instance == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    context->head = 0U;
    context->tail = 0U;
    context->txBusy = 0U;
    (void)memset(context->storage, 0, context->capacity);
    (void)memset(context->dmaBuffer, 0, context->dmaCapacity);

    if (context->handle->RxState != HAL_UART_STATE_READY) {
        (void)HAL_UART_AbortReceive(context->handle);
    }

    if (context->handle->gState != HAL_UART_STATE_READY) {
        (void)HAL_UART_AbortTransmit(context->handle);
    }

    return bspUartStartRxDma(context);
}

eDrvStatus bspUartTransmit(uint8_t uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    stBspUartRxContext *context = bspUartGetContext(uart);
    eDrvStatus lStatus;

    if ((context == NULL) || (context->handle == NULL) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lStatus = bspUartStartTxDma(context, buffer, length);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return bspUartWaitTxDone(context, timeoutMs);
}

eDrvStatus bspUartTransmitIt(uint8_t uart, const uint8_t *buffer, uint16_t length)
{
    stBspUartRxContext *context = bspUartGetContext(uart);
    UART_HandleTypeDef *handle = (context != NULL) ? context->handle : NULL;

    if ((handle == NULL) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return bspUartStatusFromHal(HAL_UART_Transmit_IT(handle, (uint8_t *)buffer, length));
}

eDrvStatus bspUartTransmitDma(uint8_t uart, const uint8_t *buffer, uint16_t length)
{
    stBspUartRxContext *context = bspUartGetContext(uart);

    if ((context == NULL) || (context->handle == NULL) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return bspUartStartTxDma(context, buffer, length);
}

uint16_t bspUartGetDataLen(uint8_t uart)
{
    stBspUartRxContext *context = bspUartGetContext(uart);
    uint16_t lUsed;
    uint32_t lPrimask;

    if (context == NULL) {
        return 0U;
    }

    lPrimask = bspUartEnterCritical();
    lUsed = bspUartGetUsedLocked(context);
    bspUartExitCritical(lPrimask);

    return lUsed;
}

eDrvStatus bspUartReceive(uint8_t uart, uint8_t *buffer, uint16_t length)
{
    stBspUartRxContext *context = bspUartGetContext(uart);
    uint16_t lRemaining = length;
    uint32_t lPrimask;

    if ((context == NULL) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lPrimask = bspUartEnterCritical();
    if (bspUartGetUsedLocked(context) < length) {
        bspUartExitCritical(lPrimask);
        return DRV_STATUS_NOT_READY;
    }

    while (lRemaining > 0U) {
        uint16_t lChunkToEnd = (uint16_t)(context->capacity - context->tail);
        uint16_t lChunk = lRemaining;

        if (lChunk > lChunkToEnd) {
            lChunk = lChunkToEnd;
        }

        memcpy(&buffer[length - lRemaining], &context->storage[context->tail], lChunk);
        context->tail = (uint16_t)((context->tail + lChunk) % context->capacity);
        lRemaining = (uint16_t)(lRemaining - lChunk);
    }

    bspUartExitCritical(lPrimask);
    return DRV_STATUS_OK;
}

void bspUartHandleIrq(uint8_t uart)
{
    stBspUartRxContext *context = bspUartGetContext(uart);
    uint16_t lPendingLength;
    uint32_t lPrimask;

    if (context == NULL) {
        return;
    }

    lPrimask = bspUartEnterCritical();
    lPendingLength = context->pendingLength;
    context->pendingLength = 0U;
    context->pendingRx = 0U;
    bspUartExitCritical(lPrimask);

    if ((lPendingLength > 0U) && (lPendingLength <= context->dmaCapacity)) {
        bspUartWriteRawData(context, context->dmaBuffer, lPendingLength);
    }

    if ((lPendingLength > 0U) || (context->handle->RxState != HAL_UART_STATE_BUSY_RX)) {
        (void)bspUartStartRxDma(context);
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Pos)
{
    stBspUartRxContext *context = bspUartGetContextByHandle(huart);
    HAL_UART_RxEventTypeTypeDef lEventType;
    uint32_t lPrimask;

    if (context == NULL) {
        return;
    }

    lEventType = HAL_UARTEx_GetRxEventType(huart);
    if (lEventType == HAL_UART_RXEVENT_HT) {
        return;
    }

    lPrimask = bspUartEnterCritical();
    if ((Pos > 0U) && (Pos <= context->dmaCapacity)) {
        bspUartWriteRawData(context, context->dmaBuffer, Pos);
        context->pendingLength = 0U;
        context->pendingRx = 0U;
    } else {
        context->pendingLength = 0U;
        context->pendingRx = 0U;
    }
    bspUartExitCritical(lPrimask);

    (void)bspUartStartRxDma(context);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    stBspUartRxContext *context = bspUartGetContextByHandle(huart);

    if (context == NULL) {
        return;
    }

    context->txBusy = 0U;

    if ((huart->ErrorCode & HAL_UART_ERROR_ORE) != 0U) {
        __HAL_UART_CLEAR_OREFLAG(huart);
    }

    (void)bspUartStartRxDma(context);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    stBspUartRxContext *context = bspUartGetContextByHandle(huart);

    if (context == NULL) {
        return;
    }

    context->txBusy = 0U;
}
/**************************End of file********************************/
