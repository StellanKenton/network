#include "sysmgr.h"

#include "sys.h"
#include "led.h"
#include "../../rep/service/log/console.h"
#include "../../rep/service/log/log.h"
#include "../../rep/service/rtos/rtos.h"

#include "system.h"

#define SYSMGR_LOG_TAG "sysmgr"

static u8 gSystemConsoleReady = 0u;

static void systemConsoleServiceInit(void)
{
	if (gSystemConsoleReady != 0u) {
		return;
	}

	if (!consoleInit()) {
		return;
	}

	gSystemConsoleReady = 1u;
	LOG_I(SYSMGR_LOG_TAG, "rtt console ready");
}

static void systemConsoleServiceProcess(void)
{
	if (gSystemConsoleReady == 0u) {
		return;
	}

	consoleProcess();
}

static void systemShowAddress(u8 mode)
{
	
}


static void systemInitMode(void)
{
	systemConsoleServiceInit();
	LOG_I(SYSMGR_LOG_TAG, "system init");
	if (repRtosStatsInit() != REP_RTOS_STATUS_OK) {
		LOG_W(SYSMGR_LOG_TAG, "rtos stats init failed");
	}
	systemShowAddress(0);
	systemSetMode(SYSTEM_MODE_NORMAL);
}

static void systemNormalMode(void)
{
	static uint32_t last_led_tick = 0u;
	uint32_t now_tick;

	now_tick = repRtosGetTickMs();
	if ((now_tick - last_led_tick) >= 500u) {
		last_led_tick = now_tick;
		LED0 = !LED0;
	}

	systemShowAddress(0);
}

void systemManagerRun(void)
{
	systemConsoleServiceInit();

	switch (systemGetMode()) {
	case SYSTEM_MODE_INIT:
		systemInitMode();
		break;
	case SYSTEM_MODE_NORMAL:
		systemNormalMode();
		break;
	default:
		systemSetMode(SYSTEM_MODE_INIT);
		break;
	}

	systemConsoleServiceProcess();
}
