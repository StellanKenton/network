/***********************************************************************************
* @file     : bspuart.c
* @brief    : Board UART BSP implementation.
**********************************************************************************/
#include "bspuart.h"

#include <string.h>

#include "../../SYSTEM/sys/sys.h"
#include "bsp_rtt.h"

#define BSP_UART_BAUDRATE               115200U
#define BSP_UART_TIMEOUT_LOOP_PER_MS    6000U
#define BSP_UART_TRACE_SAMPLE_BYTES     24U
#define BSP_UART_DMA_WAIT_LOOPS         60000U

typedef struct stBspUartContext {
	USART_TypeDef *instance;
	DMA_Stream_TypeDef *rxDmaStream;
	DMA_Stream_TypeDef *txDmaStream;
	uint32_t rxDmaChannel;
	uint32_t txDmaChannel;
	uint32_t rxDmaFlags;
	uint32_t txDmaFlags;
	uint32_t rxDmaTcIf;
	uint32_t txDmaTcIf;
	uint8_t *storage;
	uint8_t *rxDmaBuffer;
	uint16_t capacity;
	uint16_t rxDmaCapacity;
	volatile uint16_t head;
	volatile uint16_t tail;
	volatile uint8_t txBusy;
	volatile eDrvStatus txStatus;
	uint8_t isInitialized;
} stBspUartContext;

typedef struct stBspUartPendingTrace {
	volatile uint8_t pending;
	uint16_t length;
	uint8_t sample[BSP_UART_TRACE_SAMPLE_BYTES];
} stBspUartPendingTrace;

static uint8_t gBspUartRxStorageWifi[DRVUART_RECVLEN_WIFI];
static uint8_t gBspUartRxStorageCellular[DRVUART_RECVLEN_CELLULAR];
static uint8_t gBspUartRxDmaWifi[DRVUART_RECVLEN_WIFI];
static uint8_t gBspUartRxDmaCellular[DRVUART_RECVLEN_CELLULAR];
static stBspUartPendingTrace gBspUartPendingRxTrace[DRVUART_MAX];

static stBspUartContext gBspUartContext[DRVUART_MAX] = {
	{
		USART2,
		DMA1_Stream5,
		DMA1_Stream6,
		DMA_Channel_4,
		DMA_Channel_4,
		DMA_FLAG_FEIF5 | DMA_FLAG_DMEIF5 | DMA_FLAG_TEIF5 | DMA_FLAG_HTIF5 | DMA_FLAG_TCIF5,
		DMA_FLAG_FEIF6 | DMA_FLAG_DMEIF6 | DMA_FLAG_TEIF6 | DMA_FLAG_HTIF6 | DMA_FLAG_TCIF6,
		DMA_IT_TCIF5,
		DMA_IT_TCIF6,
		gBspUartRxStorageWifi,
		gBspUartRxDmaWifi,
		DRVUART_RECVLEN_WIFI,
		DRVUART_RECVLEN_WIFI,
		0U,
		0U,
		0U,
		DRV_STATUS_OK,
		0U,
	},
	{
		USART3,
		DMA1_Stream1,
		DMA1_Stream3,
		DMA_Channel_4,
		DMA_Channel_4,
		DMA_FLAG_FEIF1 | DMA_FLAG_DMEIF1 | DMA_FLAG_TEIF1 | DMA_FLAG_HTIF1 | DMA_FLAG_TCIF1,
		DMA_FLAG_FEIF3 | DMA_FLAG_DMEIF3 | DMA_FLAG_TEIF3 | DMA_FLAG_HTIF3 | DMA_FLAG_TCIF3,
		DMA_IT_TCIF1,
		DMA_IT_TCIF3,
		gBspUartRxStorageCellular,
		gBspUartRxDmaCellular,
		DRVUART_RECVLEN_CELLULAR,
		DRVUART_RECVLEN_CELLULAR,
		0U,
		0U,
		0U,
		DRV_STATUS_OK,
		0U,
	},
};

