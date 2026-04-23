/***********************************************************************************
* @file     : frameparser_port.c
* @brief    : Project binding for rep frameparser protocol defaults.
* @details  : Binds the update OTA line protocol to the reusable frameparser
*             through project-side protocol callbacks.
* @author   : GitHub Copilot
* @date     : 2026-04-20
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "frameparser_port.h"

#include <string.h>

#include "../../rep/service/rtos/rtos.h"
#include "../manager/comm/update_protocol.h"

static const uint8_t gFrameParserPortUpdateHead[] = {
    UPDATE_PROTOCOL_SYNC0,
    UPDATE_PROTOCOL_SYNC1,
    UPDATE_PROTOCOL_SYNC2,
};

static uint32_t frameParserPortGetUpdateHeadLen(const uint8_t *buf, uint32_t availLen, void *userCtx)
{
    (void)buf;
    (void)userCtx;

    if (availLen < UPDATE_PROTOCOL_FRAME_HEADER_SIZE) {
        return 0U;
    }

    return UPDATE_PROTOCOL_FRAME_HEADER_SIZE;
}

static const stFrmPsrProtoCfg gFrameParserPortProtoCfgMap[FRAMEPARSER_PORT_PROTOCOL_MAX] = {
    [FRAMEPARSER_PORT_PROTOCOL_UPDATE] = {
        .headPatList = {
            gFrameParserPortUpdateHead,
        },
        .headPatCount = 1U,
        .headPatLen = sizeof(gFrameParserPortUpdateHead),
        .minHeadLen = UPDATE_PROTOCOL_FRAME_HEADER_SIZE,
        .minPktLen = UPDATE_PROTOCOL_FRAME_HEADER_SIZE + UPDATE_PROTOCOL_FRAME_CRC16_SIZE,
        .maxPktLen = FRAMEPARSER_PORT_UPDATE_MAX_FRAME_SIZE,
        .waitPktToutMs = FRAMEPARSER_PORT_UPDATE_WAIT_PKT_TOUT_MS,
        .crcRangeStartOff = UPDATE_PROTOCOL_CMD_OFFSET,
        .crcRangeEndOff = -3,
        .crcFieldOff = -2,
        .cmdindex = UPDATE_PROTOCOL_CMD_OFFSET,
        .cmdLen = UPDATE_PROTOCOL_CMD_SIZE,
        .packlenindex = UPDATE_PROTOCOL_ENCODED_LENGTH_OFFSET,
        .packlenLen = UPDATE_PROTOCOL_ENCODED_LENGTH_SIZE,
        .crcFieldLen = UPDATE_PROTOCOL_FRAME_CRC16_SIZE,
        .crcFieldEnd = FRM_PSR_CRC_END_BIG,
        .headLenFunc = frameParserPortGetUpdateHeadLen,
        .pktLenFunc = updateProtocolGetPktLen,
        .crcCalcFunc = updateProtocolCalcCrc16,
        .getTick = frmPsrGetPlatformTickMs,
    },
};

uint32_t frmPsrGetPlatformTickMs(void)
{
    return repRtosGetTickMs();
}

void frmPsrLoadPlatformDefaultProtoCfg(uint32_t protocolId, stFrmPsrProtoCfg *protoCfg)
{
    if ((protoCfg == NULL) || (protocolId >= (uint32_t)FRAMEPARSER_PORT_PROTOCOL_MAX)) {
        return;
    }

    (void)memcpy(protoCfg, &gFrameParserPortProtoCfgMap[protocolId], sizeof(*protoCfg));
}

/**************************End of file********************************/
