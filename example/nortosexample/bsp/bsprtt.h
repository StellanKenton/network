/************************************************************************************
* @file     : bsprtt.h
* @brief    : Board-specific RTT binding for the reusable log transport layer.
* @details  : Exposes SEGGER RTT init, write, and input buffer hooks for log_port.
* @author   : GitHub Copilot
* @date     : 2026-04-17
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef BSPRTT_H
#define BSPRTT_H

#include <stdbool.h>
#include <stdint.h>

#include "../../rep/tools/ringbuffer/ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BSP_RTT_LOG_OUTPUT_ENABLE
#define BSP_RTT_LOG_OUTPUT_ENABLE    1
#endif

#ifndef BSP_RTT_LOG_INPUT_ENABLE
#define BSP_RTT_LOG_INPUT_ENABLE     1
#endif

#ifndef BSP_RTT_LOG_UP_BUFFER_INDEX
#define BSP_RTT_LOG_UP_BUFFER_INDEX      0U
#endif

#ifndef BSP_RTT_LOG_DOWN_BUFFER_INDEX
#define BSP_RTT_LOG_DOWN_BUFFER_INDEX    0U
#endif

#ifndef BSP_RTT_LOG_INPUT_BUFFER_SIZE
#define BSP_RTT_LOG_INPUT_BUFFER_SIZE    256U
#endif

#ifndef BSP_RTT_LOG_POLL_CHUNK_SIZE
#define BSP_RTT_LOG_POLL_CHUNK_SIZE      32U
#endif

void bspRttLogInit(void);
int32_t bspRttLogWrite(const uint8_t *buffer, uint16_t length);
stRingBuffer *bspRttLogGetInputBuffer(void);
void bspRttLogPollInput(void);

#ifdef __cplusplus
}
#endif

#endif  // BSPRTT_H
/**************************End of file********************************/
