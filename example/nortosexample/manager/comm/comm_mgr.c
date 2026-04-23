/***********************************************************************************
* @file     : comm_mgr.c
* @brief    : Communication manager implementation.
* @details  : Coordinates the active project communication channels and provides
*             a single scheduler entry for the system manager.
* @author   : GitHub Copilot
* @date     : 2026-04-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "comm_mgr.h"

#include <string.h>

#include "../../../rep/driver/drvuart/drvuart.h"
#include "../../../rep/driver/drvusb/drvusb.h"
#include "../../../rep/service/log/log.h"
#include "../../port/drvuart_port.h"
#include "../../port/drvusb_port.h"
#include "../../port/frameparser_port.h"
#include "update_protocol.h"
#include "usbcdc/usbcdc.h"
#include "../wireless/wirelss_mgr.h"

#define COMM_MGR_LOG_TAG                 "CommMgr"

static stCommMgrState gCommMgr;

static bool commMgrInitParser(void);
static void commMgrSetCurrentChannel(eCommChannel channel);
static void commMgrUpdateActiveChannel(void);
static eUpdateProtocolTransport commMgrGetActiveTransport(void);
static void commMgrFeedParser(const uint8_t *buffer, uint16_t length);
static void commMgrPollUsbCdcRx(void);
static void commMgrPollUartRx(void);
static void commMgrPollBleRx(void);
static void commMgrDrainParser(void);
static bool commMgrWriteActiveChannel(const uint8_t *buffer, uint16_t length);
static const char *commMgrGetParserStatusName(eFrmPsrSta status);

static const char *commMgrGetParserStatusName(eFrmPsrSta status)
{
    switch (status) {
        case FRM_PSR_OK:
            return "OK";
        case FRM_PSR_EMPTY:
            return "EMPTY";
        case FRM_PSR_NEED_MORE_DATA:
            return "NEED_MORE_DATA";
        case FRM_PSR_INVALID_ARG:
            return "INVALID_ARG";
        case FRM_PSR_HEAD_NOT_FOUND:
            return "HEAD_NOT_FOUND";
        case FRM_PSR_HEAD_INVALID:
            return "HEAD_INVALID";
        case FRM_PSR_LEN_INVALID:
            return "LEN_INVALID";
        case FRM_PSR_CRC_FAIL:
            return "CRC_FAIL";
        case FRM_PSR_OUT_BUF_SMALL:
            return "OUT_BUF_SMALL";
        case FRM_PSR_PROTO_INVALID:
            return "PROTO_INVALID";
        default:
            return "UNKNOWN";
    }
}

void commMgrReset(void)
{
    (void)memset(&gCommMgr, 0, sizeof(gCommMgr));
}

bool commMgrInit(void)
{
    commMgrReset();
    if (!commMgrInitParser()) {
        LOG_E(COMM_MGR_LOG_TAG, "frame parser init failed");
        return false;
    }

#if COMM_MGR_ENABLE_USB
    LOG_I(COMM_MGR_LOG_TAG, "initializing usb cdc channel");

    if (drvUsbInit(DRVUSB_DEV0) != DRV_STATUS_OK) {
        LOG_E(COMM_MGR_LOG_TAG, "usb cdc init failed");
        return false;
    }

    if (drvUsbStart(DRVUSB_DEV0) != DRV_STATUS_OK) {
        LOG_E(COMM_MGR_LOG_TAG, "usb cdc start failed");
        return false;
    }

    if (drvUsbConnect(DRVUSB_DEV0) != DRV_STATUS_OK) {
        LOG_E(COMM_MGR_LOG_TAG, "usb cdc connect failed");
        return false;
    }

    gCommMgr.isUsbCdcReady = usbCdcManagerInit();
    if (!gCommMgr.isUsbCdcReady) {
        LOG_E(COMM_MGR_LOG_TAG, "usb cdc manager init failed");
        return false;
    }
#endif

#if COMM_MGR_ENABLE_UART
    if (drvUartInit(DRVUART_UART1) != DRV_STATUS_OK) {
        LOG_E(COMM_MGR_LOG_TAG, "uart init failed");
        return false;
    }

    gCommMgr.isUartReady = true;
#endif

    gCommMgr.isInitialized = true;
    commMgrUpdateActiveChannel();
    return true;
}

void commMgrProcess(void)
{
    if (!gCommMgr.isInitialized) {
        return;
    }

#if COMM_MGR_ENABLE_USB
    usbCdcManagerProcess();
#endif
#if COMM_MGR_ENABLE_BLE
    gCommMgr.isBleReady = wirelessMgrIsReady() && wirelessMgrIsHandshakeReady();
#endif
    commMgrUpdateActiveChannel();

    switch (gCommMgr.currentChannel) {
        case E_COMM_CHANNEL_USB_CDC:
            commMgrPollUsbCdcRx();
            break;
        case E_COMM_CHANNEL_UART:
            commMgrPollUartRx();
            break;
        case E_COMM_CHANNEL_BLE:
            commMgrPollBleRx();
            break;
        default:
            break;
    }

    commMgrDrainParser();
}

bool commMgrSendProtocolFrame(uint8_t cmd, const uint8_t *payload, uint16_t payloadLength)
{
    uint8_t lFrame[FRAMEPARSER_PORT_UPDATE_MAX_FRAME_SIZE];
    uint8_t lEncodedPayload[UPDATE_PROTOCOL_MAX_PAYLOAD_SIZE];
    eUpdateProtocolTransport lTransport;
    uint16_t lEncodedPayloadLength;
    uint16_t lFrameLength;
    uint16_t lCrc16;

    lTransport = commMgrGetActiveTransport();
    if (!updateProtocolEncodePayload(lTransport,
                                     cmd,
                                     payload,
                                     payloadLength,
                                     lEncodedPayload,
                                     (uint16_t)sizeof(lEncodedPayload),
                                     &lEncodedPayloadLength)) {
        return false;
    }

    lFrame[0] = UPDATE_PROTOCOL_SYNC0;
    lFrame[1] = UPDATE_PROTOCOL_SYNC1;
    lFrame[2] = UPDATE_PROTOCOL_SYNC2;
    lFrame[3] = cmd;
    lFrame[4] = (uint8_t)((lEncodedPayloadLength >> 8U) & 0xFFU);
    lFrame[5] = (uint8_t)(lEncodedPayloadLength & 0xFFU);
    if (lEncodedPayloadLength > 0U) {
        (void)memcpy(&lFrame[UPDATE_PROTOCOL_FRAME_HEADER_SIZE], lEncodedPayload, lEncodedPayloadLength);
    }

    lFrameLength = (uint16_t)(UPDATE_PROTOCOL_FRAME_HEADER_SIZE + lEncodedPayloadLength + UPDATE_PROTOCOL_FRAME_CRC16_SIZE);
    lCrc16 = (uint16_t)updateProtocolCalcCrc16(&lFrame[UPDATE_PROTOCOL_CMD_OFFSET],
                                               UPDATE_PROTOCOL_CMD_SIZE + UPDATE_PROTOCOL_ENCODED_LENGTH_SIZE + lEncodedPayloadLength,
                                               NULL);
    lFrame[lFrameLength - 2U] = (uint8_t)((lCrc16 >> 8U) & 0xFFU);
    lFrame[lFrameLength - 1U] = (uint8_t)(lCrc16 & 0xFFU);
    return commMgrWriteActiveChannel(lFrame, lFrameLength);
}

static bool commMgrInitParser(void)
{
    stFrmPsrCfg lParserCfg;
    eFrmPsrSta lParserSta;

    (void)memset(&lParserCfg, 0, sizeof(lParserCfg));
    lParserCfg.protocolId = (uint32_t)FRAMEPARSER_PORT_PROTOCOL_UPDATE;
    lParserCfg.streamBuf = gCommMgr.parserStreamBuf;
    lParserCfg.streamBufSize = sizeof(gCommMgr.parserStreamBuf);
    lParserCfg.frameBuf = gCommMgr.parserFrameBuf;
    lParserCfg.frameBufSize = sizeof(gCommMgr.parserFrameBuf);

    gCommMgr.hasReadyFrame = false;
    gCommMgr.readyFrameLength = 0U;
    lParserSta = frmPsrInit(&gCommMgr.updateParser, &lParserCfg);
    if (lParserSta != FRM_PSR_OK) {
        LOG_E(COMM_MGR_LOG_TAG, "frame parser init failed: %d", (int)lParserSta);
        return false;
    }

    return true;
}

static void commMgrSetCurrentChannel(eCommChannel channel)
{
    if (gCommMgr.currentChannel == channel) {
        return;
    }

    gCommMgr.currentChannel = channel;
    (void)commMgrInitParser();
}

static void commMgrUpdateActiveChannel(void)
{
    if (gCommMgr.isUsbCdcReady && drvUsbIsConfigured(DRVUSB_DEV0)) {
        commMgrSetCurrentChannel(E_COMM_CHANNEL_USB_CDC);
        return;
    }

    if (gCommMgr.isBleReady) {
        commMgrSetCurrentChannel(E_COMM_CHANNEL_BLE);
        return;
    }

    if (gCommMgr.isUartReady) {
        commMgrSetCurrentChannel(E_COMM_CHANNEL_UART);
        return;
    }
}

static eUpdateProtocolTransport commMgrGetActiveTransport(void)
{
    switch (gCommMgr.currentChannel) {
        case E_COMM_CHANNEL_BLE:
            return E_UPDATE_PROTOCOL_TRANSPORT_BLE;
        case E_COMM_CHANNEL_USB_CDC:
            return E_UPDATE_PROTOCOL_TRANSPORT_USB;
        case E_COMM_CHANNEL_UART:
        default:
            return E_UPDATE_PROTOCOL_TRANSPORT_UART;
    }
}

static void commMgrFeedParser(const uint8_t *buffer, uint16_t length)
{
    eFrmPsrSta lParserSta;

    if ((buffer == NULL) || (length == 0U)) {
        return;
    }

    lParserSta = frmPsrFeed(&gCommMgr.updateParser, buffer, length);
    if (lParserSta == FRM_PSR_OK) {
        return;
    }

    LOG_W(COMM_MGR_LOG_TAG, "frame parser feed failed: %d", (int)lParserSta);
    (void)commMgrInitParser();
}

static void commMgrPollUsbCdcRx(void)
{
    uint8_t lRxBuffer[USBCDC_MANAGER_RX_CHUNK_SIZE];
    uint16_t lActualLength = 0U;

    if (!gCommMgr.isUsbCdcReady) {
        return;
    }

    if (usbCdcManagerGetRxLength() == 0U) {
        return;
    }

    if (!usbCdcManagerRead(lRxBuffer, sizeof(lRxBuffer), &lActualLength)) {
        return;
    }

    commMgrFeedParser(lRxBuffer, lActualLength);
}

static void commMgrPollUartRx(void)
{
    uint8_t lRxBuffer[COMM_MGR_UART_RX_CHUNK_SIZE];
    uint16_t lAvailLength;
    uint16_t lReadLength;

    if (!gCommMgr.isUartReady) {
        return;
    }

    lAvailLength = drvUartGetDataLen(DRVUART_UART1);
    while (lAvailLength > 0U) {
        lReadLength = lAvailLength;
        if (lReadLength > sizeof(lRxBuffer)) {
            lReadLength = sizeof(lRxBuffer);
        }

        if (drvUartReceive(DRVUART_UART1, lRxBuffer, lReadLength) != DRV_STATUS_OK) {
            LOG_W(COMM_MGR_LOG_TAG, "uart read failed");
            break;
        }

        commMgrFeedParser(lRxBuffer, lReadLength);
        lAvailLength = drvUartGetDataLen(DRVUART_UART1);
    }
}

static void commMgrPollBleRx(void)
{
#if COMM_MGR_ENABLE_BLE
    uint8_t lRxBuffer[COMM_MGR_UART_RX_CHUNK_SIZE];
    uint16_t lAvailLength;
    uint16_t lReadLength;

    if (!gCommMgr.isBleReady) {
        return;
    }

    lAvailLength = wirelessMgrGetRxLength();
    while (lAvailLength > 0U) {
        lReadLength = lAvailLength;
        if (lReadLength > sizeof(lRxBuffer)) {
            lReadLength = sizeof(lRxBuffer);
        }

        lReadLength = wirelessMgrReadRxData(lRxBuffer, lReadLength);
        if (lReadLength == 0U) {
            LOG_W(COMM_MGR_LOG_TAG, "ble read failed");
            break;
        }

        commMgrFeedParser(lRxBuffer, lReadLength);
        lAvailLength = wirelessMgrGetRxLength();
    }
#else
    return;
#endif
}

static void commMgrDrainParser(void)
{
    eFrmPsrSta lParserSta;
    const stFrmPsrPkt *lpPkt;
    stUpdateProtocolReply lReply;

    while (true) {
        lParserSta = frmPsrProcess(&gCommMgr.updateParser);
        if ((lParserSta == FRM_PSR_OK) && gCommMgr.updateParser.hasReadyPkt) {
            lpPkt = frmPsrRelease(&gCommMgr.updateParser);
            if (lpPkt == NULL) {
                break;
            }

            if (updateProtocolHandlePkt(lpPkt, commMgrGetActiveTransport(), &lReply) && lReply.hasReply) {
                if (!commMgrSendProtocolFrame(lReply.cmd, lReply.payload, lReply.payloadLength)) {
                    LOG_W(COMM_MGR_LOG_TAG, "send reply failed, cmd=0x%02X", lReply.cmd);
                }
            }
            continue;
        }

        if ((lParserSta == FRM_PSR_CRC_FAIL) ||
            (lParserSta == FRM_PSR_LEN_INVALID) ||
            (lParserSta == FRM_PSR_HEAD_INVALID) ||
            (lParserSta == FRM_PSR_OUT_BUF_SMALL)) {
            LOG_W(COMM_MGR_LOG_TAG,
                  "frame parser process status: %d (%s)",
                  (int)lParserSta,
                  commMgrGetParserStatusName(lParserSta));
        }
        break;
    }
}

static bool commMgrWriteActiveChannel(const uint8_t *buffer, uint16_t length)
{
    if ((buffer == NULL) || (length == 0U)) {
        return false;
    }

    switch (gCommMgr.currentChannel) {
        case E_COMM_CHANNEL_USB_CDC:
#if COMM_MGR_ENABLE_USB
            return gCommMgr.isUsbCdcReady && usbCdcManagerWrite(buffer, length);
#else
            return false;
#endif
        case E_COMM_CHANNEL_UART:
#if COMM_MGR_ENABLE_UART
            return drvUartTransmit(DRVUART_UART1, buffer, length, 100U) == DRV_STATUS_OK;
#else
            return false;
#endif
        case E_COMM_CHANNEL_BLE:
    #if COMM_MGR_ENABLE_BLE
            return gCommMgr.isBleReady && wirelessMgrWriteData(buffer, length);
    #else
            return false;
    #endif
        default:
            return false;
    }
}

/**************************End of file********************************/
