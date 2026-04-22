/***********************************************************************************
* @file     : bsp_sdio.c
* @brief    : Board SDIO BSP implementation.
**********************************************************************************/
#include "bsp_sdio.h"

#include "../../SYSTEM/sys/sys.h"

#define BSP_SDIO_INIT_CLK_DIV                118U
#define BSP_SDIO_TRANSFER_CLK_DIV            0U
#define BSP_SDIO_CMD_TIMEOUT_LOOP_PER_MS     6000U
#define BSP_SDIO_DATA_TIMEOUT                0x00FFFFFFUL
#define BSP_SDIO_BLOCK_SIZE                  512U
#define BSP_SDIO_CARD_STATE_TRAN             4U
#define BSP_SDIO_ACMD41_ARG                  0x40FF8000UL

#define BSP_SDIO_R1_ERROR_MASK               0xFDF98008UL

typedef enum eBspSdioRespType {
    BSP_SDIO_RESP_NONE = 0,
    BSP_SDIO_RESP_SHORT,
    BSP_SDIO_RESP_SHORT_NOCRC,
    BSP_SDIO_RESP_LONG,
} eBspSdioRespType;

typedef struct stBspSdioContext {
    uint8_t isInitialized;
    uint8_t isHighCapacity;
    uint16_t rca;
    stSdcardInfo info;
    uint32_t csd[4];
    uint32_t cid[4];
} stBspSdioContext;

static stBspSdioContext gBspSdioContext[BSP_SDIO_DEV_MAX];

static bool bspSdioIsValidBus(uint8_t sdio);
static uint32_t bspSdioGetLoopCount(uint32_t timeoutMs);
static void bspSdioHwInit(void);
static void bspSdioSetClock(uint8_t clockDiv, uint32_t busWidth);
static bool bspSdioIsCardPresent(void);
static void bspSdioClearStaticFlags(void);
static eDrvStatus bspSdioWaitFlags(uint32_t waitMask, uint32_t errorMask, uint32_t timeoutMs, uint32_t *flags);
static eDrvStatus bspSdioSendCommand(uint8_t cmdIndex, uint32_t argument, eBspSdioRespType respType, uint32_t *response, uint32_t timeoutMs);
static eDrvStatus bspSdioSendAppCommand(stBspSdioContext *context, uint8_t acmdIndex, uint32_t argument, eBspSdioRespType respType, uint32_t *response, uint32_t timeoutMs);
static bool bspSdioRespHasError(uint32_t response);
static eDrvStatus bspSdioCardPowerOn(stBspSdioContext *context, uint32_t timeoutMs);
static eDrvStatus bspSdioCardReadCidCsd(stBspSdioContext *context, uint32_t timeoutMs);
static eDrvStatus bspSdioSelectCard(stBspSdioContext *context, FunctionalState enable, uint32_t timeoutMs);
static eDrvStatus bspSdioSetBlockLength(stBspSdioContext *context, uint32_t blockLength, uint32_t timeoutMs);
static eDrvStatus bspSdioSetWideBus(stBspSdioContext *context, uint32_t timeoutMs);
static uint32_t bspSdioExtractBits(const uint32_t *words, uint8_t msb, uint8_t lsb);
static void bspSdioUpdateCardInfo(stBspSdioContext *context);
static uint32_t bspSdioGetAddress(const stBspSdioContext *context, uint32_t block);
static eDrvStatus bspSdioReadSingleBlock(stBspSdioContext *context, uint32_t block, uint8_t *buffer);
static eDrvStatus bspSdioWriteSingleBlock(stBspSdioContext *context, uint32_t block, const uint8_t *buffer);
static eDrvStatus bspSdioWaitCardState(stBspSdioContext *context, uint32_t expectedState, uint32_t timeoutMs);

static bool bspSdioIsValidBus(uint8_t sdio)
{
    return (sdio < BSP_SDIO_DEV_MAX);
}

