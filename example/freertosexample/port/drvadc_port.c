/***********************************************************************************
* @file     : drvadc_port.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvadc_port.h"

#include <stdbool.h>
#include <stddef.h>

#include "adc.h"
#include "drvadc.h"

typedef struct stDrvAdcChannelMap {
    uint32_t channel;
} stDrvAdcChannelMap;

static stDrvAdcData gDrvAdcData[DRVADC_MAX] = {0};
static bool gDrvAdcCalibrated = false;
static bool gDrvAdcSingleShotConfigured = false;
volatile uint32_t gDrvAdcFaultTraceStage = 0U;
volatile uint32_t gDrvAdcFaultTraceAdc = 0U;

static const stDrvAdcChannelMap gDrvAdcChannelMap[DRVADC_MAX] = {
    [DRVADC_BAT] = {.channel = ADC_CHANNEL_0},
    [DRVADC_FORCE] = {.channel = ADC_CHANNEL_10},
    [DRVADC_DC] = {.channel = ADC_CHANNEL_11},
    [DRVADC_5V0] = {.channel = ADC_CHANNEL_14},
    [DRVADC_3V3] = {.channel = ADC_CHANNEL_15},
};

static eDrvStatus drvAdcPortConfigureSingleShot(void)
{
    gDrvAdcFaultTraceStage = 0x2001U;

    if (hadc1.Instance == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    if (gDrvAdcSingleShotConfigured) {
        gDrvAdcFaultTraceStage = 0x2002U;
        return DRV_STATUS_OK;
    }

    gDrvAdcFaultTraceStage = 0x2003U;
    (void)HAL_ADC_Stop(&hadc1);

    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;

    gDrvAdcFaultTraceStage = 0x2004U;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        return DRV_STATUS_ERROR;
    }

    gDrvAdcFaultTraceStage = 0x2005U;
    gDrvAdcSingleShotConfigured = true;
    gDrvAdcCalibrated = false;
    return DRV_STATUS_OK;
}

static eDrvStatus drvAdcPortInit(uint8_t adc)
{
    (void)adc;
    eDrvStatus lStatus;

    gDrvAdcFaultTraceAdc = adc;
    gDrvAdcFaultTraceStage = 0x2101U;

    if (hadc1.Instance == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvAdcPortConfigureSingleShot();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    if (!gDrvAdcCalibrated) {
        gDrvAdcFaultTraceStage = 0x2102U;
        if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK) {
            return DRV_STATUS_ERROR;
        }
        gDrvAdcCalibrated = true;
    }

    gDrvAdcFaultTraceStage = 0x2103U;
    return DRV_STATUS_OK;
}

static eDrvStatus drvAdcPortReadRaw(uint8_t adc, uint16_t *value, uint32_t timeoutMs)
{
    ADC_ChannelConfTypeDef channelConfig = {0};

    gDrvAdcFaultTraceAdc = adc;
    gDrvAdcFaultTraceStage = 0x2201U;

    if ((adc >= DRVADC_MAX) || (value == NULL) || (hadc1.Instance == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    channelConfig.Channel = gDrvAdcChannelMap[adc].channel;
    channelConfig.Rank = ADC_REGULAR_RANK_1;
    channelConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;

    gDrvAdcFaultTraceStage = 0x2202U;
    if (HAL_ADC_ConfigChannel(&hadc1, &channelConfig) != HAL_OK) {
        return DRV_STATUS_ERROR;
    }

    gDrvAdcFaultTraceStage = 0x2203U;
    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        return DRV_STATUS_ERROR;
    }

    gDrvAdcFaultTraceStage = 0x2204U;
    if (HAL_ADC_PollForConversion(&hadc1, timeoutMs) != HAL_OK) {
        gDrvAdcFaultTraceStage = 0x2205U;
        (void)HAL_ADC_Stop(&hadc1);
        return DRV_STATUS_TIMEOUT;
    }

    gDrvAdcFaultTraceStage = 0x2206U;
    *value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    gDrvAdcFaultTraceStage = 0x2207U;
    (void)HAL_ADC_Stop(&hadc1);
    gDrvAdcFaultTraceStage = 0x2208U;
    return DRV_STATUS_OK;
}

static const stDrvAdcBspInterface gDrvAdcBspInterface = {
    .init = drvAdcPortInit,
    .readRaw = drvAdcPortReadRaw,
    .defaultTimeoutMs = DRVADC_DEFAULT_TIMEOUT_MS,
    .referenceMv = DRVADC_DEFAULT_REFERENCE_MV,
    .resolutionBits = DRVADC_DEFAULT_RESOLUTION_BITS,
};

const stDrvAdcBspInterface *drvAdcGetPlatformBspInterface(void)
{
    return &gDrvAdcBspInterface;
}

stDrvAdcData *drvAdcGetPlatformData(void)
{
    return gDrvAdcData;
}

/**************************End of file********************************/
