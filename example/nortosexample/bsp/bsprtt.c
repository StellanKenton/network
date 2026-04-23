/***********************************************************************************
* @file     : bsprtt.c
* @brief    : Board-specific RTT binding for the reusable log transport layer.
* @details  : Wraps SEGGER RTT as a log transport and mirrors RTT input into a
*             ring buffer for console consumers.
* @author   : GitHub Copilot
* @date     : 2026-04-17
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "bsprtt.h"

#include <stddef.h>
#include <string.h>

#include "../SEGGER/SEGGER_RTT.h"

static bool gBspRttIsInitialized = false;

#if BSP_RTT_LOG_INPUT_ENABLE
static stRingBuffer gBspRttInputBuffer;
static uint8_t gBspRttInputStorage[BSP_RTT_LOG_INPUT_BUFFER_SIZE];
static uint8_t gBspRttPollScratch[BSP_RTT_LOG_POLL_CHUNK_SIZE];
#endif

static void bspRttEnsureInitialized(void)
{
    static const char lInitBanner[] = "\r\n[RTT] transport initialized\r\n";

    if (gBspRttIsInitialized) {
        return;
    }

    SEGGER_RTT_Init();

#if BSP_RTT_LOG_OUTPUT_ENABLE
    (void)SEGGER_RTT_ConfigUpBuffer(BSP_RTT_LOG_UP_BUFFER_INDEX,
                                    "LOG_RTT_UP",
                                    NULL,
                                    0U,
                                    SEGGER_RTT_MODE_NO_BLOCK_SKIP);
#endif

#if BSP_RTT_LOG_INPUT_ENABLE
    (void)memset(&gBspRttInputBuffer, 0, sizeof(gBspRttInputBuffer));
    (void)memset(gBspRttInputStorage, 0, sizeof(gBspRttInputStorage));
    (void)SEGGER_RTT_ConfigDownBuffer(BSP_RTT_LOG_DOWN_BUFFER_INDEX,
                                      "LOG_RTT_DOWN",
                                      NULL,
                                      0U,
                                      SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    (void)ringBufferInit(&gBspRttInputBuffer,
                         gBspRttInputStorage,
                         (uint32_t)sizeof(gBspRttInputStorage));
#endif

    (void)SEGGER_RTT_Write(BSP_RTT_LOG_UP_BUFFER_INDEX, lInitBanner, sizeof(lInitBanner) - 1U);
    gBspRttIsInitialized = true;
}

void bspRttLogInit(void)
{
    bspRttEnsureInitialized();
}

int32_t bspRttLogWrite(const uint8_t *buffer, uint16_t length)
{
#if !BSP_RTT_LOG_OUTPUT_ENABLE
    (void)buffer;
    (void)length;
    return 0;
#else
    if ((buffer == NULL) || (length == 0U)) {
        return 0;
    }

    bspRttEnsureInitialized();
    return (int32_t)SEGGER_RTT_Write(BSP_RTT_LOG_UP_BUFFER_INDEX, buffer, (unsigned)length);
#endif
}

stRingBuffer *bspRttLogGetInputBuffer(void)
{
#if !BSP_RTT_LOG_INPUT_ENABLE
    return NULL;
#else
    bspRttEnsureInitialized();
    return &gBspRttInputBuffer;
#endif
}

void bspRttLogPollInput(void)
{
#if BSP_RTT_LOG_INPUT_ENABLE
    uint32_t lFree = 0U;

    bspRttEnsureInitialized();
    lFree = ringBufferGetFree(&gBspRttInputBuffer);
    while (lFree > 0U) {
        unsigned lReadSize = (unsigned)((lFree < (uint32_t)sizeof(gBspRttPollScratch)) ?
                                        lFree :
                                        (uint32_t)sizeof(gBspRttPollScratch));
        unsigned lReadCount = SEGGER_RTT_Read(BSP_RTT_LOG_DOWN_BUFFER_INDEX,
                                              gBspRttPollScratch,
                                              lReadSize);

        if (lReadCount == 0U) {
            break;
        }

        (void)ringBufferWrite(&gBspRttInputBuffer, gBspRttPollScratch, (uint32_t)lReadCount);
        lFree = ringBufferGetFree(&gBspRttInputBuffer);
    }
#endif
}

/**************************End of file********************************/
