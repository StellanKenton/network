/***********************************************************************************
* @file     : bspspi.c
* @brief    : Board SPI BSP implementation.
**********************************************************************************/
#include "bspspi.h"

#include "../../SYSTEM/sys/sys.h"

#include "../../rep/driver/drvgpio/drvgpio.h"

#include "../port/drvgpio_port.h"
#include "../port/drvspi_port.h"

#define BSP_SPI_TIMEOUT_LOOP_PER_MS    6000U
#define BSP_SPI_FLASH_PERIPH           SPI5

const stBspSpiCsPin gBspSpiFlashCsPin = {DRVGPIO_FLASH_CS, 1U};

static eDrvStatus bspSpiWaitFlag(uint16_t flag, FlagStatus expected, uint32_t timeoutMs)
{
	uint32_t lLoops;

	lLoops = (timeoutMs == 0U) ? BSP_SPI_TIMEOUT_LOOP_PER_MS : (timeoutMs * BSP_SPI_TIMEOUT_LOOP_PER_MS);
	if (lLoops == 0U) {
		lLoops = 1U;
	}

	while (lLoops > 0U) {
		if (SPI_I2S_GetFlagStatus(BSP_SPI_FLASH_PERIPH, flag) == expected) {
			return DRV_STATUS_OK;
		}
		lLoops--;
	}

	return DRV_STATUS_TIMEOUT;
}

eDrvStatus bspSpiInit(uint8_t spi)
{
	GPIO_InitTypeDef lGpioInit;
	SPI_InitTypeDef lSpiInit;

	if (spi != DRVSPI_FLASH) {
		return DRV_STATUS_INVALID_PARAM;
	}

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI5, ENABLE);

	GPIO_PinAFConfig(GPIOF, GPIO_PinSource7, GPIO_AF_SPI5);
	GPIO_PinAFConfig(GPIOF, GPIO_PinSource8, GPIO_AF_SPI5);
	GPIO_PinAFConfig(GPIOF, GPIO_PinSource9, GPIO_AF_SPI5);

	GPIO_StructInit(&lGpioInit);
	lGpioInit.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9;
	lGpioInit.GPIO_Mode = GPIO_Mode_AF;
	lGpioInit.GPIO_OType = GPIO_OType_PP;
	lGpioInit.GPIO_PuPd = GPIO_PuPd_UP;
	lGpioInit.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOF, &lGpioInit);

	SPI_I2S_DeInit(BSP_SPI_FLASH_PERIPH);
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
	SPI_Init(BSP_SPI_FLASH_PERIPH, &lSpiInit);
	SPI_Cmd(BSP_SPI_FLASH_PERIPH, ENABLE);
	return DRV_STATUS_OK;
}

eDrvStatus bspSpiTransfer(uint8_t spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint8_t fillData, uint32_t timeoutMs)
{
	uint16_t lIndex;
	uint8_t lTxData;
	eDrvStatus lStatus;

	if ((spi != DRVSPI_FLASH) || (length == 0U)) {
		return DRV_STATUS_INVALID_PARAM;
	}

	for (lIndex = 0U; lIndex < length; lIndex++) {
		lTxData = (txBuffer != NULL) ? txBuffer[lIndex] : fillData;
		lStatus = bspSpiWaitFlag(SPI_I2S_FLAG_TXE, SET, timeoutMs);
		if (lStatus != DRV_STATUS_OK) {
			return lStatus;
		}
		SPI_I2S_SendData(BSP_SPI_FLASH_PERIPH, lTxData);

		lStatus = bspSpiWaitFlag(SPI_I2S_FLAG_RXNE, SET, timeoutMs);
		if (lStatus != DRV_STATUS_OK) {
			return lStatus;
		}
		lTxData = (uint8_t)SPI_I2S_ReceiveData(BSP_SPI_FLASH_PERIPH);
		if (rxBuffer != NULL) {
			rxBuffer[lIndex] = lTxData;
		}
	}

	return bspSpiWaitFlag(SPI_I2S_FLAG_BSY, RESET, timeoutMs);
}

void bspSpiCsInit(void *context)
{
	stBspSpiCsPin *lPin;

	lPin = (stBspSpiCsPin *)context;
	if (lPin == NULL) {
		return;
	}

	drvGpioWrite(lPin->pin, (lPin->isActiveLow != 0U) ? DRVGPIO_PIN_SET : DRVGPIO_PIN_RESET);
}

void bspSpiCsWrite(void *context, bool isActive)
{
	stBspSpiCsPin *lPin;
	eDrvGpioPinState lState;

	lPin = (stBspSpiCsPin *)context;
	if (lPin == NULL) {
		return;
	}

	lState = ((isActive != false) == (lPin->isActiveLow != 0U)) ? DRVGPIO_PIN_RESET : DRVGPIO_PIN_SET;
	drvGpioWrite(lPin->pin, lState);
}

/**************************End of file********************************/
