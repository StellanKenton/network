/***********************************************************************************
* @file     : fc41dport.c
* @brief    : Project-side FC41D binding implementation.
* @details  : Provides FC41D default assembly, transport binding, raw BLE frame
*             collection, and boot-ready detection for the current project.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "fc41dport.h"

#include <ctype.h>
#include <string.h>

#include "usart.h"

#include "drvgpio.h"
#include "drvgpio_port.h"
#include "drvuart.h"

#include "../../../rep/module/fc41d/fc41d_ble.h"
#include "../../../rep/service/console/log.h"
#include "../../../rep/service/rtos/rtos.h"
#include "../../../rep/tools/ringbuffer/ringbuffer.h"
#include "drvuart_port.h"

#define FC41DPORT_LOG_TAG              "fc41dport"
#define FC41DPORT_RESET_ASSERT_MS      20U
#define FC41DPORT_RESET_RELEASE_MS     200U
#define FC41DPORT_AT_RX_CAPACITY       256U
#define FC41DPORT_AT_LINE_BUF_SIZE     128U
#define FC41DPORT_AT_CMD_BUF_SIZE      128U
#define FC41DPORT_AT_PAYLOAD_BUF_SIZE  128U
#define FC41DPORT_BLE_RX_CAPACITY      512U
#define FC41DPORT_BLE_FRAME_MAX_LEN    136U
#define FC41DPORT_UNUSED_RX_CAPACITY   1U

static const char *const gFc41dPortBleInitCmdSeq[] = {
    "AT+QBLEINIT=2",
    "AT+QBLENAME=rumi",
    "AT+QBLEGATTSSRV=FE60",
    "AT+QBLEGATTSCHAR=FE61",
    "AT+QBLEGATTSCHAR=FE62",
};

static bool gFc41dPortTransportPrepared = false;
static bool gFc41dPortBootReady = false;
static uint8_t gFc41dPortBootReadyMatchLen = 0U;
static uint32_t gFc41dPortBootWaitStartTick = 0U;
static stRingBuffer gFc41dPortAtRxRb;
static uint8_t gFc41dPortAtRxStorage[FC41DPORT_AT_RX_CAPACITY];
static uint8_t gFc41dPortAtLineBuf[FC41DPORT_AT_LINE_BUF_SIZE];
static uint8_t gFc41dPortAtCmdBuf[FC41DPORT_AT_CMD_BUF_SIZE];
static uint8_t gFc41dPortAtPayloadBuf[FC41DPORT_AT_PAYLOAD_BUF_SIZE];
static uint8_t gFc41dPortBleRxStorage[FC41DPORT_BLE_RX_CAPACITY];
static uint8_t gFc41dPortUnusedRxStorage[FC41DPORT_UNUSED_RX_CAPACITY];
static uint8_t gFc41dPortBleFrameBuf[FC41DPORT_BLE_FRAME_MAX_LEN];
static uint16_t gFc41dPortBleFrameFill = 0U;
static uint16_t gFc41dPortBleFrameExpectedLen = 0U;

static void fc41dPortDelayMs(uint32_t delayMs);
static void fc41dPortResetBleFrameCollector(void);
static bool fc41dPortCollectRawBleByte(eFc41dMapType device, uint8_t byte);
static bool fc41dPortConsumeBootReadyByte(uint8_t byte);
static eDrvStatus fc41dPortSend(const uint8_t *buf, uint16_t len, void *userCtx);
static bool fc41dPortRouteLinePayload(const uint8_t *lineBuf, uint16_t lineLen,
                                      const uint8_t **payloadBuf, uint16_t *payloadLen);

static void fc41dPortDelayMs(uint32_t delayMs)
{
    if (delayMs == 0U) {
        return;
    }

    if (repRtosIsSchedulerRunning()) {
        (void)repRtosDelayMs(delayMs);
        return;
    }

    HAL_Delay(delayMs);
}

static void fc41dPortResetBleFrameCollector(void)
{
    gFc41dPortBleFrameFill = 0U;
    gFc41dPortBleFrameExpectedLen = 0U;
}

static bool fc41dPortCollectRawBleByte(eFc41dMapType device, uint8_t byte)
{
    uint16_t lPayloadLen;
    stRingBuffer *lBleRxRb;

    if (gFc41dPortBleFrameFill == 0U) {
        if (byte != 0xFAU) {
            return false;
        }

        gFc41dPortBleFrameBuf[0] = byte;
        gFc41dPortBleFrameFill = 1U;
        gFc41dPortBleFrameExpectedLen = 0U;
        return true;
    }

    if (gFc41dPortBleFrameFill >= sizeof(gFc41dPortBleFrameBuf)) {
        fc41dPortResetBleFrameCollector();
        return false;
    }

    gFc41dPortBleFrameBuf[gFc41dPortBleFrameFill++] = byte;
    if (gFc41dPortBleFrameFill == 2U) {
        if (gFc41dPortBleFrameBuf[1] != 0xFCU) {
            fc41dPortResetBleFrameCollector();
            if (byte == 0xFAU) {
                gFc41dPortBleFrameBuf[0] = byte;
                gFc41dPortBleFrameFill = 1U;
                return true;
            }
            return false;
        }
        return true;
    }

    if (gFc41dPortBleFrameFill == 6U) {
        lPayloadLen = (uint16_t)(((uint16_t)gFc41dPortBleFrameBuf[4] << 8U) | gFc41dPortBleFrameBuf[5]);
        gFc41dPortBleFrameExpectedLen = (uint16_t)(lPayloadLen + 8U);
        if ((gFc41dPortBleFrameExpectedLen < 8U) || (gFc41dPortBleFrameExpectedLen > sizeof(gFc41dPortBleFrameBuf))) {
            fc41dPortResetBleFrameCollector();
            return false;
        }
    }

    if ((gFc41dPortBleFrameExpectedLen != 0U) && (gFc41dPortBleFrameFill >= gFc41dPortBleFrameExpectedLen)) {
        lBleRxRb = fc41dBleGetRxRingBuffer(device);
        if ((lBleRxRb == NULL) ||
            (ringBufferWrite(lBleRxRb, gFc41dPortBleFrameBuf, gFc41dPortBleFrameExpectedLen) != gFc41dPortBleFrameExpectedLen)) {
            LOG_W(FC41DPORT_LOG_TAG, "ble raw frame drop len=%u", (unsigned int)gFc41dPortBleFrameExpectedLen);
        }
        fc41dPortResetBleFrameCollector();
    }

    return true;
}

static bool fc41dPortConsumeBootReadyByte(uint8_t byte)
{
    static const char lReadyToken[] = "ready";
    uint8_t lLowerByte;

    lLowerByte = (uint8_t)tolower((int)byte);
    if (lLowerByte == (uint8_t)lReadyToken[gFc41dPortBootReadyMatchLen]) {
        gFc41dPortBootReadyMatchLen++;
    } else if (lLowerByte == (uint8_t)lReadyToken[0]) {
        gFc41dPortBootReadyMatchLen = 1U;
    } else {
        gFc41dPortBootReadyMatchLen = 0U;
    }

    if (gFc41dPortBootReadyMatchLen >= (uint8_t)(sizeof(lReadyToken) - 1U)) {
        gFc41dPortBootReadyMatchLen = 0U;
        return true;
    }

    return false;
}

static eDrvStatus fc41dPortSend(const uint8_t *buf, uint16_t len, void *userCtx)
{
    (void)userCtx;

    if ((buf == NULL) || (len == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvUartTransmit(DRVUART_DEBUG, buf, len, 100U);
}

static bool fc41dPortRouteLinePayload(const uint8_t *lineBuf, uint16_t lineLen,
                                      const uint8_t **payloadBuf, uint16_t *payloadLen)
{
    uint16_t lIndex;

    if ((lineBuf == NULL) || (payloadBuf == NULL) || (payloadLen == NULL)) {
        return false;
    }

    for (lIndex = 0U; lIndex < lineLen; lIndex++) {
        if (lineBuf[lIndex] == ':') {
            lIndex++;
            while ((lIndex < lineLen) && ((lineBuf[lIndex] == ' ') || (lineBuf[lIndex] == '\t'))) {
                lIndex++;
            }
            break;
        }
    }

    if (lIndex >= lineLen) {
        *payloadBuf = lineBuf;
        *payloadLen = lineLen;
        return true;
    }

    *payloadBuf = &lineBuf[lIndex];
    *payloadLen = (uint16_t)(lineLen - lIndex);
    return true;
}

void fc41dPortResetBootWaitState(void)
{
    gFc41dPortBootReady = false;
    gFc41dPortBootReadyMatchLen = 0U;
    gFc41dPortBootWaitStartTick = fc41dPlatformGetTickMs();
    fc41dPortResetBleFrameCollector();
}

eFc41dPortBootState fc41dPortPollBootReady(eFc41dMapType device, uint32_t timeoutMs)
{
    uint8_t lByte;

    if (gFc41dPortBootReady) {
        return FC41DPORT_BOOT_READY;
    }

    fc41dPlatformPollRx(device);
    while (ringBufferRead(&gFc41dPortAtRxRb, &lByte, 1U) == 1U) {
        if (fc41dPortConsumeBootReadyByte(lByte)) {
            gFc41dPortBootReady = true;
            LOG_I(FC41DPORT_LOG_TAG, "fc41d boot ready");
            return FC41DPORT_BOOT_READY;
        }
    }

    if ((timeoutMs > 0U) && ((uint32_t)(fc41dPlatformGetTickMs() - gFc41dPortBootWaitStartTick) >= timeoutMs)) {
        return FC41DPORT_BOOT_TIMEOUT;
    }

    return FC41DPORT_BOOT_WAITING;
}

void fc41dLoadPlatformDefaultCfg(eFc41dMapType device, stFc41dCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->ble.enableRx = true;
    cfg->ble.rxOverwriteOnFull = true;
    cfg->ble.workMode = FC41D_BLE_WORK_MODE_PERIPHERAL;
    cfg->ble.initCmdSeq = gFc41dPortBleInitCmdSeq;
    cfg->ble.initCmdSeqLen = (uint8_t)(sizeof(gFc41dPortBleInitCmdSeq) / sizeof(gFc41dPortBleInitCmdSeq[0]));
    cfg->wifi.enableRx = false;
    cfg->wifi.rxOverwriteOnFull = false;
    cfg->wifi.workMode = FC41D_WIFI_WORK_MODE_DISABLED;
    cfg->execGuardMs = 5000U;
    cfg->bootMode = FC41D_MODE_COMMAND;
}

bool fc41dPlatformIsValidAssemble(eFc41dMapType device)
{
    (void)device;
    return huart4.Instance != NULL;
}

uint32_t fc41dPlatformGetTickMs(void)
{
    return repRtosIsSchedulerRunning() ? repRtosGetTickMs() : HAL_GetTick();
}

eFc41dStatus fc41dPlatformInitTransport(eFc41dMapType device)
{
    (void)device;

    if ((huart4.Instance == NULL) || (drvUartInit(DRVUART_DEBUG) != DRV_STATUS_OK)) {
        return FC41D_STATUS_NOT_READY;
    }

    if (!gFc41dPortTransportPrepared) {
        drvGpioWrite(DRVGPIO_RESET_WIFI, DRVGPIO_PIN_RESET);
        fc41dPortDelayMs(FC41DPORT_RESET_ASSERT_MS);
        drvGpioWrite(DRVGPIO_RESET_WIFI, DRVGPIO_PIN_SET);
        fc41dPortDelayMs(FC41DPORT_RESET_RELEASE_MS);
        gFc41dPortTransportPrepared = true;
    }

    fc41dPortResetBootWaitState();
    return FC41D_STATUS_OK;
}

void fc41dPlatformPollRx(eFc41dMapType device)
{
    uint8_t lByte;
    uint16_t lPendingLength;

    (void)device;
    if (huart4.Instance == NULL) {
        return;
    }

    lPendingLength = drvUartGetDataLen(DRVUART_DEBUG);
    while (lPendingLength > 0U) {
        if (drvUartReceive(DRVUART_DEBUG, &lByte, 1U) != DRV_STATUS_OK) {
            break;
        }

        if (fc41dPortCollectRawBleByte(device, lByte)) {
            lPendingLength--;
            continue;
        }

        if (ringBufferWrite(&gFc41dPortAtRxRb, &lByte, 1U) != 1U) {
            break;
        }

        lPendingLength--;
    }
}

eFc41dStatus fc41dPlatformInitRxBuffers(eFc41dMapType device, stRingBuffer *bleRxRb, stRingBuffer *wifiRxRb)
{
    (void)device;

    if ((bleRxRb == NULL) || (wifiRxRb == NULL)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    if ((ringBufferInit(bleRxRb, gFc41dPortBleRxStorage, sizeof(gFc41dPortBleRxStorage)) != RINGBUFFER_OK) ||
        (ringBufferInit(wifiRxRb, gFc41dPortUnusedRxStorage, sizeof(gFc41dPortUnusedRxStorage)) != RINGBUFFER_OK)) {
        return FC41D_STATUS_ERROR;
    }

    return FC41D_STATUS_OK;
}

eFlowParserStrmSta fc41dPlatformInitAtStream(eFc41dMapType device, stFlowParserStream *stream,
                                             flowparserStreamLineFunc urcHandler, void *urcUserCtx)
{
    stFlowParserStreamCfg lCfg;
    static const char *const lUrcPatterns[] = {
        "+BLE*",
        "+QBLE*",
    };

    (void)device;
    if (stream == NULL) {
        return FLOWPARSER_STREAM_INVALID_ARG;
    }

    if (ringBufferInit(&gFc41dPortAtRxRb, gFc41dPortAtRxStorage, sizeof(gFc41dPortAtRxStorage)) != RINGBUFFER_OK) {
        return FLOWPARSER_STREAM_INVALID_ARG;
    }

    (void)memset(&lCfg, 0, sizeof(lCfg));
    lCfg.tokCfg.ringBuf = &gFc41dPortAtRxRb;
    lCfg.tokCfg.lineBuf = gFc41dPortAtLineBuf;
    lCfg.tokCfg.lineBufSize = sizeof(gFc41dPortAtLineBuf);
    lCfg.cmdBuf = gFc41dPortAtCmdBuf;
    lCfg.cmdBufSize = sizeof(gFc41dPortAtCmdBuf);
    lCfg.payloadBuf = gFc41dPortAtPayloadBuf;
    lCfg.payloadBufSize = sizeof(gFc41dPortAtPayloadBuf);
    lCfg.send = fc41dPortSend;
    lCfg.portUserCtx = &huart4;
    lCfg.getTick = fc41dPlatformGetTickMs;
    lCfg.urcPatterns = lUrcPatterns;
    lCfg.urcPatternCnt = (uint8_t)(sizeof(lUrcPatterns) / sizeof(lUrcPatterns[0]));
    lCfg.urcHandler = urcHandler;
    lCfg.urcUserCtx = urcUserCtx;
    lCfg.procBudget = 8U;
    return flowparserStreamInit(stream, &lCfg);
}

bool fc41dPlatformRouteLine(eFc41dMapType device, const uint8_t *lineBuf, uint16_t lineLen,
                            eFc41dRxChannel *channel, const uint8_t **payloadBuf, uint16_t *payloadLen)
{
    (void)device;

    if ((lineBuf == NULL) || (channel == NULL) || (payloadBuf == NULL) || (payloadLen == NULL)) {
        return false;
    }

    if ((lineLen >= 4U) && (memcmp(lineBuf, "+BLE", 4U) == 0)) {
        *channel = FC41D_RX_CHANNEL_BLE;
        return fc41dPortRouteLinePayload(lineBuf, lineLen, payloadBuf, payloadLen);
    }

    return false;
}

/**************************End of file********************************/
