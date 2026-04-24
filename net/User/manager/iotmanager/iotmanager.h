/************************************************************************************
* @file     : iotmanager.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_MANAGER_IOTMANAGER_IOTMANAGER_H
#define NETWORK_APP_MANAGER_IOTMANAGER_IOTMANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eIotManagerInterface {
	IOT_MANAGER_INTERFACE_NONE = 0,
	IOT_MANAGER_INTERFACE_WIRELESS = 1,
	IOT_MANAGER_INTERFACE_CELLULAR = 3,
	IOT_MANAGER_INTERFACE_ETHERNET = 4,
	IOT_MANAGER_INTERFACE_MAX = 5,
} eIotManagerInterface;

typedef enum eIotManagerLinkId {
	IOT_MANAGER_LINK_NONE = 0,
	IOT_MANAGER_LINK_BLE,
	IOT_MANAGER_LINK_WIFI,
	IOT_MANAGER_LINK_CELLULAR,
	IOT_MANAGER_LINK_ETHERNET,
	IOT_MANAGER_LINK_MAX,
} eIotManagerLinkId;

typedef enum eIotManagerCellularType {
	IOT_MANAGER_CELLULAR_NONE = 0,
	IOT_MANAGER_CELLULAR_4G,
	IOT_MANAGER_CELLULAR_5G,
	IOT_MANAGER_CELLULAR_MAX,
} eIotManagerCellularType;

typedef enum eIotManagerServiceId {
	IOT_MANAGER_SERVICE_NONE = 0,
	IOT_MANAGER_SERVICE_BLE_LOCAL,
	IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP,
	IOT_MANAGER_SERVICE_MQTT,
	IOT_MANAGER_SERVICE_TCP_SERVER,
	IOT_MANAGER_SERVICE_MAX,
} eIotManagerServiceId;

typedef enum eIotManagerLinkState {
	IOT_MANAGER_LINK_STATE_ABSENT = 0,
	IOT_MANAGER_LINK_STATE_DISABLED,
	IOT_MANAGER_LINK_STATE_INITING,
	IOT_MANAGER_LINK_STATE_READY,
	IOT_MANAGER_LINK_STATE_NET_CONNECTING,
	IOT_MANAGER_LINK_STATE_NET_READY,
	IOT_MANAGER_LINK_STATE_SERVICE_CONNECTING,
	IOT_MANAGER_LINK_STATE_SERVICE_READY,
	IOT_MANAGER_LINK_STATE_DEGRADED,
	IOT_MANAGER_LINK_STATE_ERROR,
	IOT_MANAGER_LINK_STATE_MAX,
} eIotManagerLinkState;

typedef enum eIotManagerServiceState {
	IOT_MANAGER_SERVICE_STATE_IDLE = 0,
	IOT_MANAGER_SERVICE_STATE_WAIT_LINK,
	IOT_MANAGER_SERVICE_STATE_CONNECTING,
	IOT_MANAGER_SERVICE_STATE_READY,
	IOT_MANAGER_SERVICE_STATE_BACKOFF,
	IOT_MANAGER_SERVICE_STATE_ERROR,
	IOT_MANAGER_SERVICE_STATE_MAX,
} eIotManagerServiceState;

typedef enum eIotManagerSendPolicy {
	IOT_MANAGER_SEND_POLICY_AUTO = 0,
	IOT_MANAGER_SEND_POLICY_FIXED,
	IOT_MANAGER_SEND_POLICY_FORCE_LINK,
	IOT_MANAGER_SEND_POLICY_MAX,
} eIotManagerSendPolicy;

typedef enum eIotManagerNetStatus {
	IOT_MANAGER_NET_STATUS_UNKNOWN = 0,
	IOT_MANAGER_NET_STATUS_IDLE,
	IOT_MANAGER_NET_STATUS_READY,
	IOT_MANAGER_NET_STATUS_SELECTED,
	IOT_MANAGER_NET_STATUS_ACTIVE,
	IOT_MANAGER_NET_STATUS_ERROR,
	IOT_MANAGER_NET_STATUS_MAX,
} eIotManagerNetStatus;

typedef struct stIotManagerLinkStatus {
	bool ready;
	bool selected;
	bool active;
	bool busy;
	eIotManagerNetStatus status;
	uint32_t updateTickMs;
} stIotManagerLinkStatus;

typedef struct stIotManagerLinkCaps {
	bool supportBleLocal;
	bool supportMqttAuthHttp;
	bool supportMqtt;
	bool supportTcpServer;
} stIotManagerLinkCaps;

typedef struct stIotManagerLinkRuntime {
	eIotManagerLinkId linkId;
	eIotManagerCellularType cellularType;
	eIotManagerLinkState state;
	stIotManagerLinkCaps caps;
	bool installed;
	bool enabled;
	bool selected;
	bool busy;
	bool moduleReady;
	bool netReady;
	bool peerConnected;
	bool mqttAuthReady;
	bool mqttReady;
	bool tcpServerListening;
	bool tcpClientConnected;
	uint8_t retryCount;
	int16_t signalStrength;
	uint32_t lastOkTick;
	uint32_t lastFailTick;
} stIotManagerLinkRuntime;

typedef struct stIotManagerServiceRoute {
	eIotManagerServiceId serviceId;
	eIotManagerLinkId activeLink;
	eIotManagerLinkId preferredLink;
	eIotManagerServiceState state;
	eIotManagerSendPolicy policy;
} stIotManagerServiceRoute;

typedef struct stIotManagerState {
	stIotManagerLinkRuntime links[IOT_MANAGER_LINK_MAX];
	stIotManagerServiceRoute bleLocalRoute;
	stIotManagerServiceRoute mqttAuthRoute;
	stIotManagerServiceRoute mqttRoute;
	stIotManagerServiceRoute tcpServerRoute;
	eIotManagerCellularType installedCellularType;
	bool cloudAnyReady;
	bool localBleReady;
	bool mqttAuthDone;
} stIotManagerState;

bool iotManagerSend(eIotManagerServiceId serviceId, const uint8_t *buffer, uint16_t length);
bool iotManagerSendByLink(eIotManagerLinkId linkId, const uint8_t *buffer, uint16_t length);
bool iotManagerUpdateLinkState(eIotManagerLinkId linkId, const stIotManagerLinkRuntime *runtime);
bool iotManagerSelectRoute(eIotManagerServiceId serviceId, eIotManagerLinkId linkId);
bool iotManagerPushReceivedData(eIotManagerInterface interfaceType, const uint8_t *buffer, uint16_t length);
bool iotManagerSendByInterface(eIotManagerInterface interfaceType, const uint8_t *buffer, uint16_t length);
bool iotManagerSetActiveInterface(eIotManagerInterface interfaceType);
bool iotManagerSetTargetInterface(eIotManagerInterface interfaceType);
bool iotManagerSetInterfaceReady(eIotManagerInterface interfaceType, bool ready);
bool iotManagerSetInterfaceStatus(eIotManagerInterface interfaceType, eIotManagerNetStatus status);
const stIotManagerState *iotManagerGetState(void);
void iotManagerProcess(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
