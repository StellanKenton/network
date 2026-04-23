/***********************************************************************************
* @file     : display.c
* @brief    : Display manager implementation.
* @details  : Owns the TM1651 refresh path used by the scheduler.
* @author   : GitHub Copilot
* @date     : 2026-04-14
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "display.h"

#include <stddef.h>

#include "../../../rep/service/log/log.h"
#include "tm1651_port.h"

#define DISPLAY_LOG_TAG                  "SysDisplay"
#define DISPLAY_DEFAULT_BRIGHTNESS       2U

static bool gDisplayManagerReady = false;
static bool gDisplayInitFailureReported = false;

static void displayManagerLogInitFailureOnce(const char *stage, eDrvStatus status)
{
	if ((stage == NULL) || gDisplayInitFailureReported) {
		return;
	}

	gDisplayInitFailureReported = true;
//	LOG_E(DISPLAY_LOG_TAG, "tm1651 %s failed, status=%d", stage, (int)status);
}

void displayManagerReset(void)
{
	gDisplayManagerReady = false;
	gDisplayInitFailureReported = false;
}

bool displayManagerInit(void)
{
	eDrvStatus lStatus;

	gDisplayManagerReady = false;

	lStatus = tm1651PortInit();
	if (lStatus != DRV_STATUS_OK) {
		displayManagerLogInitFailureOnce("init", lStatus);
		return false;
	}

	lStatus = tm1651PortSetBrightness(DISPLAY_DEFAULT_BRIGHTNESS);
	if (lStatus != DRV_STATUS_OK) {
		displayManagerLogInitFailureOnce("brightness", lStatus);
		return false;
	}

	lStatus = tm1651PortSetDisplayOn(true);
	if (lStatus != DRV_STATUS_OK) {
		displayManagerLogInitFailureOnce("display-on", lStatus);
		return false;
	}

	lStatus = tm1651PortShowBoo();
	if (lStatus != DRV_STATUS_OK) {
		displayManagerLogInitFailureOnce("boo", lStatus);
		return false;
	}

	gDisplayInitFailureReported = false;
	gDisplayManagerReady = true;
	LOG_I(DISPLAY_LOG_TAG, "display task ready");
	return true;
}

bool displayManagerProcess(const stDisplayManagerInput *input, uint16_t *displayValue)
{
	if ((input == NULL) || (displayValue == NULL)) {
		return false;
	}

	if (!gDisplayManagerReady) {
		return false;
	}

	if (tm1651PortShowBoo() != DRV_STATUS_OK) {
		gDisplayManagerReady = false;
		LOG_W(DISPLAY_LOG_TAG, "tm1651 refresh lost");
		return false;
	}

	*displayValue = 0U;
	return true;
}
/**************************End of file********************************/
