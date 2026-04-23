/***********************************************************************************
* @file     : comm_mgr.h
* @brief    : Communication manager declarations.
* @details  : Coordinates project communication channels such as USB CDC, BLE,
*             and UART.
* @author   : GitHub Copilot
* @date     : 2026-04-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CPRSENSORBOOT_COMM_MGR_H
#define CPRSENSORBOOT_COMM_MGR_H

#include <stdbool.h>
#include <stdint.h>

#include "../../port/frameparser_port.h"
#include "update_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COMM_MGR_ENABLE_BLE
#define COMM_MGR_ENABLE_BLE              1
#endif

#ifndef COMM_MGR_ENABLE_UART
#define COMM_MGR_ENABLE_UART             1
#endif

#ifndef COMM_MGR_ENABLE_USB          
#define COMM_MGR_ENABLE_USB              0
#endif

#ifndef COMM_MGR_UART_RX_CHUNK_SIZE
#define COMM_MGR_UART_RX_CHUNK_SIZE      64U
#endif

typedef enum eCommChannel {
    E_COMM_CHANNEL_USB_CDC,
    E_COMM_CHANNEL_BLE,
    E_COMM_CHANNEL_UART,
    E_COMM_CHANNEL_COUNT
} eCommChannel;

typedef struct stCommMgrState {
    eCommChannel currentChannel;
    stFrmPsr updateParser;
    bool isInitialized;
    bool isUsbCdcReady;
    bool isBleReady;
    bool isUartReady;
    bool hasReadyFrame;
    uint16_t readyFrameLength;
    uint8_t parserStreamBuf[FRAMEPARSER_PORT_UPDATE_MAX_FRAME_SIZE];
    uint8_t parserFrameBuf[FRAMEPARSER_PORT_UPDATE_MAX_FRAME_SIZE];
    uint8_t readyFrameBuf[FRAMEPARSER_PORT_UPDATE_MAX_FRAME_SIZE];
} stCommMgrState;

void commMgrReset(void);
bool commMgrInit(void);
void commMgrProcess(void);
bool commMgrSendProtocolFrame(uint8_t cmd, const uint8_t *payload, uint16_t payloadLength);

#ifdef __cplusplus
}
#endif

#endif  // CPRSENSORBOOT_COMM_MGR_H
/**************************End of file********************************/
