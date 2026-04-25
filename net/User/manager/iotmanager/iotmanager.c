/***********************************************************************************
* @file     : iotmanager.c
* @brief    : CPR sensor IoT manager.
* @details  : Receives packets from transport managers, decodes the CPR sensor
*             protocol, and routes replies through the selected transport.
* @author   : GitHub Copilot
* @date     : 2026-04-24
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "iotmanager.h"
#include "protcolmgr.h"

#include <string.h>

#include "../cellular/cellular.h"
#include "../ethernet/ethernet.h"
#include "../wireless/wireless.h"
#include "../../../rep/service/rtos/rtos.h"

typedef struct stIotManagerContext {
	stIotManagerState publicState;
	bool initialized;
} stIotManagerContext;

static stIotManagerContext gIotManagerState;

static const stIotManagerLinkCaps gIotManagerDefaultLinkCaps[IOT_MANAGER_LINK_MAX] = {
	{ false, false, false, false },
	{ true, false, false, false },
	{ false, true, true, true },
	{ false, true, true, false },
	{ false, true, true, true },
};

static eIotManagerLinkId iotManagerInterfaceToLink(eIotManagerInterface interfaceType);
static bool iotManagerIsValidLink(eIotManagerLinkId linkId);
static bool iotManagerIsValidInterface(eIotManagerInterface interfaceType);
static bool iotManagerLinkCapsIsEmpty(const stIotManagerLinkCaps *caps);
static stIotManagerServiceRoute *iotManagerGetRouteByServiceId(eIotManagerServiceId serviceId);
static bool iotManagerLinkSupportsService(eIotManagerLinkId linkId, eIotManagerServiceId serviceId);
static bool iotManagerLinkCanRunService(const stIotManagerLinkRuntime *runtime, eIotManagerServiceId serviceId);
static void iotManagerInitRoute(stIotManagerServiceRoute *route, eIotManagerServiceId serviceId, eIotManagerLinkId preferredLink, eIotManagerSendPolicy policy);
static void iotManagerInitLinkRuntime(stIotManagerLinkRuntime *runtime, eIotManagerLinkId linkId);
void iotManagerEnsureStateInitialized(void);
static eIotManagerLinkId iotManagerPickAutoLink(eIotManagerServiceId serviceId);
static void iotManagerRefreshRouteLocked(stIotManagerServiceRoute *route);
static void iotManagerRefreshStateLocked(void);
static bool iotManagerApplyCompatibilitySelectionLocked(eIotManagerLinkId linkId);

static eIotManagerLinkId iotManagerInterfaceToLink(eIotManagerInterface interfaceType)
{
	switch (interfaceType) {
	case IOT_MANAGER_INTERFACE_WIRELESS:
		return IOT_MANAGER_LINK_BLE;
	case IOT_MANAGER_INTERFACE_CELLULAR:
		return IOT_MANAGER_LINK_CELLULAR;
	case IOT_MANAGER_INTERFACE_ETHERNET:
		return IOT_MANAGER_LINK_ETHERNET;
	default:
		break;
	}

	return IOT_MANAGER_LINK_NONE;
}

static bool iotManagerIsValidLink(eIotManagerLinkId linkId)
{
	return (linkId > IOT_MANAGER_LINK_NONE) && (linkId < IOT_MANAGER_LINK_MAX);
}

static bool iotManagerIsValidInterface(eIotManagerInterface interfaceType)
{
	return iotManagerIsValidLink(iotManagerInterfaceToLink(interfaceType));
}

static bool iotManagerLinkCapsIsEmpty(const stIotManagerLinkCaps *caps)
{
	if (caps == NULL) {
		return true;
	}

	return !caps->supportBleLocal && !caps->supportMqttAuthHttp && !caps->supportMqtt && !caps->supportTcpServer;
}

static stIotManagerServiceRoute *iotManagerGetRouteByServiceId(eIotManagerServiceId serviceId)
{
	switch (serviceId) {
	case IOT_MANAGER_SERVICE_BLE_LOCAL:
		return &gIotManagerState.publicState.bleLocalRoute;
	case IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP:
		return &gIotManagerState.publicState.mqttAuthRoute;
	case IOT_MANAGER_SERVICE_MQTT:
		return &gIotManagerState.publicState.mqttRoute;
	case IOT_MANAGER_SERVICE_TCP_SERVER:
		return &gIotManagerState.publicState.tcpServerRoute;
	default:
		break;
	}

	return NULL;
}

static bool iotManagerLinkSupportsService(eIotManagerLinkId linkId, eIotManagerServiceId serviceId)
{
	const stIotManagerLinkCaps *lCaps;

	if (!iotManagerIsValidLink(linkId)) {
		return false;
	}

	lCaps = &gIotManagerState.publicState.links[linkId].caps;
	switch (serviceId) {
	case IOT_MANAGER_SERVICE_BLE_LOCAL:
		return lCaps->supportBleLocal;
	case IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP:
		return lCaps->supportMqttAuthHttp;
	case IOT_MANAGER_SERVICE_MQTT:
		return lCaps->supportMqtt;
	case IOT_MANAGER_SERVICE_TCP_SERVER:
		return lCaps->supportTcpServer;
	default:
		break;
	}

	return false;
}

static bool iotManagerLinkCanRunService(const stIotManagerLinkRuntime *runtime, eIotManagerServiceId serviceId)
{
	if ((runtime == NULL) || !runtime->installed || !runtime->enabled) {
		return false;
	}

	switch (serviceId) {
	case IOT_MANAGER_SERVICE_BLE_LOCAL:
		return runtime->caps.supportBleLocal && (runtime->peerConnected || runtime->moduleReady);
	case IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP:
		return runtime->caps.supportMqttAuthHttp && (runtime->mqttAuthReady || runtime->netReady);
	case IOT_MANAGER_SERVICE_MQTT:
		return runtime->caps.supportMqtt && (runtime->mqttReady || runtime->netReady);
	case IOT_MANAGER_SERVICE_TCP_SERVER:
		return runtime->caps.supportTcpServer && (runtime->tcpServerListening || runtime->netReady);
	default:
		break;
	}

	return false;
}

static void iotManagerInitRoute(stIotManagerServiceRoute *route, eIotManagerServiceId serviceId, eIotManagerLinkId preferredLink, eIotManagerSendPolicy policy)
{
	if (route == NULL) {
		return;
	}

	(void)memset(route, 0, sizeof(*route));
	route->serviceId = serviceId;
	route->preferredLink = preferredLink;
	route->policy = policy;
	route->state = IOT_MANAGER_SERVICE_STATE_WAIT_LINK;
}

static void iotManagerInitLinkRuntime(stIotManagerLinkRuntime *runtime, eIotManagerLinkId linkId)
{
	if (runtime == NULL) {
		return;
	}

	(void)memset(runtime, 0, sizeof(*runtime));
	runtime->linkId = linkId;
	runtime->state = IOT_MANAGER_LINK_STATE_DISABLED;
	runtime->caps = gIotManagerDefaultLinkCaps[linkId];
	runtime->installed = (linkId != IOT_MANAGER_LINK_WIFI);
	runtime->enabled = runtime->installed;
	if (linkId == IOT_MANAGER_LINK_CELLULAR) {
		runtime->cellularType = IOT_MANAGER_CELLULAR_NONE;
	}
}

void iotManagerEnsureStateInitialized(void)
{
	uint32_t lIndex;

	if (gIotManagerState.initialized) {
		return;
	}

	(void)memset(&gIotManagerState.publicState, 0, sizeof(gIotManagerState.publicState));
	for (lIndex = 0U; lIndex < (uint32_t)IOT_MANAGER_LINK_MAX; ++lIndex) {
		iotManagerInitLinkRuntime(&gIotManagerState.publicState.links[lIndex], (eIotManagerLinkId)lIndex);
	}
	gIotManagerState.publicState.links[IOT_MANAGER_LINK_NONE].state = IOT_MANAGER_LINK_STATE_ABSENT;
	iotManagerInitRoute(&gIotManagerState.publicState.bleLocalRoute,
			     IOT_MANAGER_SERVICE_BLE_LOCAL,
			     IOT_MANAGER_LINK_BLE,
			     IOT_MANAGER_SEND_POLICY_FIXED);
	iotManagerInitRoute(&gIotManagerState.publicState.mqttAuthRoute,
			     IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP,
			     IOT_MANAGER_LINK_ETHERNET,
			     IOT_MANAGER_SEND_POLICY_AUTO);
	iotManagerInitRoute(&gIotManagerState.publicState.mqttRoute,
			     IOT_MANAGER_SERVICE_MQTT,
			     IOT_MANAGER_LINK_ETHERNET,
			     IOT_MANAGER_SEND_POLICY_AUTO);
	iotManagerInitRoute(&gIotManagerState.publicState.tcpServerRoute,
			     IOT_MANAGER_SERVICE_TCP_SERVER,
			     IOT_MANAGER_LINK_ETHERNET,
			     IOT_MANAGER_SEND_POLICY_AUTO);
	gIotManagerState.initialized = true;
	iotManagerRefreshStateLocked();
}

static eIotManagerLinkId iotManagerPickAutoLink(eIotManagerServiceId serviceId)
{
	static const eIotManagerLinkId lCloudPriority[] = {
		IOT_MANAGER_LINK_ETHERNET,
		IOT_MANAGER_LINK_WIFI,
		IOT_MANAGER_LINK_CELLULAR,
	};
	uint32_t lIndex;

	if (serviceId == IOT_MANAGER_SERVICE_BLE_LOCAL) {
		return iotManagerLinkCanRunService(&gIotManagerState.publicState.links[IOT_MANAGER_LINK_BLE], serviceId) ?
			IOT_MANAGER_LINK_BLE : IOT_MANAGER_LINK_NONE;
	}

	for (lIndex = 0U; lIndex < (uint32_t)(sizeof(lCloudPriority) / sizeof(lCloudPriority[0])); ++lIndex) {
		eIotManagerLinkId lLinkId;

		lLinkId = lCloudPriority[lIndex];
		if (iotManagerLinkCanRunService(&gIotManagerState.publicState.links[lLinkId], serviceId)) {
			return lLinkId;
		}
	}

	return IOT_MANAGER_LINK_NONE;
}

static void iotManagerRefreshRouteLocked(stIotManagerServiceRoute *route)
{
	eIotManagerLinkId lCandidate;

	if (route == NULL) {
		return;
	}

	lCandidate = IOT_MANAGER_LINK_NONE;
	if ((route->preferredLink != IOT_MANAGER_LINK_NONE) && iotManagerIsValidLink(route->preferredLink) &&
		iotManagerLinkSupportsService(route->preferredLink, route->serviceId) &&
		iotManagerLinkCanRunService(&gIotManagerState.publicState.links[route->preferredLink], route->serviceId)) {
		lCandidate = route->preferredLink;
	} else if (route->policy == IOT_MANAGER_SEND_POLICY_AUTO) {
		lCandidate = iotManagerPickAutoLink(route->serviceId);
	}

	route->activeLink = lCandidate;
	if (lCandidate != IOT_MANAGER_LINK_NONE) {
		route->state = IOT_MANAGER_SERVICE_STATE_READY;
	} else if ((route->preferredLink != IOT_MANAGER_LINK_NONE) && (route->policy != IOT_MANAGER_SEND_POLICY_AUTO)) {
		route->state = IOT_MANAGER_SERVICE_STATE_ERROR;
	} else {
		route->state = IOT_MANAGER_SERVICE_STATE_WAIT_LINK;
	}
}

static void iotManagerRefreshStateLocked(void)
{
	uint32_t lIndex;
	stIotManagerServiceRoute *lRoutes[4];

	lRoutes[0] = &gIotManagerState.publicState.bleLocalRoute;
	lRoutes[1] = &gIotManagerState.publicState.mqttAuthRoute;
	lRoutes[2] = &gIotManagerState.publicState.mqttRoute;
	lRoutes[3] = &gIotManagerState.publicState.tcpServerRoute;

	for (lIndex = 0U; lIndex < (uint32_t)IOT_MANAGER_LINK_MAX; ++lIndex) {
		gIotManagerState.publicState.links[lIndex].selected = false;
	}

	for (lIndex = 0U; lIndex < 4U; ++lIndex) {
		stIotManagerServiceRoute *lRoute;

		lRoute = lRoutes[lIndex];
		iotManagerRefreshRouteLocked(lRoute);
		if (iotManagerIsValidLink(lRoute->activeLink)) {
			gIotManagerState.publicState.links[lRoute->activeLink].selected = true;
		}
	}

	gIotManagerState.publicState.installedCellularType =
		gIotManagerState.publicState.links[IOT_MANAGER_LINK_CELLULAR].cellularType;
	gIotManagerState.publicState.localBleReady =
		iotManagerLinkCanRunService(&gIotManagerState.publicState.links[IOT_MANAGER_LINK_BLE],
					   IOT_MANAGER_SERVICE_BLE_LOCAL);
	gIotManagerState.publicState.cloudAnyReady =
		iotManagerPickAutoLink(IOT_MANAGER_SERVICE_MQTT) != IOT_MANAGER_LINK_NONE;
	gIotManagerState.publicState.mqttAuthDone =
		gIotManagerState.publicState.links[IOT_MANAGER_LINK_WIFI].mqttAuthReady ||
		gIotManagerState.publicState.links[IOT_MANAGER_LINK_CELLULAR].mqttAuthReady ||
		gIotManagerState.publicState.links[IOT_MANAGER_LINK_ETHERNET].mqttAuthReady;
}

static bool iotManagerApplyCompatibilitySelectionLocked(eIotManagerLinkId linkId)
{
	stIotManagerServiceRoute *lBleRoute;
	stIotManagerServiceRoute *lMqttAuthRoute;
	stIotManagerServiceRoute *lMqttRoute;
	stIotManagerServiceRoute *lTcpRoute;

	lBleRoute = &gIotManagerState.publicState.bleLocalRoute;
	lMqttAuthRoute = &gIotManagerState.publicState.mqttAuthRoute;
	lMqttRoute = &gIotManagerState.publicState.mqttRoute;
	lTcpRoute = &gIotManagerState.publicState.tcpServerRoute;

	if (linkId == IOT_MANAGER_LINK_NONE) {
		lBleRoute->preferredLink = IOT_MANAGER_LINK_NONE;
		lBleRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
		lMqttAuthRoute->preferredLink = IOT_MANAGER_LINK_NONE;
		lMqttAuthRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
		lMqttRoute->preferredLink = IOT_MANAGER_LINK_NONE;
		lMqttRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
		lTcpRoute->preferredLink = IOT_MANAGER_LINK_NONE;
		lTcpRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
		iotManagerRefreshStateLocked();
		return true;
	}

	if (!iotManagerIsValidLink(linkId)) {
		return false;
	}

	if (linkId == IOT_MANAGER_LINK_BLE) {
		lBleRoute->preferredLink = linkId;
		lBleRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
		iotManagerRefreshStateLocked();
		return true;
	}

	lMqttAuthRoute->preferredLink = linkId;
	lMqttAuthRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
	lMqttRoute->preferredLink = linkId;
	lMqttRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
	if (iotManagerLinkSupportsService(linkId, IOT_MANAGER_SERVICE_TCP_SERVER)) {
		lTcpRoute->preferredLink = linkId;
		lTcpRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
	}
	iotManagerRefreshStateLocked();

	return true;
}

bool iotManagerSendByLink(eIotManagerLinkId linkId, const uint8_t *buffer, uint16_t length)
{
	if (!iotManagerIsValidLink(linkId) || (buffer == NULL) || (length == 0U)) {
		return false;
	}

	switch (linkId) {
	case IOT_MANAGER_LINK_BLE:
		return wirelessSendBleData(buffer, length);
	case IOT_MANAGER_LINK_WIFI:
		return wirelessSendWifiData(buffer, length);
	case IOT_MANAGER_LINK_CELLULAR:
		return cellularSendData(buffer, length);
	case IOT_MANAGER_LINK_ETHERNET:
		return ethernetSendData(buffer, length);
	default:
		break;
	}

	return false;
}

bool iotManagerSend(eIotManagerServiceId serviceId, const uint8_t *buffer, uint16_t length)
{
	stIotManagerServiceRoute *lRoute;
	eIotManagerLinkId lLinkId;

	iotManagerEnsureStateInitialized();
	if ((buffer == NULL) || (length == 0U)) {
		return false;
	}

	repRtosEnterCritical();
	lRoute = iotManagerGetRouteByServiceId(serviceId);
	if (lRoute == NULL) {
		repRtosExitCritical();
		return false;
	}
	iotManagerRefreshStateLocked();
	lLinkId = lRoute->activeLink;
	repRtosExitCritical();

	return iotManagerSendByLink(lLinkId, buffer, length);
}

bool iotManagerSendByInterface(eIotManagerInterface interfaceType, const uint8_t *buffer, uint16_t length)
{
	return iotManagerSendByLink(iotManagerInterfaceToLink(interfaceType), buffer, length);
}

bool iotManagerUpdateLinkState(eIotManagerLinkId linkId, const stIotManagerLinkRuntime *runtime)
{
	stIotManagerLinkRuntime *lRuntime;

	iotManagerEnsureStateInitialized();
	if (!iotManagerIsValidLink(linkId) || (runtime == NULL)) {
		return false;
	}

	repRtosEnterCritical();
	lRuntime = &gIotManagerState.publicState.links[linkId];
	*lRuntime = *runtime;
	lRuntime->linkId = linkId;
	if (iotManagerLinkCapsIsEmpty(&lRuntime->caps)) {
		lRuntime->caps = gIotManagerDefaultLinkCaps[linkId];
	}
	iotManagerRefreshStateLocked();
	repRtosExitCritical();
	return true;
}

bool iotManagerSelectRoute(eIotManagerServiceId serviceId, eIotManagerLinkId linkId)
{
	stIotManagerServiceRoute *lRoute;

	iotManagerEnsureStateInitialized();
	if ((linkId != IOT_MANAGER_LINK_NONE) &&
		(!iotManagerIsValidLink(linkId) || !iotManagerLinkSupportsService(linkId, serviceId))) {
		return false;
	}

	repRtosEnterCritical();
	lRoute = iotManagerGetRouteByServiceId(serviceId);
	if (lRoute == NULL) {
		repRtosExitCritical();
		return false;
	}
	lRoute->preferredLink = linkId;
	lRoute->policy = IOT_MANAGER_SEND_POLICY_FIXED;
	iotManagerRefreshStateLocked();
	repRtosExitCritical();
	return true;
}

bool iotManagerSetActiveInterface(eIotManagerInterface interfaceType)
{
	eIotManagerLinkId lLinkId;

	iotManagerEnsureStateInitialized();
	if ((interfaceType != IOT_MANAGER_INTERFACE_NONE) && !iotManagerIsValidInterface(interfaceType)) {
		return false;
	}

	lLinkId = iotManagerInterfaceToLink(interfaceType);
	repRtosEnterCritical();
	if (!iotManagerApplyCompatibilitySelectionLocked(lLinkId)) {
		repRtosExitCritical();
		return false;
	}
	repRtosExitCritical();
	return true;
}

bool iotManagerSetTargetInterface(eIotManagerInterface interfaceType)
{
	eIotManagerLinkId lLinkId;

	iotManagerEnsureStateInitialized();
	if ((interfaceType != IOT_MANAGER_INTERFACE_NONE) && !iotManagerIsValidInterface(interfaceType)) {
		return false;
	}

	lLinkId = iotManagerInterfaceToLink(interfaceType);
	repRtosEnterCritical();
	if (!iotManagerApplyCompatibilitySelectionLocked(lLinkId)) {
		repRtosExitCritical();
		return false;
	}
	repRtosExitCritical();
	return true;
}

bool iotManagerSetInterfaceReady(eIotManagerInterface interfaceType, bool ready)
{
	stIotManagerLinkRuntime *lRuntime;
	eIotManagerLinkId lLinkId;
	uint32_t lTick;

	iotManagerEnsureStateInitialized();
	if (!iotManagerIsValidInterface(interfaceType)) {
		return false;
	}

	lLinkId = iotManagerInterfaceToLink(interfaceType);
	repRtosEnterCritical();
	lRuntime = &gIotManagerState.publicState.links[lLinkId];
	lTick = repRtosGetTickMs();
	lRuntime->installed = true;
	lRuntime->enabled = ready;
	lRuntime->moduleReady = ready;
	lRuntime->busy = false;
	if (!ready) {
		lRuntime->state = IOT_MANAGER_LINK_STATE_DISABLED;
		lRuntime->netReady = false;
		lRuntime->peerConnected = false;
		lRuntime->mqttAuthReady = false;
		lRuntime->mqttReady = false;
		lRuntime->tcpServerListening = false;
		lRuntime->tcpClientConnected = false;
		lRuntime->lastFailTick = lTick;
	} else {
		lRuntime->state = (lLinkId == IOT_MANAGER_LINK_BLE) ? IOT_MANAGER_LINK_STATE_READY : IOT_MANAGER_LINK_STATE_NET_READY;
		lRuntime->peerConnected = (lLinkId == IOT_MANAGER_LINK_BLE);
		lRuntime->netReady = (lLinkId != IOT_MANAGER_LINK_BLE);
		lRuntime->mqttAuthReady = lRuntime->netReady && lRuntime->caps.supportMqttAuthHttp;
		lRuntime->mqttReady = false;
		lRuntime->tcpServerListening = false;
		lRuntime->tcpClientConnected = false;
		lRuntime->lastOkTick = lTick;
	}

	iotManagerRefreshStateLocked();
	repRtosExitCritical();
	return true;
}

bool iotManagerSetInterfaceStatus(eIotManagerInterface interfaceType, eIotManagerNetStatus status)
{
	stIotManagerLinkRuntime *lRuntime;
	eIotManagerLinkId lLinkId;
	uint32_t lTick;

	iotManagerEnsureStateInitialized();
	if (!iotManagerIsValidInterface(interfaceType) || (status >= IOT_MANAGER_NET_STATUS_MAX)) {
		return false;
	}

	lLinkId = iotManagerInterfaceToLink(interfaceType);
	repRtosEnterCritical();
	lRuntime = &gIotManagerState.publicState.links[lLinkId];
	lTick = repRtosGetTickMs();
	lRuntime->installed = true;
	lRuntime->enabled = true;
	lRuntime->busy = false;
	switch (status) {
	case IOT_MANAGER_NET_STATUS_UNKNOWN:
		lRuntime->state = IOT_MANAGER_LINK_STATE_INITING;
		lRuntime->moduleReady = false;
		lRuntime->netReady = false;
		lRuntime->peerConnected = false;
		lRuntime->mqttAuthReady = false;
		lRuntime->mqttReady = false;
		lRuntime->tcpServerListening = false;
		lRuntime->tcpClientConnected = false;
		break;
	case IOT_MANAGER_NET_STATUS_IDLE:
		lRuntime->state = IOT_MANAGER_LINK_STATE_DISABLED;
		lRuntime->moduleReady = false;
		lRuntime->netReady = false;
		lRuntime->peerConnected = false;
		lRuntime->mqttAuthReady = false;
		lRuntime->mqttReady = false;
		lRuntime->tcpServerListening = false;
		lRuntime->tcpClientConnected = false;
		lRuntime->lastFailTick = lTick;
		break;
	case IOT_MANAGER_NET_STATUS_READY:
		lRuntime->state = (lLinkId == IOT_MANAGER_LINK_BLE) ? IOT_MANAGER_LINK_STATE_READY : IOT_MANAGER_LINK_STATE_NET_READY;
		lRuntime->moduleReady = true;
		lRuntime->netReady = (lLinkId != IOT_MANAGER_LINK_BLE);
		lRuntime->peerConnected = (lLinkId == IOT_MANAGER_LINK_BLE);
		lRuntime->mqttAuthReady = lRuntime->netReady && lRuntime->caps.supportMqttAuthHttp;
		lRuntime->mqttReady = false;
		lRuntime->tcpServerListening = false;
		lRuntime->tcpClientConnected = false;
		lRuntime->lastOkTick = lTick;
		break;
	case IOT_MANAGER_NET_STATUS_SELECTED:
		lRuntime->state = IOT_MANAGER_LINK_STATE_SERVICE_CONNECTING;
		lRuntime->moduleReady = true;
		lRuntime->netReady = (lLinkId != IOT_MANAGER_LINK_BLE);
		lRuntime->peerConnected = (lLinkId == IOT_MANAGER_LINK_BLE);
		lRuntime->mqttAuthReady = lRuntime->netReady && lRuntime->caps.supportMqttAuthHttp;
		lRuntime->lastOkTick = lTick;
		break;
	case IOT_MANAGER_NET_STATUS_ACTIVE:
		lRuntime->state = IOT_MANAGER_LINK_STATE_SERVICE_READY;
		lRuntime->moduleReady = true;
		lRuntime->netReady = (lLinkId != IOT_MANAGER_LINK_BLE);
		lRuntime->peerConnected = (lLinkId == IOT_MANAGER_LINK_BLE);
		lRuntime->mqttAuthReady = lRuntime->netReady && lRuntime->caps.supportMqttAuthHttp;
		lRuntime->mqttReady = lRuntime->netReady && lRuntime->caps.supportMqtt;
		lRuntime->tcpServerListening = lRuntime->caps.supportTcpServer;
		lRuntime->lastOkTick = lTick;
		break;
	case IOT_MANAGER_NET_STATUS_ERROR:
		lRuntime->state = IOT_MANAGER_LINK_STATE_ERROR;
		lRuntime->mqttReady = false;
		lRuntime->tcpServerListening = false;
		lRuntime->tcpClientConnected = false;
		lRuntime->lastFailTick = lTick;
		break;
	default:
		break;
	}
	iotManagerRefreshStateLocked();
	repRtosExitCritical();
	return true;
}

const stIotManagerState *iotManagerGetState(void)
{
	iotManagerEnsureStateInitialized();
	return &gIotManagerState.publicState;
}

void iotManagerProcess(void)
{
	protcolMgrProcess();
}

/**************************End of file********************************/