static uint32_t bspSdioGetLoopCount(uint32_t timeoutMs)
{
    uint32_t lLoops;

    lLoops = (timeoutMs == 0U) ? BSP_SDIO_CMD_TIMEOUT_LOOP_PER_MS : (timeoutMs * BSP_SDIO_CMD_TIMEOUT_LOOP_PER_MS);
    if (lLoops == 0U) {
        lLoops = 1U;
    }

    return lLoops;
}

static void bspSdioHwInit(void)
{
    GPIO_InitTypeDef lGpioInit;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SDIO, ENABLE);

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource8, GPIO_AF_SDIO);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource9, GPIO_AF_SDIO);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_SDIO);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_SDIO);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource12, GPIO_AF_SDIO);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource2, GPIO_AF_SDIO);

    GPIO_StructInit(&lGpioInit);
    lGpioInit.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
    lGpioInit.GPIO_Mode = GPIO_Mode_AF;
    lGpioInit.GPIO_OType = GPIO_OType_PP;
    lGpioInit.GPIO_PuPd = GPIO_PuPd_UP;
    lGpioInit.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOC, &lGpioInit);

    lGpioInit.GPIO_Pin = GPIO_Pin_2;
    GPIO_Init(GPIOD, &lGpioInit);

    GPIO_StructInit(&lGpioInit);
    lGpioInit.GPIO_Pin = GPIO_Pin_15;
    lGpioInit.GPIO_Mode = GPIO_Mode_IN;
    lGpioInit.GPIO_PuPd = GPIO_PuPd_UP;
    lGpioInit.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &lGpioInit);

    SDIO_DeInit();
    SDIO_SetPowerState(SDIO_PowerState_OFF);
    SDIO_ClockCmd(DISABLE);
    bspSdioSetClock(BSP_SDIO_INIT_CLK_DIV, SDIO_BusWide_1b);
    SDIO_SetPowerState(SDIO_PowerState_ON);
    SDIO_ClockCmd(ENABLE);
}

static void bspSdioSetClock(uint8_t clockDiv, uint32_t busWidth)
{
    SDIO_InitTypeDef lSdioInit;

    SDIO_StructInit(&lSdioInit);
    lSdioInit.SDIO_ClockDiv = clockDiv;
    lSdioInit.SDIO_ClockEdge = SDIO_ClockEdge_Rising;
    lSdioInit.SDIO_ClockBypass = SDIO_ClockBypass_Disable;
    lSdioInit.SDIO_ClockPowerSave = SDIO_ClockPowerSave_Disable;
    lSdioInit.SDIO_BusWide = busWidth;
    lSdioInit.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Disable;
    SDIO_Init(&lSdioInit);
}

static bool bspSdioIsCardPresent(void)
{
    return (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_15) == Bit_RESET);
}

static void bspSdioClearStaticFlags(void)
{
    SDIO_ClearFlag(SDIO_FLAG_CCRCFAIL |
                   SDIO_FLAG_DCRCFAIL |
                   SDIO_FLAG_CTIMEOUT |
                   SDIO_FLAG_DTIMEOUT |
                   SDIO_FLAG_TXUNDERR |
                   SDIO_FLAG_RXOVERR |
                   SDIO_FLAG_CMDREND |
                   SDIO_FLAG_CMDSENT |
                   SDIO_FLAG_DATAEND |
                   SDIO_FLAG_STBITERR |
                   SDIO_FLAG_DBCKEND);
}

static eDrvStatus bspSdioWaitFlags(uint32_t waitMask, uint32_t errorMask, uint32_t timeoutMs, uint32_t *flags)
{
    uint32_t lLoops;
    uint32_t lFlags;

    lLoops = bspSdioGetLoopCount(timeoutMs);
    while (lLoops > 0U) {
        lFlags = SDIO->STA;
        if ((lFlags & errorMask) != 0U) {
            if (flags != NULL) {
                *flags = lFlags;
            }

            if ((lFlags & (SDIO_FLAG_CTIMEOUT | SDIO_FLAG_DTIMEOUT)) != 0U) {
                return DRV_STATUS_TIMEOUT;
            }

            if ((lFlags & (SDIO_FLAG_CCRCFAIL | SDIO_FLAG_DCRCFAIL)) != 0U) {
                return DRV_STATUS_NACK;
            }

            return DRV_STATUS_ERROR;
        }

        if ((lFlags & waitMask) != 0U) {
            if (flags != NULL) {
                *flags = lFlags;
            }
            return DRV_STATUS_OK;
        }

        lLoops--;
    }

    if (flags != NULL) {
        *flags = SDIO->STA;
    }
    return DRV_STATUS_TIMEOUT;
}

