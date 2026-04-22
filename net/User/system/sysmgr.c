#include "sysmgr.h"

#include <stdio.h>

#include "includes.h"
#include "lcd.h"
#include "led.h"
#include "lwip_comm.h"
#include "../../rep/service/console/console.h"
#include "../../rep/service/console/log.h"

#include "system.h"

#define SYSMGR_LOG_TAG "sysmgr"

static u8 gSystemConsoleReady = 0u;

static void systemConsoleServiceInit(void)
{
	if (gSystemConsoleReady != 0u) {
		return;
	}

	if (!logInit()) {
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

	logProcessOutput();
	consoleProcess();
}

static void systemShowAddress(u8 mode)
{
	
}


static void systemInitMode(void)
{
	systemConsoleServiceInit();
	LOG_I(SYSMGR_LOG_TAG, "system init");
	OSStatInit();
#if LWIP_DHCP
	lwip_comm_dhcp_creat();
	systemSetMode(SYSTEM_MODE_DHCP_WAIT);
#else
	systemShowAddress(0);
	systemSetMode(SYSTEM_MODE_NORMAL);
#endif
}

static void systemDhcpWaitMode(void)
{
#if LWIP_DHCP
	if (lwipdev.dhcpstatus != 0) {
		systemShowAddress(lwipdev.dhcpstatus);
		systemSetMode(SYSTEM_MODE_NORMAL);
	}
#else
	systemSetMode(SYSTEM_MODE_NORMAL);
#endif
}

static void systemNormalMode(void)
{
	static OS_TICK last_led_tick = 0u;
	static OS_TICK last_display_tick = 0u;
	OS_TICK now_tick;

	now_tick = OSTimeGet();
	if ((now_tick - last_led_tick) >= 500u) {
		last_led_tick = now_tick;
		LED0 = !LED0;
	}

	if ((now_tick - last_display_tick) >= 500u) {
		last_display_tick = now_tick;
#if LWIP_DHCP
		if (lwipdev.dhcpstatus != 0) {
			systemShowAddress(lwipdev.dhcpstatus);
		}
#else
		systemShowAddress(0);
#endif
	}
}

void systemManagerRun(void)
{
	systemConsoleServiceInit();

	switch (systemGetMode()) {
	case SYSTEM_MODE_INIT:
		systemInitMode();
		break;
	case SYSTEM_MODE_DHCP_WAIT:
		systemDhcpWaitMode();
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
