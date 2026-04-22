/************************************************************************************
* @file     : selfcheck.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_SELFCHECK_H
#define REBUILDCPR_SELFCHECK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stSelfCheckSummary {
    bool expanderReady;
    bool displayReady;
    bool flashReady;
    bool motionReady;
    bool powerReady;
    bool updateReady;
    bool hasRun;
    bool isPassed;
} stSelfCheckSummary;

typedef struct stSelfCheckStatus {
    stSelfCheckSummary summary;
} stSelfCheckStatus;

bool selfCheckInit(void);
bool selfCheckStart(void);
void selfCheckReset(void);
void selfCheckSetExpanderResult(bool isPassed);
void selfCheckSetDisplayResult(bool isPassed);
void selfCheckSetFlashResult(bool isPassed);
void selfCheckSetMotionResult(bool isPassed);
void selfCheckSetPowerResult(bool isPassed);
void selfCheckSetUpdateResult(bool isPassed);
bool selfCheckCommit(void);
const stSelfCheckSummary *selfCheckGetSummary(void);
const stSelfCheckStatus *selfCheckGetStatus(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
