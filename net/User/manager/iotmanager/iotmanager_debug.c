/***********************************************************************************
* @file     : iotmanager_debug.c
* @brief    : Iot manager debug and console helpers.
* @details  : Provides interface selection and status query utilities for bring-up.
* @author   : GitHub Copilot
* @date     : 2026-04-24
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "iotmanager_debug.h"

#include <string.h>

#include "../wireless/wireless.h"
#include "../../../rep/service/log/console.h"

static bool iotManagerDebugTryParseLink(const char *text, eIotManagerLinkId *linkId);
static bool iotManagerDebugTryParseService(const char *text, eIotManagerServiceId *serviceId);
static eConsoleCommandResult iotManagerDebugReplyHelp(uint32_t transport);
static eConsoleCommandResult iotManagerDebugReplyStatus(uint32_t transport);
static eConsoleCommandResult iotManagerDebugHandlePub(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult iotManagerDebugHandleRetry(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult iotManagerDebugConsoleHandler(uint32_t transport, int argc, char *argv[]);

#if (IOT_MANAGER_DEBUG_CONSOLE_SUPPORT == 1)
static const stConsoleCommand gIotManagerDebugConsoleCommand = {
	.commandName = "iot",
	.helpText = "iot <status|route|select|pub|retry|help> ...",
	.ownerTag = "iotmgr",
	.handler = iotManagerDebugConsoleHandler,
};
#endif

bool iotManagerDebugSelectRoute(eIotManagerServiceId serviceId, eIotManagerLinkId linkId)
{
	return iotManagerSelectRoute(serviceId, linkId);
}

bool iotManagerDebugGetStateSnapshot(stIotManagerState *state)
{
	const stIotManagerState *lState;

	if (state == NULL) {
		return false;
	}

	lState = iotManagerGetState();
	if (lState == NULL) {
		return false;
	}

	(void)memcpy(state, lState, sizeof(*state));
	return true;
}

const char *iotManagerDebugGetLinkName(eIotManagerLinkId linkId)
{
	switch (linkId) {
	case IOT_MANAGER_LINK_NONE:
		return "none";
	case IOT_MANAGER_LINK_BLE:
		return "ble";
	case IOT_MANAGER_LINK_WIFI:
		return "wifi";
	case IOT_MANAGER_LINK_CELLULAR:
		return "cellular";
	case IOT_MANAGER_LINK_ETHERNET:
		return "ethernet";
	default:
		return "invalid";
	}
}

const char *iotManagerDebugGetServiceName(eIotManagerServiceId serviceId)
{
	switch (serviceId) {
	case IOT_MANAGER_SERVICE_BLE_LOCAL:
		return "ble";
	case IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP:
		return "mqttauth";
	case IOT_MANAGER_SERVICE_MQTT:
		return "mqtt";
	case IOT_MANAGER_SERVICE_TCP_SERVER:
		return "tcp";
	default:
		return "invalid";
	}
}

const char *iotManagerDebugGetLinkStateName(eIotManagerLinkState state)
{
	switch (state) {
	case IOT_MANAGER_LINK_STATE_ABSENT:
		return "absent";
	case IOT_MANAGER_LINK_STATE_DISABLED:
		return "disabled";
	case IOT_MANAGER_LINK_STATE_INITING:
		return "initing";
	case IOT_MANAGER_LINK_STATE_READY:
		return "ready";
	case IOT_MANAGER_LINK_STATE_NET_CONNECTING:
		return "net-connecting";
	case IOT_MANAGER_LINK_STATE_NET_READY:
		return "net-ready";
	case IOT_MANAGER_LINK_STATE_SERVICE_CONNECTING:
		return "svc-connecting";
	case IOT_MANAGER_LINK_STATE_SERVICE_READY:
		return "svc-ready";
	case IOT_MANAGER_LINK_STATE_DEGRADED:
		return "degraded";
	case IOT_MANAGER_LINK_STATE_ERROR:
		return "error";
	default:
		return "invalid";
	}
}

const char *iotManagerDebugGetServiceStateName(eIotManagerServiceState state)
{
	switch (state) {
	case IOT_MANAGER_SERVICE_STATE_IDLE:
		return "idle";
	case IOT_MANAGER_SERVICE_STATE_WAIT_LINK:
		return "wait-link";
	case IOT_MANAGER_SERVICE_STATE_CONNECTING:
		return "connecting";
	case IOT_MANAGER_SERVICE_STATE_READY:
		return "ready";
	case IOT_MANAGER_SERVICE_STATE_BACKOFF:
		return "backoff";
	case IOT_MANAGER_SERVICE_STATE_ERROR:
		return "error";
	default:
		return "invalid";
	}
}

static bool iotManagerDebugTryParseLink(const char *text, eIotManagerLinkId *linkId)
{
	if ((text == NULL) || (linkId == NULL)) {
		return false;
	}

	if ((strcmp(text, "none") == 0) || (strcmp(text, "off") == 0)) {
		*linkId = IOT_MANAGER_LINK_NONE;
		return true;
	}

	if ((strcmp(text, "ble") == 0) || (strcmp(text, "wireless") == 0)) {
		*linkId = IOT_MANAGER_LINK_BLE;
		return true;
	}

	if (strcmp(text, "wifi") == 0) {
		*linkId = IOT_MANAGER_LINK_WIFI;
		return true;
	}

	if ((strcmp(text, "cellular") == 0) || (strcmp(text, "cell") == 0) ||
		(strcmp(text, "4g") == 0) || (strcmp(text, "5g") == 0)) {
		*linkId = IOT_MANAGER_LINK_CELLULAR;
		return true;
	}

	if ((strcmp(text, "ethernet") == 0) || (strcmp(text, "eth") == 0) || (strcmp(text, "lan") == 0)) {
		*linkId = IOT_MANAGER_LINK_ETHERNET;
		return true;
	}

	return false;
}

static bool iotManagerDebugTryParseService(const char *text, eIotManagerServiceId *serviceId)
{
	if ((text == NULL) || (serviceId == NULL)) {
		return false;
	}

	if ((strcmp(text, "ble") == 0) || (strcmp(text, "ble_local") == 0)) {
		*serviceId = IOT_MANAGER_SERVICE_BLE_LOCAL;
		return true;
	}

	if ((strcmp(text, "mqttauth") == 0) || (strcmp(text, "auth") == 0) || (strcmp(text, "http") == 0)) {
		*serviceId = IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP;
		return true;
	}

	if (strcmp(text, "mqtt") == 0) {
		*serviceId = IOT_MANAGER_SERVICE_MQTT;
		return true;
	}

	if ((strcmp(text, "tcp") == 0) || (strcmp(text, "tcpserver") == 0)) {
		*serviceId = IOT_MANAGER_SERVICE_TCP_SERVER;
		return true;
	}

	return false;
}

static eConsoleCommandResult iotManagerDebugReplyHelp(uint32_t transport)
{
	if (consoleReply(transport,
		"iot status\n"
		"iot pub <text>\n"
		"iot retry\n"
		"iot route <ble|mqttauth|mqtt|tcp> <none|ble|wifi|cellular|ethernet>\n"
		"iot select <none|ble|cellular|ethernet>\n"
		"iot help\n"
		"OK") <= 0) {
		return CONSOLE_COMMAND_RESULT_ERROR;
	}

	return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult iotManagerDebugReplyStatus(uint32_t transport)
{
	stIotManagerState lState;
	uint32_t lIndex;
	const stIotManagerServiceRoute *lRoutes[4];

	if (!iotManagerDebugGetStateSnapshot(&lState)) {
		return CONSOLE_COMMAND_RESULT_ERROR;
	}

	if (consoleReply(transport,
		"cloudReady=%u bleReady=%u mqttAuthDone=%u cellular=%s",
		lState.cloudAnyReady ? 1U : 0U,
		lState.localBleReady ? 1U : 0U,
		lState.mqttAuthDone ? 1U : 0U,
		(lState.installedCellularType == IOT_MANAGER_CELLULAR_4G) ? "4g" :
		(lState.installedCellularType == IOT_MANAGER_CELLULAR_5G) ? "5g" : "none") <= 0) {
		return CONSOLE_COMMAND_RESULT_ERROR;
	}

	for (lIndex = (uint32_t)IOT_MANAGER_LINK_BLE; lIndex < (uint32_t)IOT_MANAGER_LINK_MAX; ++lIndex) {
		const stIotManagerLinkRuntime *lRuntime;

		lRuntime = &lState.links[lIndex];
		if (consoleReply(transport,
			"\nlink[%s]: state=%s installed=%u enabled=%u selected=%u module=%u net=%u peer=%u auth=%u mqtt=%u tcpListen=%u tcpClient=%u ok=%lu fail=%lu",
			iotManagerDebugGetLinkName((eIotManagerLinkId)lIndex),
			iotManagerDebugGetLinkStateName(lRuntime->state),
			lRuntime->installed ? 1U : 0U,
			lRuntime->enabled ? 1U : 0U,
			lRuntime->selected ? 1U : 0U,
			lRuntime->moduleReady ? 1U : 0U,
			lRuntime->netReady ? 1U : 0U,
			lRuntime->peerConnected ? 1U : 0U,
			lRuntime->mqttAuthReady ? 1U : 0U,
			lRuntime->mqttReady ? 1U : 0U,
			lRuntime->tcpServerListening ? 1U : 0U,
			lRuntime->tcpClientConnected ? 1U : 0U,
			(unsigned long)lRuntime->lastOkTick,
			(unsigned long)lRuntime->lastFailTick) <= 0) {
			return CONSOLE_COMMAND_RESULT_ERROR;
		}
	}

	lRoutes[0] = &lState.bleLocalRoute;
	lRoutes[1] = &lState.mqttAuthRoute;
	lRoutes[2] = &lState.mqttRoute;
	lRoutes[3] = &lState.tcpServerRoute;
	for (lIndex = 0U; lIndex < 4U; ++lIndex) {
		const stIotManagerServiceRoute *lRoute;

		lRoute = lRoutes[lIndex];
		if (consoleReply(transport,
			"\nroute[%s]: active=%s preferred=%s policy=%u state=%s",
			iotManagerDebugGetServiceName(lRoute->serviceId),
			iotManagerDebugGetLinkName(lRoute->activeLink),
			iotManagerDebugGetLinkName(lRoute->preferredLink),
			(unsigned int)lRoute->policy,
			iotManagerDebugGetServiceStateName(lRoute->state)) <= 0) {
			return CONSOLE_COMMAND_RESULT_ERROR;
		}
	}

	if (consoleReply(transport, "\nOK") <= 0) {
		return CONSOLE_COMMAND_RESULT_ERROR;
	}

	return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult iotManagerDebugHandlePub(uint32_t transport, int argc, char *argv[])
{
	char payload[160];
	uint16_t offset;
	int index;
	uint16_t partLen;

	if ((argc < 3) || (argv == NULL)) {
		return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
	}

	offset = 0U;
	for (index = 2; index < argc; ++index) {
		if (argv[index] == NULL) {
			return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
		}

		partLen = (uint16_t)strlen(argv[index]);
		if ((index > 2) && (offset < (uint16_t)(sizeof(payload) - 1U))) {
			payload[offset++] = ' ';
		}
		if ((uint16_t)(offset + partLen) >= sizeof(payload)) {
			return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
		}
		(void)memcpy(&payload[offset], argv[index], partLen);
		offset = (uint16_t)(offset + partLen);
	}

	if (offset == 0U) {
		return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
	}
	payload[offset] = '\0';

	if (!iotManagerSendByLink(IOT_MANAGER_LINK_WIFI, (const uint8_t *)payload, offset)) {
		return CONSOLE_COMMAND_RESULT_ERROR;
	}

	if (consoleReply(transport, "mqtt pub ok len=%u\nOK", (unsigned int)offset) <= 0) {
		return CONSOLE_COMMAND_RESULT_ERROR;
	}

	return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult iotManagerDebugHandleRetry(uint32_t transport, int argc, char *argv[])
{
	if (argc != 2) {
		return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
	}

	if (!wirelessRequestIotRetry()) {
		return CONSOLE_COMMAND_RESULT_ERROR;
	}

	if (consoleReply(transport, "iot retry armed\nOK") <= 0) {
		return CONSOLE_COMMAND_RESULT_ERROR;
	}

	return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult iotManagerDebugConsoleHandler(uint32_t transport, int argc, char *argv[])
{
	eIotManagerServiceId lServiceId;
	eIotManagerLinkId lLinkId;

	if ((argc <= 1) || (argv == NULL)) {
		return iotManagerDebugReplyHelp(transport);
	}

	if (strcmp(argv[1], "status") == 0) {
		if (argc != 2) {
			return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
		}

		return iotManagerDebugReplyStatus(transport);
	}

	if (strcmp(argv[1], "pub") == 0) {
		return iotManagerDebugHandlePub(transport, argc, argv);
	}

	if (strcmp(argv[1], "retry") == 0) {
		return iotManagerDebugHandleRetry(transport, argc, argv);
	}

	if (strcmp(argv[1], "route") == 0) {
		if ((argc != 4) ||
			!iotManagerDebugTryParseService(argv[2], &lServiceId) ||
			!iotManagerDebugTryParseLink(argv[3], &lLinkId)) {
			return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
		}

		if (!iotManagerDebugSelectRoute(lServiceId, lLinkId)) {
			return CONSOLE_COMMAND_RESULT_ERROR;
		}

		if (consoleReply(transport,
			"route[%s]=%s\nOK",
			iotManagerDebugGetServiceName(lServiceId),
			iotManagerDebugGetLinkName(lLinkId)) <= 0) {
			return CONSOLE_COMMAND_RESULT_ERROR;
		}

		return CONSOLE_COMMAND_RESULT_OK;
	}

	if (strcmp(argv[1], "select") == 0) {
		if ((argc != 3) || !iotManagerDebugTryParseLink(argv[2], &lLinkId)) {
			return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
		}

		if (lLinkId == IOT_MANAGER_LINK_BLE) {
			if (!iotManagerDebugSelectRoute(IOT_MANAGER_SERVICE_BLE_LOCAL, lLinkId)) {
				return CONSOLE_COMMAND_RESULT_ERROR;
			}
		} else {
			if (!iotManagerDebugSelectRoute(IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP, lLinkId) ||
				!iotManagerDebugSelectRoute(IOT_MANAGER_SERVICE_MQTT, lLinkId)) {
				return CONSOLE_COMMAND_RESULT_ERROR;
			}
			if ((lLinkId == IOT_MANAGER_LINK_NONE) ||
				(lLinkId == IOT_MANAGER_LINK_WIFI) ||
				(lLinkId == IOT_MANAGER_LINK_ETHERNET)) {
				if (!iotManagerDebugSelectRoute(IOT_MANAGER_SERVICE_TCP_SERVER, lLinkId)) {
					return CONSOLE_COMMAND_RESULT_ERROR;
				}
			}
		}

		if (lLinkId == IOT_MANAGER_LINK_NONE) {
			if (!iotManagerDebugSelectRoute(IOT_MANAGER_SERVICE_BLE_LOCAL, lLinkId)) {
				return CONSOLE_COMMAND_RESULT_ERROR;
			}
		}

		if (consoleReply(transport,
			"selected=%s\nOK",
			iotManagerDebugGetLinkName(lLinkId)) <= 0) {
			return CONSOLE_COMMAND_RESULT_ERROR;
		}

		return CONSOLE_COMMAND_RESULT_OK;
	}

	if (strcmp(argv[1], "help") == 0) {
		return iotManagerDebugReplyHelp(transport);
	}

	return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}

bool iotManagerDebugConsoleRegister(void)
{
#if (IOT_MANAGER_DEBUG_CONSOLE_SUPPORT == 1)
	return consoleRegisterCommand(&gIotManagerDebugConsoleCommand);
#else
	return true;
#endif
}

/**************************End of file********************************/
