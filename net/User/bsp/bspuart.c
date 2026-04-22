/***********************************************************************************
* @file     : bspuart.c
* @brief    : Board UART BSP implementation.
**********************************************************************************/
#include "bspuart.h"

#include <string.h>

#include "../../SYSTEM/sys/sys.h"

#define BSP_UART_BAUDRATE               115200U
#define BSP_UART_TIMEOUT_LOOP_PER_MS    6000U

typedef struct stBspUartContext {
	USART_TypeDef *instance;
	uint8_t *storage;
	uint16_t capacity;
	volatile uint16_t head;
	volatile uint16_t tail;
	uint8_t isInitialized;
} stBspUartContext;

static uint8_t gBspUartRxStorageWifi[DRVUART_RECVLEN_WIFI];
static uint8_t gBspUartRxStorageCellular[DRVUART_RECVLEN_CELLULAR];

static stBspUartContext gBspUartContext[DRVUART_MAX] = {
	{USART3, gBspUartRxStorageWifi, DRVUART_RECVLEN_WIFI, 0U, 0U, 0U},
	{USART1, gBspUartRxStorageCellular, DRVUART_RECVLEN_CELLULAR, 0U, 0U, 0U},
};

static uint32_t bspUartEnterCritical(void)
{
	uint32_t lPrimask;

	lPrimask = __get_PRIMASK();
	__disable_irq();
	return lPrimask;
}

static void bspUartExitCritical(uint32_t primask)
{
	if (primask == 0U) {
		__enable_irq();
	}
}

static stBspUartContext *bspUartGetContext(uint8_t uart)
{
	if (uart >= DRVUART_MAX) {
		return NULL;
	}

	return &gBspUartContext[uart];
}

static uint16_t bspUartGetUsedLocked(const stBspUartContext *context)
{
	if (context->head >= context->tail) {
		return (uint16_t)(context->head - context->tail);
	}

	return (uint16_t)(context->capacity - context->tail + context->head);
}

static void bspUartPushByte(stBspUartContext *context, uint8_t data)
{
	uint16_t lNextHead;

	lNextHead = (uint16_t)((context->head + 1U) % context->capacity);
	if (lNextHead == context->tail) {
		context->tail = (uint16_t)((context->tail + 1U) % context->capacity);
	}

	context->storage[context->head] = data;
	context->head = lNextHead;
}

static void bspUartEnableClockAndPins(uint8_t uart)
{
	GPIO_InitTypeDef lGpioInit;
	USART_InitTypeDef lUartInit;

	GPIO_StructInit(&lGpioInit);
	USART_StructInit(&lUartInit);

	if (uart == DRVUART_WIFI) {
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

		GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_USART3);
		GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_USART3);

		lGpioInit.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
		lGpioInit.GPIO_Mode = GPIO_Mode_AF;
		lGpioInit.GPIO_OType = GPIO_OType_PP;
		lGpioInit.GPIO_PuPd = GPIO_PuPd_UP;
		lGpioInit.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(GPIOB, &lGpioInit);

		lUartInit.USART_BaudRate = BSP_UART_BAUDRATE;
		lUartInit.USART_WordLength = USART_WordLength_8b;
		lUartInit.USART_StopBits = USART_StopBits_1;
		lUartInit.USART_Parity = USART_Parity_No;
		lUartInit.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
		lUartInit.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
		USART_Init(USART3, &lUartInit);
		USART_Cmd(USART3, ENABLE);
	} else {
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

		GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
		GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

		lGpioInit.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
		lGpioInit.GPIO_Mode = GPIO_Mode_AF;
		lGpioInit.GPIO_OType = GPIO_OType_PP;
		lGpioInit.GPIO_PuPd = GPIO_PuPd_UP;
		lGpioInit.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(GPIOA, &lGpioInit);

		lUartInit.USART_BaudRate = BSP_UART_BAUDRATE;
		lUartInit.USART_WordLength = USART_WordLength_8b;
		lUartInit.USART_StopBits = USART_StopBits_1;
		lUartInit.USART_Parity = USART_Parity_No;
		lUartInit.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
		lUartInit.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
		USART_Init(USART1, &lUartInit);
		USART_Cmd(USART1, ENABLE);
	}
}