static eDrvStatus bspSdioSendCommand(uint8_t cmdIndex, uint32_t argument, eBspSdioRespType respType, uint32_t *response, uint32_t timeoutMs)
{
    SDIO_CmdInitTypeDef lCmdInit;
    eDrvStatus lStatus;
    uint32_t lFlags;
    uint32_t lWaitMask;
    uint32_t lErrorMask;

    bspSdioClearStaticFlags();
    SDIO_CmdStructInit(&lCmdInit);
    lCmdInit.SDIO_Argument = argument;
    lCmdInit.SDIO_CmdIndex = cmdIndex;
    lCmdInit.SDIO_Wait = SDIO_Wait_No;
    lCmdInit.SDIO_CPSM = SDIO_CPSM_Enable;

    if (respType == BSP_SDIO_RESP_NONE) {
        lCmdInit.SDIO_Response = SDIO_Response_No;
        lWaitMask = SDIO_FLAG_CMDSENT;
        lErrorMask = 0U;
    } else if (respType == BSP_SDIO_RESP_LONG) {
        lCmdInit.SDIO_Response = SDIO_Response_Long;
        lWaitMask = SDIO_FLAG_CMDREND;
        lErrorMask = SDIO_FLAG_CTIMEOUT | SDIO_FLAG_CCRCFAIL;
    } else {
        lCmdInit.SDIO_Response = SDIO_Response_Short;
        lWaitMask = SDIO_FLAG_CMDREND;
        lErrorMask = SDIO_FLAG_CTIMEOUT;
        if (respType == BSP_SDIO_RESP_SHORT) {
            lErrorMask |= SDIO_FLAG_CCRCFAIL;
        } else {
            lWaitMask |= SDIO_FLAG_CCRCFAIL;
        }
    }

    SDIO_SendCommand(&lCmdInit);
    lStatus = bspSdioWaitFlags(lWaitMask, lErrorMask, timeoutMs, &lFlags);
    if (lStatus != DRV_STATUS_OK) {
        bspSdioClearStaticFlags();
        return lStatus;
    }

    if ((respType != BSP_SDIO_RESP_NONE) && (SDIO_GetCommandResponse() != cmdIndex)) {
        bspSdioClearStaticFlags();
        return DRV_STATUS_ID_NOTMATCH;
    }

    if (response != NULL) {
        if (respType == BSP_SDIO_RESP_LONG) {
            response[0] = SDIO_GetResponse(SDIO_RESP1);
            response[1] = SDIO_GetResponse(SDIO_RESP2);
            response[2] = SDIO_GetResponse(SDIO_RESP3);
            response[3] = SDIO_GetResponse(SDIO_RESP4);
        } else if (respType != BSP_SDIO_RESP_NONE) {
            response[0] = SDIO_GetResponse(SDIO_RESP1);
        }
    }

    bspSdioClearStaticFlags();
    (void)lFlags;
    return DRV_STATUS_OK;
}

static eDrvStatus bspSdioSendAppCommand(stBspSdioContext *context, uint8_t acmdIndex, uint32_t argument, eBspSdioRespType respType, uint32_t *response, uint32_t timeoutMs)
{
    uint32_t lResponse[4] = {0U};
    eDrvStatus lStatus;

    lStatus = bspSdioSendCommand(55U,
                                 ((uint32_t)context->rca << 16),
                                 BSP_SDIO_RESP_SHORT,
                                 lResponse,
                                 timeoutMs);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    if (bspSdioRespHasError(lResponse[0])) {
        return DRV_STATUS_ERROR;
    }

    return bspSdioSendCommand(acmdIndex, argument, respType, response, timeoutMs);
}

