#include "sysmgr.h"

#include "sys.h"
#include "led.h"
#include "systask.h"
#include "system_storage.h"
#include "core_cm4.h"
#include "../manager/wireless/wireless.h"
#include "../../rep/driver/drvgpio/drvgpio.h"
#include "../../rep/service/log/console.h"
#include "../../rep/service/log/log.h"
#include "../../rep/service/rtos/rtos.h"

#include "../port/drvgpio_port.h"
#include "system.h"

#define SYSMGR_LOG_TAG "sysmgr"

static u8 gSystemConsoleReady = 0u;
static u8 gSystemInitStarted = 0u;
static u8 gSystemGpioReady = 0u;
static eDrvGpioPinState gSystemLastWifiEnState = DRVGPIO_PIN_STATE_INVALID;

static eConsoleCommandResult systemConsoleRebootHandler(uint32_t transport, int argc, char *argv[]);

static const stConsoleCommand gSystemRebootConsoleCommand = {
	.commandName = "reboot",
	.helpText = "reboot - software reset the MCU",
	.ownerTag = "sys",
	.handler = systemConsoleRebootHandler,
};

static eConsoleCommandResult systemConsoleRebootHandler(uint32_t transport, int argc, char *argv[])
{
	uint32_t lAttempt;
	(void)argv;

	if (argc != 1) {
		return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
	}

	for (lAttempt = 0U; lAttempt < 5U; lAttempt++) {
		if (logDirectWriteText(transport, "rebooting...\n") > 0) {
			(void)repRtosDelayMs(20U);
			__DSB();
			NVIC_SystemReset();
			return CONSOLE_COMMAND_RESULT_OK;
		}

		logProcessOutput();
		(void)repRtosDelayMs(10U);
	}

	return CONSOLE_COMMAND_RESULT_ERROR;
}

static void systemTraceWifiEnState(void)
{
	eDrvGpioPinState lState;

	lState = drvGpioRead(DRVGPIO_WIFI_EN);
	if (lState == gSystemLastWifiEnState) {
		return;
	}

	gSystemLastWifiEnState = lState;
	if (lState == DRVGPIO_PIN_SET) {
		LOG_I(SYSMGR_LOG_TAG, "wifi_en=SET (PD8 high, module enabled)");
	} else if (lState == DRVGPIO_PIN_RESET) {
		LOG_I(SYSMGR_LOG_TAG, "wifi_en=RESET (PD8 low, module held reset/disable)");
	}
}

static void systemGpioServiceInit(void)
{
	if (gSystemGpioReady != 0u) {
		return;
	}

	drvGpioInit();
	gSystemGpioReady = 1u;
}

static void systemConsoleServiceInit(void)
{
	if (gSystemConsoleReady != 0u) {
		return;
	}

	if (!consoleInit()) {
		return;
	}

	if (!consoleRegisterCommand(&gSystemRebootConsoleCommand)) {
		return;
	}

	gSystemConsoleReady = 1u;
	LOG_I(SYSMGR_LOG_TAG, "USART2 PA2/PA3 reserved for wifi, debug logs use RTT");
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
	systemGpioServiceInit();
	if (gSystemInitStarted == 0u) {
		gSystemInitStarted = 1u;
		LOG_I(SYSMGR_LOG_TAG, "system init");
		if (repRtosStatsInit() != REP_RTOS_STATUS_OK) {
			LOG_W(SYSMGR_LOG_TAG, "rtos stats init failed");
		}
	}

	if (!systemStorageIsReady()) {
		return;
	}

	if (systemStorageIsBusy()) {
		return;
	}

	systaskCreateWorkerTasks();

	systemTraceWifiEnState();
	if (!wirelessIsReady()) {
		return;
	}

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

	systemTraceWifiEnState();

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