static void bspUartTraceBuffer(uint8_t uart, const char *direction, const uint8_t *buffer, uint16_t length);
static uint32_t bspUartEnterCritical(void);
static void bspUartExitCritical(uint32_t primask);

static const char *bspUartGetName(uint8_t uart)
{
	if (uart == DRVUART_WIFI) {
		return "wifi";
	}

	if (uart == DRVUART_CELLULAR) {
		return "cellular";
	}

	return "unknown";
}

static uint8_t bspUartGetIdByContext(const stBspUartContext *context)
{
	if (context == &gBspUartContext[DRVUART_WIFI]) {
		return DRVUART_WIFI;
	}

	if (context == &gBspUartContext[DRVUART_CELLULAR]) {
		return DRVUART_CELLULAR;
	}

	return DRVUART_MAX;
}

static uint8_t bspUartTraceAppendChar(char *buffer, uint16_t bufferSize, uint16_t *offset, char value)
{
	if ((buffer == NULL) || (offset == NULL) || (bufferSize == 0U) || (*offset >= (uint16_t)(bufferSize - 1U))) {
		return 0U;
	}

	buffer[*offset] = value;
	*offset = (uint16_t)(*offset + 1U);
	buffer[*offset] = '\0';
	return 1U;
}

static uint8_t bspUartTraceAppendText(char *buffer, uint16_t bufferSize, uint16_t *offset, const char *text)
{
	if ((buffer == NULL) || (offset == NULL) || (text == NULL)) {
		return 0U;
	}

	while (*text != '\0') {
		if (bspUartTraceAppendChar(buffer, bufferSize, offset, *text++) == 0U) {
			return 0U;
		}
	}

	return 1U;
}

static uint8_t bspUartTraceAppendU16(char *buffer, uint16_t bufferSize, uint16_t *offset, uint16_t value)
{
	char lDigits[5];
	uint16_t lDigitCount = 0U;

	do {
		lDigits[lDigitCount++] = (char)('0' + (value % 10U));
		value = (uint16_t)(value / 10U);
	} while ((value > 0U) && (lDigitCount < (uint16_t)sizeof(lDigits)));

	while (lDigitCount > 0U) {
		lDigitCount--;
		if (bspUartTraceAppendChar(buffer, bufferSize, offset, lDigits[lDigitCount]) == 0U) {
			return 0U;
		}
	}

	return 1U;
}

static uint8_t bspUartTraceAppendHexByte(char *buffer, uint16_t bufferSize, uint16_t *offset, uint8_t value)
{
	static const char lHexChars[] = "0123456789ABCDEF";

	if (bspUartTraceAppendChar(buffer, bufferSize, offset, lHexChars[(value >> 4) & 0x0FU]) == 0U) {
		return 0U;
	}

	return bspUartTraceAppendChar(buffer, bufferSize, offset, lHexChars[value & 0x0FU]);
}

static void bspUartTraceText(const char *text)
{
	if (text == NULL) {
		return;
	}

	(void)bspRttLogWrite((const uint8_t *)text, (uint16_t)strlen(text));
}

static void bspUartStorePendingRxTrace(uint8_t uart, const uint8_t *buffer, uint16_t length)
{
	stBspUartPendingTrace *lTrace;
	uint16_t lSampleLen;

	if ((uart >= DRVUART_MAX) || (buffer == NULL) || (length == 0U)) {
		return;
	}

	lTrace = &gBspUartPendingRxTrace[uart];
	lSampleLen = (length > BSP_UART_TRACE_SAMPLE_BYTES) ? BSP_UART_TRACE_SAMPLE_BYTES : length;
	(void)memcpy(lTrace->sample, buffer, lSampleLen);
	lTrace->length = length;
	lTrace->pending = 1U;
}