static bool bspSdioRespHasError(uint32_t response)
{
    return ((response & BSP_SDIO_R1_ERROR_MASK) != 0U);
}

static eDrvStatus bspSdioCardPowerOn(stBspSdioContext *context, uint32_t timeoutMs)
{
    uint32_t lResponse[4] = {0U};
    eDrvStatus lStatus;
    uint32_t lLoops;
    uint8_t lIsV2;

    lIsV2 = 0U;

    lLoops = bspSdioGetLoopCount(1U);
    while (lLoops > 0U) {
        lLoops--;
    }

    lStatus = bspSdioSendCommand(0U, 0U, BSP_SDIO_RESP_NONE, NULL, timeoutMs);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = bspSdioSendCommand(8U, 0x1AAU, BSP_SDIO_RESP_SHORT, lResponse, timeoutMs);
    if ((lStatus == DRV_STATUS_OK) && (lResponse[0] == 0x000001AAUL)) {
        lIsV2 = 1U;
    }

    lLoops = bspSdioGetLoopCount(timeoutMs);
    while (lLoops > 0U) {
        lStatus = bspSdioSendAppCommand(context,
                                        41U,
                                        (lIsV2 != 0U) ? BSP_SDIO_ACMD41_ARG : (BSP_SDIO_ACMD41_ARG & 0x00FFFFFFUL),
                                        BSP_SDIO_RESP_SHORT_NOCRC,
                                        lResponse,
                                        timeoutMs);
        if (lStatus != DRV_STATUS_OK) {
            return lStatus;
        }

        if ((lResponse[0] & 0x80000000UL) != 0U) {
            context->isHighCapacity = ((lResponse[0] & 0x40000000UL) != 0U) ? 1U : 0U;
            return DRV_STATUS_OK;
        }

        lLoops--;
    }

    return DRV_STATUS_TIMEOUT;
}

static eDrvStatus bspSdioCardReadCidCsd(stBspSdioContext *context, uint32_t timeoutMs)
{
    uint32_t lResponse[4] = {0U};
    eDrvStatus lStatus;

    lStatus = bspSdioSendCommand(2U, 0U, BSP_SDIO_RESP_LONG, context->cid, timeoutMs);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = bspSdioSendCommand(3U, 0U, BSP_SDIO_RESP_SHORT, lResponse, timeoutMs);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    if (bspSdioRespHasError(lResponse[0])) {
        return DRV_STATUS_ERROR;
    }

    context->rca = (uint16_t)(lResponse[0] >> 16);
    if (context->rca == 0U) {
        return DRV_STATUS_ERROR;
    }

    lStatus = bspSdioSendCommand(9U,
                                 ((uint32_t)context->rca << 16),
                                 BSP_SDIO_RESP_LONG,
                                 context->csd,
                                 timeoutMs);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return DRV_STATUS_OK;
}

static eDrvStatus bspSdioSelectCard(stBspSdioContext *context, FunctionalState enable, uint32_t timeoutMs)
{
    uint32_t lResponse[4] = {0U};
    uint32_t lArgument;
    eDrvStatus lStatus;

    lArgument = (enable != DISABLE) ? ((uint32_t)context->rca << 16) : 0U;
    lStatus = bspSdioSendCommand(7U, lArgument, BSP_SDIO_RESP_SHORT, lResponse, timeoutMs);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return bspSdioRespHasError(lResponse[0]) ? DRV_STATUS_ERROR : DRV_STATUS_OK;
}

static eDrvStatus bspSdioSetBlockLength(stBspSdioContext *context, uint32_t blockLength, uint32_t timeoutMs)
{
    uint32_t lResponse[4] = {0U};
    eDrvStatus lStatus;

    (void)context;
    lStatus = bspSdioSendCommand(16U, blockLength, BSP_SDIO_RESP_SHORT, lResponse, timeoutMs);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return bspSdioRespHasError(lResponse[0]) ? DRV_STATUS_ERROR : DRV_STATUS_OK;
}