static eDrvStatus bspUartWaitFlag(USART_TypeDef *instance, uint16_t flag, FlagStatus expected, uint32_t timeoutMs)
{
	uint32_t lLoops;

	lLoops = (timeoutMs == 0U) ? BSP_UART_TIMEOUT_LOOP_PER_MS : (timeoutMs * BSP_UART_TIMEOUT_LOOP_PER_MS);
	if (lLoops == 0U) {
		lLoops = 1U;
	}

	while (lLoops > 0U) {
		if (USART_GetFlagStatus(instance, flag) == expected) {
			return DRV_STATUS_OK;
		}
		lLoops--;
	}

	return DRV_STATUS_TIMEOUT;
}

static void bspUartServiceRx(stBspUartContext *context)
{
	uint32_t lPrimask;

	if ((context == NULL) || (context->instance == NULL)) {
		return;
	}

	lPrimask = bspUartEnterCritical();
	while (USART_GetFlagStatus(context->instance, USART_FLAG_RXNE) != RESET) {
		bspUartPushByte(context, (uint8_t)USART_ReceiveData(context->instance));
	}
	if (USART_GetFlagStatus(context->instance, USART_FLAG_ORE) != RESET) {
		(void)context->instance->SR;
		(void)context->instance->DR;
	}
	bspUartExitCritical(lPrimask);
}

eDrvStatus bspUartInit(uint8_t uart)
{
	stBspUartContext *lContext;

	lContext = bspUartGetContext(uart);
	if (lContext == NULL) {
		return DRV_STATUS_INVALID_PARAM;
	}

	lContext->head = 0U;
	lContext->tail = 0U;
	(void)memset(lContext->storage, 0, lContext->capacity);
	bspUartEnableClockAndPins(uart);
	lContext->isInitialized = 1U;
    return DRV_STATUS_OK;
}

eDrvStatus bspUartTransmit(uint8_t uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
	stBspUartContext *lContext;
	uint16_t lIndex;
	eDrvStatus lStatus;

	lContext = bspUartGetContext(uart);
	if ((lContext == NULL) || (buffer == NULL) || (length == 0U) || (lContext->isInitialized == 0U)) {
		return DRV_STATUS_INVALID_PARAM;
	}

	for (lIndex = 0U; lIndex < length; lIndex++) {
		lStatus = bspUartWaitFlag(lContext->instance, USART_FLAG_TXE, SET, timeoutMs);
		if (lStatus != DRV_STATUS_OK) {
			return lStatus;
		}
		USART_SendData(lContext->instance, buffer[lIndex]);
	}

	return bspUartWaitFlag(lContext->instance, USART_FLAG_TC, SET, timeoutMs);
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
	(void)uart;
	(void)buffer;
	(void)length;
	return DRV_STATUS_UNSUPPORTED;
}

uint16_t bspUartGetDataLen(uint8_t uart)
{
	stBspUartContext *lContext;
	uint16_t lUsed;
	uint32_t lPrimask;

	lContext = bspUartGetContext(uart);
	if (lContext == NULL) {
		return 0U;
	}

	bspUartServiceRx(lContext);
	lPrimask = bspUartEnterCritical();
	lUsed = bspUartGetUsedLocked(lContext);
	bspUartExitCritical(lPrimask);
	return lUsed;
}

eDrvStatus bspUartReceive(uint8_t uart, uint8_t *buffer, uint16_t length)
{
	stBspUartContext *lContext;
	uint16_t lRemaining;
	uint16_t lChunk;
	uint16_t lChunkToEnd;
	uint32_t lPrimask;

	lContext = bspUartGetContext(uart);
	if ((lContext == NULL) || (buffer == NULL) || (length == 0U)) {
		return DRV_STATUS_INVALID_PARAM;
	}

	bspUartServiceRx(lContext);
	lPrimask = bspUartEnterCritical();
	if (bspUartGetUsedLocked(lContext) < length) {
		bspUartExitCritical(lPrimask);
		return DRV_STATUS_NOT_READY;
	}

	lRemaining = length;
	while (lRemaining > 0U) {
		lChunkToEnd = (uint16_t)(lContext->capacity - lContext->tail);
		lChunk = lRemaining;
		if (lChunk > lChunkToEnd) {
			lChunk = lChunkToEnd;
		}
		memcpy(&buffer[length - lRemaining], &lContext->storage[lContext->tail], lChunk);
		lContext->tail = (uint16_t)((lContext->tail + lChunk) % lContext->capacity);
		lRemaining = (uint16_t)(lRemaining - lChunk);
	}
	bspUartExitCritical(lPrimask);
	return DRV_STATUS_OK;
}

void bspUartHandleIrq(uint8_t uart)
{
	bspUartServiceRx(bspUartGetContext(uart));
}

/**************************End of file********************************/