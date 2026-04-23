/************************************************************************************
* @file     : bsp_rtt.h
* @brief    : SEGGER RTT log transport BSP declarations.
* @details  : Bridges SEGGER RTT with the reusable log/console transport hooks.
***********************************************************************************/
#ifndef BSP_RTT_H
#define BSP_RTT_H

#include <stdint.h>

#include "ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BSP_RTT_LOG_OUTPUT_ENABLE
#define BSP_RTT_LOG_OUTPUT_ENABLE      1
#endif

#ifndef BSP_RTT_LOG_INPUT_ENABLE
#define BSP_RTT_LOG_INPUT_ENABLE       1
#endif

#ifndef BSP_RTT_UP_BUFFER_INDEX
#define BSP_RTT_UP_BUFFER_INDEX        0U
#endif

#ifndef BSP_RTT_DOWN_BUFFER_INDEX
#define BSP_RTT_DOWN_BUFFER_INDEX      0U
#endif

#ifndef BSP_RTT_INPUT_BUFFER_SIZE
#define BSP_RTT_INPUT_BUFFER_SIZE      256U
#endif

void bspRttLogInit(void);
int32_t bspRttLogWrite(const uint8_t *buffer, uint16_t length);
stRingBuffer *bspRttLogGetInputBuffer(void);

#ifdef __cplusplus
}
#endif

#endif  // BSP_RTT_H
/**************************End of file********************************/