static eDrvStatus bspSdioSetWideBus(stBspSdioContext *context, uint32_t timeoutMs)
{
    uint32_t lResponse[4] = {0U};
    eDrvStatus lStatus;

    lStatus = bspSdioSendAppCommand(context, 6U, 2U, BSP_SDIO_RESP_SHORT, lResponse, timeoutMs);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    if (bspSdioRespHasError(lResponse[0])) {
        return DRV_STATUS_ERROR;
    }

    bspSdioSetClock(BSP_SDIO_TRANSFER_CLK_DIV, SDIO_BusWide_4b);
    return DRV_STATUS_OK;
}

static uint32_t bspSdioExtractBits(const uint32_t *words, uint8_t msb, uint8_t lsb)
{
    uint32_t lValue;
    uint8_t lBit;
    uint8_t lWordIndex;
    uint8_t lBitIndex;

    lValue = 0U;
    for (lBit = lsb; lBit <= msb; lBit++) {
        lWordIndex = (uint8_t)(3U - (lBit / 32U));
        lBitIndex = (uint8_t)(lBit % 32U);
        lValue |= (((words[lWordIndex] >> lBitIndex) & 0x1U) << (lBit - lsb));
    }

    return lValue;
}

static void bspSdioUpdateCardInfo(stBspSdioContext *context)
{
    uint32_t lCsdStructure;
    uint32_t lReadBlockLen;
    uint32_t lCSize;
    uint32_t lCSizeMult;
    uint64_t lCapacityBytes;
    uint32_t lBlockCount;

    context->info.blockSize = BSP_SDIO_BLOCK_SIZE;
    context->info.eraseBlockSize = BSP_SDIO_BLOCK_SIZE;
    context->info.isPresent = bspSdioIsCardPresent();
    context->info.isWriteProtected = false;
    context->info.isHighCapacity = (context->isHighCapacity != 0U);

    lCsdStructure = bspSdioExtractBits(context->csd, 127U, 126U);
    if (lCsdStructure == 1U) {
        lCSize = bspSdioExtractBits(context->csd, 69U, 48U);
        lBlockCount = (lCSize + 1U) * 1024U;
        context->info.blockCount = lBlockCount;
        context->info.capacityBytes = (uint64_t)lBlockCount * (uint64_t)BSP_SDIO_BLOCK_SIZE;
        return;
    }

    lReadBlockLen = bspSdioExtractBits(context->csd, 83U, 80U);
    lCSize = bspSdioExtractBits(context->csd, 73U, 62U);
    lCSizeMult = bspSdioExtractBits(context->csd, 49U, 47U);
    lCapacityBytes = (uint64_t)(lCSize + 1U) * (uint64_t)(1UL << (lCSizeMult + 2U)) * (uint64_t)(1UL << lReadBlockLen);
    context->info.capacityBytes = lCapacityBytes;
    context->info.blockCount = (uint32_t)(lCapacityBytes / (uint64_t)BSP_SDIO_BLOCK_SIZE);
}

static uint32_t bspSdioGetAddress(const stBspSdioContext *context, uint32_t block)
{
    return (context->isHighCapacity != 0U) ? block : (block * BSP_SDIO_BLOCK_SIZE);
}

