/************************************************************************************
* @file     : bsp_rtt.c
* @brief    : SEGGER RTT log transport BSP implementation.
* @details  : Provides the RTT hooks required by the reusable log and console layers.
***********************************************************************************/
#include "bsp_rtt.h"

#include <stdbool.h>
#include <stddef.h>

#include "../../../net/rep/lib/SEGGER/SEGGER_RTT.h"

#define BSP_RTT_INPUT_READ_CHUNK_SIZE  32U

static bool gBspRttIsInitialized = false;

#if (BSP_RTT_LOG_INPUT_ENABLE != 0)
static stRingBuffer gBspRttInputBuffer;
static uint8_t gBspRttInputStorage[BSP_RTT_INPUT_BUFFER_SIZE];
#endif

static void bspRttRefillInputBuffer(void);

void bspRttLogInit(void)
{
    if (gBspRttIsInitialized) {
        return;
    }

#if (BSP_RTT_LOG_OUTPUT_ENABLE != 0)
    SEGGER_RTT_ConfigUpBuffer(BSP_RTT_UP_BUFFER_INDEX,
                              "RTTUP",
                              NULL,
                              0U,
                              SEGGER_RTT_MODE_NO_BLOCK_SKIP);
#endif

#if (BSP_RTT_LOG_INPUT_ENABLE != 0)
    SEGGER_RTT_ConfigDownBuffer(BSP_RTT_DOWN_BUFFER_INDEX,
                                "RTTDOWN",
                                NULL,
                                0U,
                                SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    (void)ringBufferInit(&gBspRttInputBuffer, gBspRttInputStorage, BSP_RTT_INPUT_BUFFER_SIZE);
#endif

    gBspRttIsInitialized = true;
}

int32_t bspRttLogWrite(const uint8_t *buffer, uint16_t length)
{
#if (BSP_RTT_LOG_OUTPUT_ENABLE == 0)
    (void)buffer;
    (void)length;
    return 0;
#else
    if ((buffer == NULL) || (length == 0U)) {
        return 0;
    }

    if (!gBspRttIsInitialized) {
        bspRttLogInit();
    }

    return (int32_t)SEGGER_RTT_Write(BSP_RTT_UP_BUFFER_INDEX, buffer, (unsigned)length);
#endif
}

stRingBuffer *bspRttLogGetInputBuffer(void)
{
#if (BSP_RTT_LOG_INPUT_ENABLE == 0)
    return NULL;
#else
    if (!gBspRttIsInitialized) {
        bspRttLogInit();
    }

    bspRttRefillInputBuffer();
    return &gBspRttInputBuffer;
#endif
}

static void bspRttRefillInputBuffer(void)
{
#if (BSP_RTT_LOG_INPUT_ENABLE != 0)
    uint8_t lReadBuffer[BSP_RTT_INPUT_READ_CHUNK_SIZE];

    for (;;) {
        uint32_t lFree = ringBufferGetFree(&gBspRttInputBuffer);
        uint32_t lReadSize = sizeof(lReadBuffer);
        unsigned lReceived = 0U;

        if (lFree == 0U) {
            break;
        }

        if (lReadSize > lFree) {
            lReadSize = lFree;
        }

        lReceived = SEGGER_RTT_Read(BSP_RTT_DOWN_BUFFER_INDEX, lReadBuffer, (unsigned)lReadSize);
        if (lReceived == 0U) {
            break;
        }

        if (ringBufferWrite(&gBspRttInputBuffer, lReadBuffer, (uint32_t)lReceived) != (uint32_t)lReceived) {
            break;
        }
    }
#endif
}
/**************************End of file********************************/
