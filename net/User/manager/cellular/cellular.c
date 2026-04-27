/***********************************************************************************
* @file     : cellular.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "cellular.h"

#include <string.h>

#include "../iotmanager/iotmanager.h"
#include "../../../rep/service/log/log.h"
#include "../../../rep/service/rtos/rtos.h"

enum eCellularInfoStage {
	CELLULAR_INFO_STAGE_IDLE = 0,
	CELLULAR_INFO_STAGE_ATI,
	CELLULAR_INFO_STAGE_IMEI,
	CELLULAR_INFO_STAGE_CPIN,
	CELLULAR_INFO_STAGE_CSQ,
	CELLULAR_INFO_STAGE_QCCID,
	CELLULAR_INFO_STAGE_DONE,
};

static const char *const gCellularInfoCommands[] = {
	"",
	"ATI\r\n",
	"AT+GSN\r\n",
	"AT+CPIN?\r\n",
	"AT+CSQ\r\n",
	"AT+QCCID\r\n",
};

static bool gCellularInitialized;
static bool gCellularStarted;
static bool gCellularInfoReadPending;
static bool gCellularInfoReadActive;
static enum eCellularInfoStage gCellularInfoStage = CELLULAR_INFO_STAGE_IDLE;
static eEc800mStatus gCellularInitStatus = EC800M_STATUS_NOT_READY;
static eEc800mStatus gCellularLastProcessStatus = EC800M_STATUS_NOT_READY;
static int16_t gCellularSignalStrength = -1;
static uint32_t gCellularLastOkTick;
static uint32_t gCellularLastFailTick;
static uint32_t gCellularNextInitTick;
static uint32_t gCellularNextProcessWarnTick;

static void cellularUpdateIotState(void);
static void cellularServiceInit(uint32_t nowTickMs);
static void cellularServiceInfoRead(void);
static void cellularInfoLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static bool cellularMatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix);
static void cellularParseCsq(const uint8_t *lineBuf, uint16_t lineLen);
static bool cellularInfoSubmitCurrentCommand(void);

bool cellularSendData(const uint8_t *buffer, uint16_t length)
{
	(void)buffer;
	(void)length;
	LOG_W("cellular", "mqtt send requested but ec800m mqtt session is not configured");
	return false;
}

bool cellularRequestInfoRead(void)
{
	gCellularInfoReadPending = true;
	return true;
}

bool cellularGetDebugStatus(stCellularDebugStatus *status)
{
	const stEc800mState *ecState;

	if (status == NULL) {
		return false;
	}

	(void)memset(status, 0, sizeof(*status));
	status->initialized = gCellularInitialized;
	status->started = gCellularStarted;
	status->infoReadPending = gCellularInfoReadPending;
	status->infoReadActive = gCellularInfoReadActive;
	status->initStatus = gCellularInitStatus;
	status->lastProcessStatus = gCellularLastProcessStatus;
	status->signalStrength = gCellularSignalStrength;
	status->lastOkTick = gCellularLastOkTick;
	status->lastFailTick = gCellularLastFailTick;

	ecState = ec800mGetState(EC800M_DEV0);
	if (ecState != NULL) {
		status->runState = ecState->runState;
		status->moduleReady = ecState->isReady;
		status->busy = ecState->isBusy;
		status->atReady = ecState->isAtReady;
		status->simChecked = ecState->isSimChecked;
		status->signalChecked = ecState->isSignalChecked;
	}

	return true;
}

void cellularProcess(void)
{
	uint32_t nowTickMs;

	nowTickMs = repRtosGetTickMs();
	cellularServiceInit(nowTickMs);
	if (gCellularInitialized && gCellularStarted) {
		gCellularLastProcessStatus = ec800mProcess(EC800M_DEV0, nowTickMs);
		if (gCellularLastProcessStatus == EC800M_STATUS_OK) {
			gCellularLastOkTick = nowTickMs;
		} else {
			gCellularLastFailTick = nowTickMs;
			if ((gCellularNextProcessWarnTick == 0U) || ((uint32_t)(nowTickMs - gCellularNextProcessWarnTick) <= 0x7FFFFFFFUL)) {
				gCellularNextProcessWarnTick = nowTickMs + 1000U;
				LOG_W("cellular", "ec800m process status=%d", (int)gCellularLastProcessStatus);
			}
		}
		cellularServiceInfoRead();
	}
	cellularUpdateIotState();
}

static void cellularUpdateIotState(void)
{
	const stEc800mState *ecState;
	stIotManagerLinkRuntime runtime;

	ecState = ec800mGetState(EC800M_DEV0);
	(void)memset(&runtime, 0, sizeof(runtime));
	runtime.linkId = IOT_MANAGER_LINK_CELLULAR;
	runtime.cellularType = IOT_MANAGER_CELLULAR_4G;
	runtime.installed = true;
	runtime.enabled = true;
	runtime.caps.supportMqttAuthHttp = true;
	runtime.caps.supportMqtt = true;
	runtime.signalStrength = gCellularSignalStrength;
	runtime.lastOkTick = gCellularLastOkTick;
	runtime.lastFailTick = gCellularLastFailTick;

	if (ecState == NULL) {
		runtime.state = IOT_MANAGER_LINK_STATE_INITING;
	} else if (ecState->runState == EC800M_RUN_ERROR) {
		runtime.state = IOT_MANAGER_LINK_STATE_ERROR;
		runtime.busy = ecState->isBusy;
		runtime.moduleReady = false;
	} else if (!gCellularInitialized || !gCellularStarted) {
		runtime.state = (gCellularInitStatus == EC800M_STATUS_UNSUPPORTED) ?
			IOT_MANAGER_LINK_STATE_ERROR : IOT_MANAGER_LINK_STATE_INITING;
	} else if (ecState->isReady) {
		runtime.state = IOT_MANAGER_LINK_STATE_NET_READY;
		runtime.moduleReady = true;
		runtime.netReady = true;
		runtime.busy = ecState->isBusy;
	} else {
		runtime.state = IOT_MANAGER_LINK_STATE_INITING;
		runtime.busy = ecState->isBusy;
		runtime.moduleReady = ecState->isAtReady;
	}

	(void)iotManagerUpdateLinkState(IOT_MANAGER_LINK_CELLULAR, &runtime);
}

static void cellularServiceInit(uint32_t nowTickMs)
{
	if (gCellularInitialized && gCellularStarted) {
		return;
	}

	if ((gCellularNextInitTick != 0U) && ((uint32_t)(nowTickMs - gCellularNextInitTick) > 0x7FFFFFFFUL)) {
		return;
	}

	if (!gCellularInitialized) {
		gCellularInitStatus = ec800mInit(EC800M_DEV0);
		if (gCellularInitStatus != EC800M_STATUS_OK) {
			gCellularLastFailTick = nowTickMs;
			gCellularNextInitTick = nowTickMs + 3000U;
			LOG_W("cellular", "ec800m init status=%d", (int)gCellularInitStatus);
			return;
		}
		gCellularInitialized = true;
		LOG_I("cellular", "ec800m init ok");
	}

	gCellularInitStatus = ec800mStart(EC800M_DEV0, EC800M_SERVICE_MQTT_HTTP);
	if (gCellularInitStatus != EC800M_STATUS_OK) {
		gCellularLastFailTick = nowTickMs;
		gCellularNextInitTick = nowTickMs + 3000U;
		LOG_W("cellular", "ec800m start status=%d", (int)gCellularInitStatus);
		return;
	}

	gCellularStarted = true;
	gCellularLastOkTick = nowTickMs;
	LOG_I("cellular", "ec800m start ok");
}

static void cellularServiceInfoRead(void)
{
	const stEc800mInfo *info;
	const stEc800mState *state;

	info = ec800mGetInfo(EC800M_DEV0);
	state = ec800mGetState(EC800M_DEV0);
	if ((info == NULL) || (state == NULL) || !state->isReady) {
		return;
	}

	if (!gCellularInfoReadActive) {
		if (!gCellularInfoReadPending || info->isBusy) {
			return;
		}
		gCellularInfoReadPending = false;
		gCellularInfoReadActive = true;
		gCellularInfoStage = CELLULAR_INFO_STAGE_ATI;
		(void)cellularInfoSubmitCurrentCommand();
		return;
	}

	if (info->isBusy) {
		return;
	}

	if (info->hasLastResult && (info->lastResult != FLOWPARSER_RESULT_OK)) {
		LOG_W("cellular", "info read failed stage=%u result=%u", (unsigned int)gCellularInfoStage, (unsigned int)info->lastResult);
		gCellularInfoReadActive = false;
		gCellularInfoStage = CELLULAR_INFO_STAGE_IDLE;
		return;
	}

	if (gCellularInfoStage < CELLULAR_INFO_STAGE_QCCID) {
		gCellularInfoStage = (enum eCellularInfoStage)((uint8_t)gCellularInfoStage + 1U);
		(void)cellularInfoSubmitCurrentCommand();
	} else {
		gCellularInfoReadActive = false;
		gCellularInfoStage = CELLULAR_INFO_STAGE_DONE;
		LOG_I("cellular", "info read done csq=%d", (int)gCellularSignalStrength);
	}
}

static void cellularInfoLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
	char lineText[96];
	uint16_t copyLen;

	(void)userData;
	if ((lineBuf == NULL) || (lineLen == 0U)) {
		return;
	}

	cellularParseCsq(lineBuf, lineLen);
	copyLen = lineLen;
	if (copyLen >= sizeof(lineText)) {
		copyLen = (uint16_t)(sizeof(lineText) - 1U);
	}
	(void)memcpy(lineText, lineBuf, copyLen);
	lineText[copyLen] = '\0';
	LOG_I("cellular", "info: %s", lineText);
}

static bool cellularMatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix)
{
	uint16_t prefixLen;

	if ((lineBuf == NULL) || (prefix == NULL)) {
		return false;
	}

	prefixLen = (uint16_t)strlen(prefix);
	return (lineLen >= prefixLen) && (memcmp(lineBuf, prefix, prefixLen) == 0);
}

static void cellularParseCsq(const uint8_t *lineBuf, uint16_t lineLen)
{
	uint16_t index;
	uint16_t value;
	uint8_t digitCount;

	if (!cellularMatchPrefix(lineBuf, lineLen, "+CSQ:")) {
		return;
	}

	index = 5U;
	while ((index < lineLen) && ((lineBuf[index] == ' ') || (lineBuf[index] == '\t'))) {
		index++;
	}

	value = 0U;
	digitCount = 0U;
	while ((index < lineLen) && (lineBuf[index] >= '0') && (lineBuf[index] <= '9')) {
		value = (uint16_t)((value * 10U) + (uint16_t)(lineBuf[index] - '0'));
		digitCount++;
		index++;
	}

	if (digitCount > 0U) {
		gCellularSignalStrength = (value == 99U) ? -1 : (int16_t)value;
	}
}

static bool cellularInfoSubmitCurrentCommand(void)
{
	eEc800mStatus status;

	if ((gCellularInfoStage <= CELLULAR_INFO_STAGE_IDLE) || (gCellularInfoStage >= CELLULAR_INFO_STAGE_DONE)) {
		return false;
	}

	status = ec800mSubmitTextCommandEx(EC800M_DEV0,
		gCellularInfoCommands[(uint8_t)gCellularInfoStage],
		cellularInfoLineHandler,
		NULL);
	if (status != EC800M_STATUS_OK) {
		LOG_W("cellular", "info submit failed stage=%u status=%d", (unsigned int)gCellularInfoStage, (int)status);
		gCellularInfoReadActive = false;
		gCellularInfoStage = CELLULAR_INFO_STAGE_IDLE;
		return false;
	}

	return true;
}

/**************************End of file********************************/