static eDrvStatus bspSdioReadSingleBlock(stBspSdioContext *context, uint32_t block, uint8_t *buffer)
{
    SDIO_DataInitTypeDef lDataInit;
    uint32_t lResponse[4] = {0U};
    uint32_t lFlags;
    uint32_t lLoops;
    uint32_t lWord;
    uint32_t lOffset;
    eDrvStatus lStatus;

    lOffset = 0U;
    bspSdioClearStaticFlags();
    SDIO_DataStructInit(&lDataInit);
    lDataInit.SDIO_DataTimeOut = BSP_SDIO_DATA_TIMEOUT;
    lDataInit.SDIO_DataLength = BSP_SDIO_BLOCK_SIZE;
    lDataInit.SDIO_DataBlockSize = SDIO_DataBlockSize_512b;
    lDataInit.SDIO_TransferDir = SDIO_TransferDir_ToSDIO;
    lDataInit.SDIO_TransferMode = SDIO_TransferMode_Block;
    lDataInit.SDIO_DPSM = SDIO_DPSM_Enable;
    SDIO_DataConfig(&lDataInit);

    lStatus = bspSdioSendCommand(17U,
                                 bspSdioGetAddress(context, block),
                                 BSP_SDIO_RESP_SHORT,
                                 lResponse,
                                 100U);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    if (bspSdioRespHasError(lResponse[0])) {
        return DRV_STATUS_ERROR;
    }

    lLoops = bspSdioGetLoopCount(100U);
    while (lOffset < BSP_SDIO_BLOCK_SIZE) {
        lFlags = SDIO->STA;
        if ((lFlags & (SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_RXOVERR | SDIO_FLAG_STBITERR)) != 0U) {
            bspSdioClearStaticFlags();
            if ((lFlags & SDIO_FLAG_DTIMEOUT) != 0U) {
                return DRV_STATUS_TIMEOUT;
            }
            if ((lFlags & SDIO_FLAG_DCRCFAIL) != 0U) {
                return DRV_STATUS_NACK;
            }
            return DRV_STATUS_ERROR;
        }

        if ((lFlags & SDIO_FLAG_RXDAVL) != 0U) {
            lWord = SDIO_ReadData();
            buffer[lOffset] = (uint8_t)(lWord & 0xFFU);
            buffer[lOffset + 1U] = (uint8_t)((lWord >> 8) & 0xFFU);
            buffer[lOffset + 2U] = (uint8_t)((lWord >> 16) & 0xFFU);
            buffer[lOffset + 3U] = (uint8_t)((lWord >> 24) & 0xFFU);
            lOffset += 4U;
            lLoops = bspSdioGetLoopCount(100U);
        } else {
            if (lLoops == 0U) {
                bspSdioClearStaticFlags();
                return DRV_STATUS_TIMEOUT;
            }
            lLoops--;
        }
    }

    lStatus = bspSdioWaitFlags(SDIO_FLAG_DBCKEND,
                               SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_RXOVERR | SDIO_FLAG_STBITERR,
                               100U,
                               NULL);
    bspSdioClearStaticFlags();
    return lStatus;
}