static void bspUartFlushPendingRxTrace(uint8_t uart)
{
	stBspUartPendingTrace lTrace;
	uint32_t lPrimask;

	if (uart >= DRVUART_MAX) {
		return;
	}

	lPrimask = bspUartEnterCritical();
	if (gBspUartPendingRxTrace[uart].pending == 0U) {
		bspUartExitCritical(lPrimask);
		return;
	}

	lTrace = gBspUartPendingRxTrace[uart];
	gBspUartPendingRxTrace[uart].pending = 0U;
	bspUartExitCritical(lPrimask);

	bspUartTraceBuffer(uart, "rx", lTrace.sample, lTrace.length);
}

static void bspUartTraceBuffer(uint8_t uart, const char *direction, const uint8_t *buffer, uint16_t length)
{
	char lLine[192];
	uint16_t lOffset = 0U;
	uint16_t lIndex;
	uint16_t lTraceLen;

	if ((uart != DRVUART_WIFI) || (direction == NULL) || (buffer == NULL) || (length == 0U)) {
		return;
	}

	lTraceLen = (length > BSP_UART_TRACE_SAMPLE_BYTES) ? BSP_UART_TRACE_SAMPLE_BYTES : length;
	lLine[0] = '\0';
	if ((bspUartTraceAppendText(lLine, sizeof(lLine), &lOffset, "[uart:") == 0U) ||
	    (bspUartTraceAppendText(lLine, sizeof(lLine), &lOffset, bspUartGetName(uart)) == 0U) ||
	    (bspUartTraceAppendText(lLine, sizeof(lLine), &lOffset, "] ") == 0U) ||
	    (bspUartTraceAppendText(lLine, sizeof(lLine), &lOffset, direction) == 0U) ||
	    (bspUartTraceAppendText(lLine, sizeof(lLine), &lOffset, " len=") == 0U) ||
	    (bspUartTraceAppendU16(lLine, sizeof(lLine), &lOffset, length) == 0U) ||
	    (bspUartTraceAppendText(lLine, sizeof(lLine), &lOffset, " data=") == 0U)) {
		return;
	}

	for (lIndex = 0U; lIndex < lTraceLen; lIndex++) {
		if (bspUartTraceAppendHexByte(lLine, sizeof(lLine), &lOffset, buffer[lIndex]) == 0U) {
			break;
		}

		if ((lIndex + 1U < lTraceLen) &&
		    (bspUartTraceAppendChar(lLine, sizeof(lLine), &lOffset, ' ') == 0U)) {
			break;
		}
	}

	if ((length > lTraceLen) && (lOffset < (sizeof(lLine) - 6U))) {
		(void)memcpy(&lLine[lOffset], " ...", sizeof(" ...") - 1U);
		lOffset = (uint16_t)(lOffset + (sizeof(" ...") - 1U));
	}

	if (lOffset < (sizeof(lLine) - 2U)) {
		lLine[lOffset++] = '\r';
		lLine[lOffset++] = '\n';
	}
	lLine[lOffset] = '\0';
	bspUartTraceText(lLine);
}

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

static eDrvStatus bspUartWaitDmaDisabled(DMA_Stream_TypeDef *stream)
{
	uint32_t lLoops;

	lLoops = BSP_UART_DMA_WAIT_LOOPS;
	while ((DMA_GetCmdStatus(stream) != DISABLE) && (lLoops > 0U)) {
		lLoops--;
	}

	return (lLoops > 0U) ? DRV_STATUS_OK : DRV_STATUS_TIMEOUT;
}

static void bspUartEnableInterrupt(uint8_t uart)
{
	NVIC_InitTypeDef lNvicInit;
	stBspUartContext *lContext;
	IRQn_Type lIrqChannel;

	lContext = bspUartGetContext(uart);
	if (lContext == NULL) {
		return;
	}

	if (uart == DRVUART_WIFI) {
		lIrqChannel = USART2_IRQn;
	} else if (uart == DRVUART_CELLULAR) {
		lIrqChannel = USART3_IRQn;
	} else {
		return;
	}

	USART_ITConfig(lContext->instance, USART_IT_IDLE, ENABLE);

	lNvicInit.NVIC_IRQChannel = lIrqChannel;
	lNvicInit.NVIC_IRQChannelPreemptionPriority = 6U;
	lNvicInit.NVIC_IRQChannelSubPriority = 0U;
	lNvicInit.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&lNvicInit);

	if (uart == DRVUART_WIFI) {
		lNvicInit.NVIC_IRQChannel = DMA1_Stream5_IRQn;
		NVIC_Init(&lNvicInit);
		lNvicInit.NVIC_IRQChannel = DMA1_Stream6_IRQn;
		NVIC_Init(&lNvicInit);
	} else {
		lNvicInit.NVIC_IRQChannel = DMA1_Stream1_IRQn;
		NVIC_Init(&lNvicInit);
		lNvicInit.NVIC_IRQChannel = DMA1_Stream3_IRQn;
		NVIC_Init(&lNvicInit);
	}
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

