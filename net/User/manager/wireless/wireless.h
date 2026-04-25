/************************************************************************************
* @file     : wireless.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef NETWORK_APP_MANAGER_WIRELESS_WIRELESS_H
#define NETWORK_APP_MANAGER_WIRELESS_WIRELESS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIRELESS_WIFI_SSID_MAX_LEN        32U
#define WIRELESS_WIFI_PASSWORD_MAX_LEN    64U
#define WIRELESS_IOT_SN_MAX_LEN           32U
#define WIRELESS_IOT_KEY_MAX_LEN          96U
#define WIRELESS_IOT_URL_MAX_LEN          160U
#define WIRELESS_IOT_HOST_MAX_LEN         96U
#define WIRELESS_IOT_USER_MAX_LEN         64U
#define WIRELESS_IOT_TOPIC_MAX_LEN        96U
#define WIRELESS_IOT_CMD_MAX_LEN          384U
#define WIRELESS_IOT_HTTP_PAYLOAD_LEN     192U
#define WIRELESS_IOT_MD5_HEX_LEN          33U
#define WIRELESS_IOT_HTTP_RESPONSE_LEN    512U

#define WIRELESS_WIFI_SSID_PATH           "/mem/setting/wifiname"
#define WIRELESS_IOT_RETRY_MS              5000U
#define WIRELESS_WIFI_PASSWORD_PATH       "/mem/setting/wifipassword"
#define WIRELESS_IOT_SN_PATH              "/mem/dev/serial"
#define WIRELESS_IOT_SN_LEGACY_PATH       "/mem/dev/sn"
#define WIRELESS_IOT_HTTP_URL_PATH        "/mem/setting/iot_http_url"
#define WIRELESS_IOT_MQTT_KEY_PATH        "/mem/setting/mqttkey"
#define WIRELESS_IOT_MQTT_HOST_PATH       "/mem/setting/mqtt_host"
#define WIRELESS_IOT_MQTT_PORT_PATH       "/mem/setting/mqtt_port"
#define WIRELESS_IOT_MQTT_USER_PATH       "/mem/setting/mqtt_user"
#define WIRELESS_IOT_MQTT_CLIENT_ID_PATH  "/mem/setting/mqtt_clientid"
#define WIRELESS_IOT_MQTT_TOPIC_PATH      "/mem/setting/mqtt_topic"
#define WIRELESS_IOT_MQTT_SUB_TOPIC_PATH  "/mem/setting/mqtt_sub_topic"
#define WIRELESS_IOT_HTTP_URL_BACKUP_PATH "/mem/setting/iot_http_url.bak"
#define WIRELESS_IOT_MQTT_HOST_BACKUP_PATH "/mem/setting/mqtt_host.bak"
#define WIRELESS_IOT_MQTT_PORT_BACKUP_PATH "/mem/setting/mqtt_port.bak"
#define WIRELESS_IOT_MQTT_KEY_BACKUP_PATH "/mem/setting/mqttkey.bak"

#define WIRELESS_IOT_DEFAULT_HTTP_URL     "http://iot-test2.yuwell.com:8800/device/secret-key"
#define WIRELESS_IOT_DEFAULT_MQTT_HOST    "iot-test2.yuwell.com"
#define WIRELESS_IOT_DEFAULT_MQTT_PORT    "1883"
#define WIRELESS_IOT_PRODUCT_SECRET       "YuWell@CPR"
#define WIRELESS_IOT_HTTP_RANDOM          "1234567812345678"
#define WIRELESS_IOT_LOCAL_TEST_MODE      1U
#define WIRELESS_IOT_LOCAL_HTTP_URL       "http://192.168.8.8:8800/device/secret-key"
#define WIRELESS_IOT_LOCAL_MQTT_HOST      "192.168.8.8"
#define WIRELESS_IOT_LOCAL_MQTT_PORT      "1883"

typedef enum wirlessMode {
    WIRELESS_MODE_NONE = 0,
    WIRELESS_MODE_WIFI,
    WIRELESS_MODE_BLE,
} eWirelessMode;

typedef enum bleState {
    BLE_STATE_IDLE = 0,
    BLE_STATE_INITIALIZING,
    BLE_STATE_READY,
    BLE_WAITING_CONNECTION,
    BLE_STATE_DISCONNECTED,
    BLE_STATE_CONNECTED,
    BLE_STATE_ERROR,
    BLE_STATE_MAX,
} eBleState;

typedef enum wifiState {
    WIFI_STATE_IDLE = 0,
    WIFI_STATE_INITIALIZING,
    WIFI_STATE_READY,
    WIFI_STATE_SCANNING,
    WIFI_STATE_WAITING_CONNECTION,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_ERROR,
    WIFI_STATE_MAX,
} eWifiState;

typedef enum eWirelessIotState {
    WIRELESS_IOT_STATE_IDLE = 0,
    WIRELESS_IOT_STATE_WAIT_WIFI,
    WIRELESS_IOT_STATE_WAIT_AUTH,
    WIRELESS_IOT_STATE_AUTH_READY,
    WIRELESS_IOT_STATE_MQTT_CONNECTING,
    WIRELESS_IOT_STATE_MQTT_READY,
    WIRELESS_IOT_STATE_ERROR,
    WIRELESS_IOT_STATE_MAX,
} eWirelessIotState;

enum bleState wirelessGetBleState(void);
enum wifiState wirelessGetWifiState(void);
eWirelessIotState wirelessGetIotState(void);
bool wirelessIsReady(void);
bool wirelessInit(void);
bool wirelessGetBleEnabled(void);
bool wirelessSetBleEnabled(bool enabled);
bool wirelessGetWifiEnabled(void);
bool wirelessSetWifiEnabled(bool enabled);
bool wirelessConnectWifi(const uint8_t *ssid, uint8_t ssidLen, const uint8_t *password, uint8_t passwordLen);
bool wirelessGetMqttEnabled(void);
bool wirelessSetMqttEnabled(bool enabled);
bool wirelessSendData(const uint8_t *buffer, uint16_t length);
bool wirelessSendBleData(const uint8_t *buffer, uint16_t length);
bool wirelessSendWifiData(const uint8_t *buffer, uint16_t length);
bool wirelessSetWifiCredentials(const uint8_t *ssid, uint8_t ssidLen, const uint8_t *password, uint8_t passwordLen);
bool wirelessGetMacAddress(char *buffer, uint16_t bufferSize);
bool wirelessRequestIotRetry(void);

void wirelessProcess(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