static eDrvStatus bspSdioWriteSingleBlock(stBspSdioContext *context, uint32_t block, const uint8_t *buffer)
{
    SDIO_DataInitTypeDef lDataInit;
    uint32_t lResponse[4] = {0U};
    uint32_t lFlags;
    uint32_t lLoops;
    uint32_t lWord;
    uint32_t lOffset;
    eDrvStatus lStatus;

    lOffset = 0U;
    bspSdioClearStaticFlags();
    SDIO_DataStructInit(&lDataInit);
    lDataInit.SDIO_DataTimeOut = BSP_SDIO_DATA_TIMEOUT;
    lDataInit.SDIO_DataLength = BSP_SDIO_BLOCK_SIZE;
    lDataInit.SDIO_DataBlockSize = SDIO_DataBlockSize_512b;
    lDataInit.SDIO_TransferDir = SDIO_TransferDir_ToCard;
    lDataInit.SDIO_TransferMode = SDIO_TransferMode_Block;
    lDataInit.SDIO_DPSM = SDIO_DPSM_Enable;
    SDIO_DataConfig(&lDataInit);

    lStatus = bspSdioSendCommand(24U,
                                 bspSdioGetAddress(context, block),
                                 BSP_SDIO_RESP_SHORT,
                                 lResponse,
                                 100U);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    if (bspSdioRespHasError(lResponse[0])) {
        return DRV_STATUS_ERROR;
    }

    lLoops = bspSdioGetLoopCount(100U);
    while (lOffset < BSP_SDIO_BLOCK_SIZE) {
        lFlags = SDIO->STA;
        if ((lFlags & (SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_TXUNDERR | SDIO_FLAG_STBITERR)) != 0U) {
            bspSdioClearStaticFlags();
            if ((lFlags & SDIO_FLAG_DTIMEOUT) != 0U) {
                return DRV_STATUS_TIMEOUT;
            }
            if ((lFlags & SDIO_FLAG_DCRCFAIL) != 0U) {
                return DRV_STATUS_NACK;
            }
            return DRV_STATUS_ERROR;
        }

        if ((lFlags & SDIO_FLAG_TXFIFOHE) != 0U) {
            lWord = (uint32_t)buffer[lOffset] |
                    ((uint32_t)buffer[lOffset + 1U] << 8) |
                    ((uint32_t)buffer[lOffset + 2U] << 16) |
                    ((uint32_t)buffer[lOffset + 3U] << 24);
            SDIO_WriteData(lWord);
            lOffset += 4U;
            lLoops = bspSdioGetLoopCount(100U);
        } else {
            if (lLoops == 0U) {
                bspSdioClearStaticFlags();
                return DRV_STATUS_TIMEOUT;
            }
            lLoops--;
        }
    }

    lStatus = bspSdioWaitFlags(SDIO_FLAG_DBCKEND,
                               SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_TXUNDERR | SDIO_FLAG_STBITERR,
                               200U,
                               NULL);
    bspSdioClearStaticFlags();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return bspSdioWaitCardState(context, BSP_SDIO_CARD_STATE_TRAN, 200U);
}

static eDrvStatus bspSdioWaitCardState(stBspSdioContext *context, uint32_t expectedState, uint32_t timeoutMs)
{
    uint32_t lResponse[4] = {0U};
    uint32_t lLoops;
    eDrvStatus lStatus;

    lLoops = bspSdioGetLoopCount(timeoutMs);
    while (lLoops > 0U) {
        lStatus = bspSdioSendCommand(13U,
                                     ((uint32_t)context->rca << 16),
                                     BSP_SDIO_RESP_SHORT,
                                     lResponse,
                                     timeoutMs);
        if (lStatus != DRV_STATUS_OK) {
            return lStatus;
        }

        if (!bspSdioRespHasError(lResponse[0]) && (((lResponse[0] >> 9) & 0xFU) == expectedState)) {
            return DRV_STATUS_OK;
        }

        lLoops--;
    }

    return DRV_STATUS_TIMEOUT;
}