static void bspUartPushBuffer(stBspUartContext *context, const uint8_t *buffer, uint16_t length)
{
	uint8_t lTraceBuffer[BSP_UART_TRACE_SAMPLE_BYTES];
	uint16_t lIndex;
	uint16_t lTraceCount;
	uint8_t lUart;

	if ((context == NULL) || (buffer == NULL) || (length == 0U)) {
		return;
	}

	lUart = bspUartGetIdByContext(context);
	lTraceCount = 0U;
	for (lIndex = 0U; lIndex < length; lIndex++) {
		bspUartPushByte(context, buffer[lIndex]);
		if (lTraceCount < BSP_UART_TRACE_SAMPLE_BYTES) {
			lTraceBuffer[lTraceCount++] = buffer[lIndex];
		}
	}

	if (lTraceCount > 0U) {
		bspUartStorePendingRxTrace(lUart, lTraceBuffer, length);
	}
}

static void bspUartEnableClockAndPins(uint8_t uart)
{
	GPIO_InitTypeDef lGpioInit;
	USART_InitTypeDef lUartInit;

	GPIO_StructInit(&lGpioInit);
	USART_StructInit(&lUartInit);

	if (uart == DRVUART_WIFI) {
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

		GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
		GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

		lGpioInit.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
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
		USART_Init(USART2, &lUartInit);
		USART_Cmd(USART2, ENABLE);
	} else {
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
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
	}
}

