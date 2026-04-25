/***********************************************************************************
* @file     : wireless_debug.c
* @brief    : Wireless debug and console helpers.
* @details  : Provides RTT commands for BLE, WiFi, and MQTT switch control.
* @author   : GitHub Copilot
* @date     : 2026-04-25
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "wireless_debug.h"

#include <string.h>

#include "wireless_ble.h"
#include "wireless_wifi.h"
#include "../../../rep/service/log/console.h"

static const char *wirelessDebugGetBleStateName(enum bleState state);
static const char *wirelessDebugGetWifiStateName(enum wifiState state);
static const char *wirelessDebugGetIotStateName(eWirelessIotState state);
static eConsoleCommandResult wirelessDebugReplyHelp(uint32_t transport);
static eConsoleCommandResult wirelessDebugReplyStatus(uint32_t transport);
static eConsoleCommandResult wirelessDebugConsoleHandler(uint32_t transport, int argc, char *argv[]);

#if (WIRELESS_DEBUG_CONSOLE_SUPPORT == 1)
static const stConsoleCommand gWirelessDebugConsoleCommand = {
	.commandName = "wireless",
	.helpText = "wireless <status|ble_on|ble_off|wifi_on|wifi_off|wifi_connect|mqtt_on|mqtt_off>",
	.ownerTag = "wireless",
	.handler = wirelessDebugConsoleHandler,
};
#endif

static const char *wirelessDebugGetBleStateName(enum bleState state)
{
	switch (state) {
	case BLE_STATE_IDLE:
		return "idle";
	case BLE_STATE_INITIALIZING:
		return "initializing";
	case BLE_STATE_READY:
		return "ready";
	case BLE_WAITING_CONNECTION:
		return "waiting";
	case BLE_STATE_DISCONNECTED:
		return "disconnected";
	case BLE_STATE_CONNECTED:
		return "connected";
	case BLE_STATE_ERROR:
		return "error";
	default:
		break;
	}

	return "invalid";
}

static const char *wirelessDebugGetWifiStateName(enum wifiState state)
{
	switch (state) {
	case WIFI_STATE_IDLE:
		return "idle";
	case WIFI_STATE_INITIALIZING:
		return "initializing";
	case WIFI_STATE_READY:
		return "ready";
	case WIFI_STATE_SCANNING:
		return "scanning";
	case WIFI_STATE_WAITING_CONNECTION:
		return "waiting";
	case WIFI_STATE_DISCONNECTED:
		return "disconnected";
	case WIFI_STATE_CONNECTED:
		return "connected";
	case WIFI_STATE_ERROR:
		return "error";
	default:
		break;
	}

	return "invalid";
}

static const char *wirelessDebugGetIotStateName(eWirelessIotState state)
{
	switch (state) {
	case WIRELESS_IOT_STATE_IDLE:
		return "idle";
	case WIRELESS_IOT_STATE_WAIT_WIFI:
		return "wait-wifi";
	case WIRELESS_IOT_STATE_WAIT_AUTH:
		return "wait-auth";
	case WIRELESS_IOT_STATE_AUTH_READY:
		return "auth-ready";
	case WIRELESS_IOT_STATE_MQTT_CONNECTING:
		return "mqtt-connecting";
	case WIRELESS_IOT_STATE_MQTT_READY:
		return "mqtt-ready";
	case WIRELESS_IOT_STATE_ERROR:
		return "error";
	default:
		break;
	}

	return "invalid";
}

static eConsoleCommandResult wirelessDebugReplyHelp(uint32_t transport)
{
	if (consoleReply(transport,
		"wireless status\n"
		"wireless ble_on\n"
		"wireless ble_off\n"
		"wireless wifi_on\n"
		"wireless wifi_off\n"
		"wireless wifi_connect <ssid> <password>\n"
		"wireless mqtt_on\n"
		"wireless mqtt_off\n"
		"OK") <= 0) {
		return CONSOLE_COMMAND_RESULT_ERROR;
	}

	return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult wirelessDebugReplyStatus(uint32_t transport)
{
	if (consoleReply(transport,
		"bleEnabled=%u ble=%s wifiEnabled=%u wifi=%s mqttEnabled=%u iot=%s ready=%u\nOK",
		wirelessGetBleEnabled() ? 1U : 0U,
		wirelessDebugGetBleStateName(wirelessGetBleState()),
		wirelessGetWifiEnabled() ? 1U : 0U,
		wirelessDebugGetWifiStateName(wirelessGetWifiState()),
		wirelessGetMqttEnabled() ? 1U : 0U,
		wirelessDebugGetIotStateName(wirelessGetIotState()),
		wirelessIsReady() ? 1U : 0U) <= 0) {
		return CONSOLE_COMMAND_RESULT_ERROR;
	}

	return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult wirelessDebugConsoleHandler(uint32_t transport, int argc, char *argv[])
{
	if ((argc <= 1) || (argv == NULL)) {
		return wirelessDebugReplyHelp(transport);
	}

	if (strcmp(argv[1], "status") == 0) {
		return (argc == 2) ? wirelessDebugReplyStatus(transport) : CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
	}

	if (strcmp(argv[1], "ble_on") == 0) {
		if ((argc != 2) || !wirelessSetBleEnabled(true)) {
			return CONSOLE_COMMAND_RESULT_ERROR;
		}
		return wirelessDebugReplyStatus(transport);
	}

	if (strcmp(argv[1], "ble_off") == 0) {
		if ((argc != 2) || !wirelessSetBleEnabled(false)) {
			return CONSOLE_COMMAND_RESULT_ERROR;
		}
		return wirelessDebugReplyStatus(transport);
	}

	if (strcmp(argv[1], "wifi_on") == 0) {
		if ((argc != 2) || !wirelessSetWifiEnabled(true)) {
			return CONSOLE_COMMAND_RESULT_ERROR;
		}
		return wirelessDebugReplyStatus(transport);
	}

	if (strcmp(argv[1], "wifi_off") == 0) {
		if ((argc != 2) || !wirelessSetWifiEnabled(false)) {
			return CONSOLE_COMMAND_RESULT_ERROR;
		}
		return wirelessDebugReplyStatus(transport);
	}

	if (strcmp(argv[1], "wifi_connect") == 0) {
		if ((argc != 4) ||
			!wirelessConnectWifi((const uint8_t *)argv[2],
						 (uint8_t)strlen(argv[2]),
						 (const uint8_t *)argv[3],
						 (uint8_t)strlen(argv[3]))) {
			return CONSOLE_COMMAND_RESULT_ERROR;
		}
		return wirelessDebugReplyStatus(transport);
	}

	if (strcmp(argv[1], "mqtt_on") == 0) {
		if ((argc != 2) || !wirelessSetMqttEnabled(true)) {
			return CONSOLE_COMMAND_RESULT_ERROR;
		}
		return wirelessDebugReplyStatus(transport);
	}

	if (strcmp(argv[1], "mqtt_off") == 0) {
		if ((argc != 2) || !wirelessSetMqttEnabled(false)) {
			return CONSOLE_COMMAND_RESULT_ERROR;
		}
		return wirelessDebugReplyStatus(transport);
	}

	if (strcmp(argv[1], "help") == 0) {
		return wirelessDebugReplyHelp(transport);
	}

	return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}

bool wirelessDebugConsoleRegister(void)
{
#if (WIRELESS_DEBUG_CONSOLE_SUPPORT == 1)
	return consoleRegisterCommand(&gWirelessDebugConsoleCommand);
#else
	return true;
#endif
}

/**************************End of file********************************/