eDrvStatus bspSdioInit(uint8_t sdio, uint32_t timeoutMs)
{
    stBspSdioContext *lContext;
    eDrvStatus lStatus;

    if (!bspSdioIsValidBus(sdio)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lContext = &gBspSdioContext[sdio];
    lContext->isInitialized = 0U;
    lContext->isHighCapacity = 0U;
    lContext->rca = 0U;
    bspSdioHwInit();
    lContext->info.blockSize = BSP_SDIO_BLOCK_SIZE;
    lContext->info.blockCount = 0U;
    lContext->info.eraseBlockSize = BSP_SDIO_BLOCK_SIZE;
    lContext->info.capacityBytes = 0ULL;
    lContext->info.isPresent = bspSdioIsCardPresent();
    lContext->info.isWriteProtected = false;
    lContext->info.isHighCapacity = false;

    if (!lContext->info.isPresent) {
        lContext->isInitialized = 1U;
        return DRV_STATUS_OK;
    }

    lStatus = bspSdioCardPowerOn(lContext, timeoutMs);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = bspSdioCardReadCidCsd(lContext, timeoutMs);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = bspSdioSelectCard(lContext, ENABLE, timeoutMs);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = bspSdioSetWideBus(lContext, timeoutMs);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    if (lContext->isHighCapacity == 0U) {
        lStatus = bspSdioSetBlockLength(lContext, BSP_SDIO_BLOCK_SIZE, timeoutMs);
        if (lStatus != DRV_STATUS_OK) {
            return lStatus;
        }
    }

    bspSdioUpdateCardInfo(lContext);
    lContext->isInitialized = 1U;
    return DRV_STATUS_OK;
}

eDrvStatus bspSdioGetStatus(uint8_t sdio, bool *isPresent, bool *isWriteProtected)
{
    bool lPresent;

    if (!bspSdioIsValidBus(sdio)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lPresent = bspSdioIsCardPresent();
    gBspSdioContext[sdio].info.isPresent = lPresent;
    gBspSdioContext[sdio].info.isWriteProtected = false;

    if (isPresent != NULL) {
        *isPresent = lPresent;
    }

    if (isWriteProtected != NULL) {
        *isWriteProtected = false;
    }

    return DRV_STATUS_OK;
}

eDrvStatus bspSdioReadBlocks(uint8_t sdio, uint32_t startBlock, uint8_t *buffer, uint32_t blockCount)
{
    stBspSdioContext *lContext;
    uint32_t lIndex;
    eDrvStatus lStatus;

    if (!bspSdioIsValidBus(sdio) || (buffer == NULL) || (blockCount == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lContext = &gBspSdioContext[sdio];
    if ((lContext->isInitialized == 0U) || !bspSdioIsCardPresent()) {
        return (eDrvStatus)SDCARD_STATUS_NO_MEDIUM;
    }

    for (lIndex = 0U; lIndex < blockCount; lIndex++) {
        lStatus = bspSdioReadSingleBlock(lContext, startBlock + lIndex, &buffer[lIndex * BSP_SDIO_BLOCK_SIZE]);
        if (lStatus != DRV_STATUS_OK) {
            return lStatus;
        }
    }

    return DRV_STATUS_OK;
}

eDrvStatus bspSdioWriteBlocks(uint8_t sdio, uint32_t startBlock, const uint8_t *buffer, uint32_t blockCount)
{
    stBspSdioContext *lContext;
    uint32_t lIndex;
    eDrvStatus lStatus;

    if (!bspSdioIsValidBus(sdio) || (buffer == NULL) || (blockCount == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lContext = &gBspSdioContext[sdio];
    if ((lContext->isInitialized == 0U) || !bspSdioIsCardPresent()) {
        return (eDrvStatus)SDCARD_STATUS_NO_MEDIUM;
    }

    for (lIndex = 0U; lIndex < blockCount; lIndex++) {
        lStatus = bspSdioWriteSingleBlock(lContext, startBlock + lIndex, &buffer[lIndex * BSP_SDIO_BLOCK_SIZE]);
        if (lStatus != DRV_STATUS_OK) {
            return lStatus;
        }
    }

    return DRV_STATUS_OK;
}

eDrvStatus bspSdioIoctl(uint8_t sdio, uint32_t command, void *buffer)
{
    stBspSdioContext *lContext;

    if (!bspSdioIsValidBus(sdio)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lContext = &gBspSdioContext[sdio];
    if ((lContext->isInitialized == 0U) || !bspSdioIsCardPresent()) {
        return (eDrvStatus)SDCARD_STATUS_NO_MEDIUM;
    }

    if (command == (uint32_t)eSDCARD_IOCTL_GET_INFO) {
        stSdcardInfo *lInfo;

        if (buffer == NULL) {
            return DRV_STATUS_INVALID_PARAM;
        }

        lInfo = (stSdcardInfo *)buffer;
        *lInfo = lContext->info;
        lInfo->isPresent = true;
        lInfo->isWriteProtected = false;
        return DRV_STATUS_OK;
    }

    if (command == (uint32_t)eSDCARD_IOCTL_SYNC) {
        return DRV_STATUS_OK;
    }

    if (command == (uint32_t)eSDCARD_IOCTL_TRIM) {
        (void)buffer;
        return DRV_STATUS_UNSUPPORTED;
    }

    return DRV_STATUS_UNSUPPORTED;
}

/**************************End of file********************************/