static eDrvStatus bspUartConfigureDma(stBspUartContext *context)
{
	DMA_InitTypeDef lDmaInit;

	if ((context == NULL) || (context->instance == NULL) || (context->rxDmaStream == NULL) ||
	    (context->txDmaStream == NULL) || (context->rxDmaBuffer == NULL) || (context->rxDmaCapacity == 0U)) {
		return DRV_STATUS_INVALID_PARAM;
	}

	DMA_Cmd(context->rxDmaStream, DISABLE);
	if (bspUartWaitDmaDisabled(context->rxDmaStream) != DRV_STATUS_OK) {
		return DRV_STATUS_TIMEOUT;
	}
	DMA_Cmd(context->txDmaStream, DISABLE);
	if (bspUartWaitDmaDisabled(context->txDmaStream) != DRV_STATUS_OK) {
		return DRV_STATUS_TIMEOUT;
	}

	DMA_DeInit(context->rxDmaStream);
	DMA_DeInit(context->txDmaStream);
	DMA_ClearFlag(context->rxDmaStream, context->rxDmaFlags);
	DMA_ClearFlag(context->txDmaStream, context->txDmaFlags);

	DMA_StructInit(&lDmaInit);
	lDmaInit.DMA_Channel = context->rxDmaChannel;
	lDmaInit.DMA_PeripheralBaseAddr = (uint32_t)&context->instance->DR;
	lDmaInit.DMA_Memory0BaseAddr = (uint32_t)context->rxDmaBuffer;
	lDmaInit.DMA_DIR = DMA_DIR_PeripheralToMemory;
	lDmaInit.DMA_BufferSize = context->rxDmaCapacity;
	lDmaInit.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	lDmaInit.DMA_MemoryInc = DMA_MemoryInc_Enable;
	lDmaInit.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	lDmaInit.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	lDmaInit.DMA_Mode = DMA_Mode_Normal;
	lDmaInit.DMA_Priority = DMA_Priority_VeryHigh;
	lDmaInit.DMA_FIFOMode = DMA_FIFOMode_Disable;
	lDmaInit.DMA_FIFOThreshold = DMA_FIFOThreshold_1QuarterFull;
	lDmaInit.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	lDmaInit.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(context->rxDmaStream, &lDmaInit);
	DMA_ITConfig(context->rxDmaStream, DMA_IT_TC, ENABLE);

	DMA_StructInit(&lDmaInit);
	lDmaInit.DMA_Channel = context->txDmaChannel;
	lDmaInit.DMA_PeripheralBaseAddr = (uint32_t)&context->instance->DR;
	lDmaInit.DMA_Memory0BaseAddr = 0U;
	lDmaInit.DMA_DIR = DMA_DIR_MemoryToPeripheral;
	lDmaInit.DMA_BufferSize = 0U;
	lDmaInit.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	lDmaInit.DMA_MemoryInc = DMA_MemoryInc_Enable;
	lDmaInit.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	lDmaInit.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	lDmaInit.DMA_Mode = DMA_Mode_Normal;
	lDmaInit.DMA_Priority = DMA_Priority_High;
	lDmaInit.DMA_FIFOMode = DMA_FIFOMode_Disable;
	lDmaInit.DMA_FIFOThreshold = DMA_FIFOThreshold_1QuarterFull;
	lDmaInit.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	lDmaInit.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(context->txDmaStream, &lDmaInit);
	DMA_ITConfig(context->txDmaStream, DMA_IT_TC, ENABLE);
	return DRV_STATUS_OK;
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

static eDrvStatus bspUartStartRxDma(stBspUartContext *context)
{
	if ((context == NULL) || (context->rxDmaStream == NULL) || (context->instance == NULL)) {
		return DRV_STATUS_INVALID_PARAM;
	}

	USART_DMACmd(context->instance, USART_DMAReq_Rx, DISABLE);
	DMA_Cmd(context->rxDmaStream, DISABLE);
	if (bspUartWaitDmaDisabled(context->rxDmaStream) != DRV_STATUS_OK) {
		return DRV_STATUS_TIMEOUT;
	}
	DMA_ClearFlag(context->rxDmaStream, context->rxDmaFlags);
	DMA_SetCurrDataCounter(context->rxDmaStream, context->rxDmaCapacity);
	DMA_Cmd(context->rxDmaStream, ENABLE);
	USART_DMACmd(context->instance, USART_DMAReq_Rx, ENABLE);
	return DRV_STATUS_OK;
}

static void bspUartAbortTxDma(stBspUartContext *context)
{
	if ((context == NULL) || (context->txDmaStream == NULL) || (context->instance == NULL)) {
		return;
	}

	USART_DMACmd(context->instance, USART_DMAReq_Tx, DISABLE);
	DMA_Cmd(context->txDmaStream, DISABLE);
	(void)bspUartWaitDmaDisabled(context->txDmaStream);
	DMA_ClearFlag(context->txDmaStream, context->txDmaFlags);
	context->txBusy = 0U;
}

static eDrvStatus bspUartStartTxDma(stBspUartContext *context, const uint8_t *buffer, uint16_t length)
{
	if ((context == NULL) || (buffer == NULL) || (length == 0U) || (context->txDmaStream == NULL) ||
	    (context->instance == NULL)) {
		return DRV_STATUS_INVALID_PARAM;
	}

	if (context->txBusy != 0U) {
		return DRV_STATUS_BUSY;
	}

	DMA_Cmd(context->txDmaStream, DISABLE);
	if (bspUartWaitDmaDisabled(context->txDmaStream) != DRV_STATUS_OK) {
		return DRV_STATUS_TIMEOUT;
	}

	DMA_ClearFlag(context->txDmaStream, context->txDmaFlags);
	context->txDmaStream->M0AR = (uint32_t)buffer;
	context->txDmaStream->NDTR = length;
	context->txStatus = DRV_STATUS_OK;
	context->txBusy = 1U;
	USART_ClearFlag(context->instance, USART_FLAG_TC);
	USART_DMACmd(context->instance, USART_DMAReq_Tx, ENABLE);
	DMA_Cmd(context->txDmaStream, ENABLE);
	return DRV_STATUS_OK;
}

static void bspUartServiceRxDma(stBspUartContext *context)
{
	uint16_t lRemaining;
	uint16_t lReceived;

	if ((context == NULL) || (context->rxDmaStream == NULL) || (context->instance == NULL)) {
		return;
	}

	USART_DMACmd(context->instance, USART_DMAReq_Rx, DISABLE);
	DMA_Cmd(context->rxDmaStream, DISABLE);
	if (bspUartWaitDmaDisabled(context->rxDmaStream) != DRV_STATUS_OK) {
		return;
	}

	lRemaining = DMA_GetCurrDataCounter(context->rxDmaStream);
	if (lRemaining > context->rxDmaCapacity) {
		lRemaining = context->rxDmaCapacity;
	}
	lReceived = (uint16_t)(context->rxDmaCapacity - lRemaining);
	if (lReceived > 0U) {
		bspUartPushBuffer(context, context->rxDmaBuffer, lReceived);
	}

	(void)bspUartStartRxDma(context);
}

static eDrvStatus bspUartWaitTransmitComplete(stBspUartContext *context, uint32_t timeoutMs)
{
	uint32_t lLoops;

	if (context == NULL) {
		return DRV_STATUS_INVALID_PARAM;
	}

	lLoops = (timeoutMs == 0U) ? BSP_UART_TIMEOUT_LOOP_PER_MS : (timeoutMs * BSP_UART_TIMEOUT_LOOP_PER_MS);
	if (lLoops == 0U) {
		lLoops = 1U;
	}

	while ((context->txBusy != 0U) && (lLoops > 0U)) {
		lLoops--;
	}

	if (context->txBusy != 0U) {
		bspUartAbortTxDma(context);
		context->txStatus = DRV_STATUS_TIMEOUT;
		return DRV_STATUS_TIMEOUT;
	}

	if (context->txStatus != DRV_STATUS_OK) {
		return context->txStatus;
	}

	return bspUartWaitFlag(context->instance, USART_FLAG_TC, SET, timeoutMs);
}

static void bspUartHandleOverrun(stBspUartContext *context)
{
	uint8_t lUart;

	if ((context == NULL) || (context->instance == NULL)) {
		return;
	}

	if (USART_GetFlagStatus(context->instance, USART_FLAG_ORE) == RESET) {
		return;
	}

	lUart = bspUartGetIdByContext(context);
	(void)context->instance->SR;
	(void)context->instance->DR;
	if (lUart == DRVUART_WIFI) {
		bspUartTraceText("[uart:wifi] rx overrun detected\r\n");
	}
	(void)bspUartStartRxDma(context);
}

eDrvStatus bspUartInit(uint8_t uart)
{
	stBspUartContext *lContext;
	eDrvStatus lStatus;

	lContext = bspUartGetContext(uart);
	if (lContext == NULL) {
		return DRV_STATUS_INVALID_PARAM;
	}

	lContext->head = 0U;
	lContext->tail = 0U;
	lContext->txBusy = 0U;
	lContext->txStatus = DRV_STATUS_OK;
	(void)memset(lContext->storage, 0, lContext->capacity);
	(void)memset(lContext->rxDmaBuffer, 0, lContext->rxDmaCapacity);
	bspUartEnableClockAndPins(uart);
	lStatus = bspUartConfigureDma(lContext);
	if (lStatus != DRV_STATUS_OK) {
		return lStatus;
	}
	bspUartEnableInterrupt(uart);
	lStatus = bspUartStartRxDma(lContext);
	if (lStatus != DRV_STATUS_OK) {
		return lStatus;
	}
	lContext->isInitialized = 1U;
	if (uart == DRVUART_WIFI) {
		bspUartTraceText("[uart:wifi] init USART2 PA2/PA3 115200, host logs stay on RTT\r\n");
	}
	return DRV_STATUS_OK;
}

eDrvStatus bspUartTransmit(uint8_t uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
	stBspUartContext *lContext;
	eDrvStatus lStatus;

	lContext = bspUartGetContext(uart);
	if ((lContext == NULL) || (buffer == NULL) || (length == 0U) || (lContext->isInitialized == 0U)) {
		return DRV_STATUS_INVALID_PARAM;
	}

	bspUartTraceBuffer(uart, "tx", buffer, length);
	lStatus = bspUartStartTxDma(lContext, buffer, length);
	if (lStatus != DRV_STATUS_OK) {
		return lStatus;
	}

	return bspUartWaitTransmitComplete(lContext, timeoutMs);
}

eDrvStatus bspUartTransmitIt(uint8_t uart, const uint8_t *buffer, uint16_t length)
{
	return bspUartTransmitDma(uart, buffer, length);
}

eDrvStatus bspUartTransmitDma(uint8_t uart, const uint8_t *buffer, uint16_t length)
{
	stBspUartContext *lContext;
	eDrvStatus lStatus;

	lContext = bspUartGetContext(uart);
	if ((lContext == NULL) || (buffer == NULL) || (length == 0U) || (lContext->isInitialized == 0U)) {
		return DRV_STATUS_INVALID_PARAM;
	}

	bspUartTraceBuffer(uart, "tx", buffer, length);
	lStatus = bspUartStartTxDma(lContext, buffer, length);
	if (lStatus != DRV_STATUS_OK) {
		return lStatus;
	}

	return DRV_STATUS_OK;
}

uint16_t bspUartGetDataLen(uint8_t uart)
{
	stBspUartContext *lContext;
	uint16_t lUsed;
	uint32_t lPrimask;

	bspUartFlushPendingRxTrace(uart);

	lContext = bspUartGetContext(uart);
	if (lContext == NULL) {
		return 0U;
	}

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

	bspUartFlushPendingRxTrace(uart);

	lContext = bspUartGetContext(uart);
	if ((lContext == NULL) || (buffer == NULL) || (length == 0U)) {
		return DRV_STATUS_INVALID_PARAM;
	}

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
	stBspUartContext *lContext;

	lContext = bspUartGetContext(uart);
	if (lContext == NULL) {
		return;
	}

	if (USART_GetITStatus(lContext->instance, USART_IT_IDLE) != RESET) {
		(void)lContext->instance->SR;
		(void)lContext->instance->DR;
		bspUartServiceRxDma(lContext);
	}

	bspUartHandleOverrun(lContext);
}

void bspUartHandleDmaRxIrq(uint8_t uart)
{
	stBspUartContext *lContext;

	lContext = bspUartGetContext(uart);
	if (lContext == NULL) {
		return;
	}

	if (DMA_GetITStatus(lContext->rxDmaStream, lContext->rxDmaTcIf) != RESET) {
		DMA_ClearITPendingBit(lContext->rxDmaStream, lContext->rxDmaTcIf);
		DMA_ClearFlag(lContext->rxDmaStream, lContext->rxDmaFlags);
		bspUartServiceRxDma(lContext);
	}
}

void bspUartHandleDmaTxIrq(uint8_t uart)
{
	stBspUartContext *lContext;

	lContext = bspUartGetContext(uart);
	if (lContext == NULL) {
		return;
	}

	if (DMA_GetITStatus(lContext->txDmaStream, lContext->txDmaTcIf) != RESET) {
		DMA_ClearITPendingBit(lContext->txDmaStream, lContext->txDmaTcIf);
		DMA_ClearFlag(lContext->txDmaStream, lContext->txDmaFlags);
		USART_DMACmd(lContext->instance, USART_DMAReq_Tx, DISABLE);
		DMA_Cmd(lContext->txDmaStream, DISABLE);
		lContext->txStatus = DRV_STATUS_OK;
		lContext->txBusy = 0U;
	}
}

/**************************End of file********************************/
