---
doc_role: manager-spec
layer: manager
module: wireless
status: active
portability: project-bound
public_headers:
  - wireless.h
  - wireless_ble.h
  - wireless_wifi.h
core_files:
  - wireless.c
  - wireless_ble.h
  - wireless_wifi.h
  - wireless_debug.c
  - wireless_debug.h
port_files: []
debug_files: []
depends_on:
  - ../iotmanager/iotmanager.md
  - ../../../rep/module/esp32c5/esp32c5.md
forbidden_depends_on:
  - 直接调用原生 RTOS API
required_hooks: []
optional_hooks: []
common_utils:
  - ../../../rep/module/esp32c5
copy_minimal_set:
  - wireless/
read_next:
  - wireless.h
  - wireless_ble.h
  - wireless_wifi.h
---

# Wireless 管理器说明

这是当前目录的权威入口文档。

## 1. 概要设计

`wireless` 是当前项目绑定的 ESP32-C5 编排层，负责把同一个 ESP-AT 模块拆成两条业务链路：

| 链路 | 入口头文件 | 主要职责 |
| --- | --- | --- |
| BLE | `wireless_ble.h` | BLE 广播、连接状态、CPR 协议握手、BLE notify 发送 |
| WiFi | `wireless_wifi.h` | WiFi 入网、HTTP 获取 MQTT key、MQTT 连接/订阅/发布 |
| 兼容入口 | `wireless.h` | 保留旧调用方 API，内部转发到 BLE 或 WiFi 能力 |

`wireless.c` 保留项目状态机和共享状态；ESP-AT 命令组包已经下沉到 `rep/module/esp32c5/esp32c5_ble.c`、`esp32c5_wifi.c`、`esp32c5_http.c`、`esp32c5_mqtt.c`。

## 2. 接口契约

| API | 链路 | 调用要求 | 行为 |
| --- | --- | --- | --- |
| `wirelessInit` / `wirelessProcess` | 公共 | 周期调用 | 驱动 ESP32-C5、BLE、WiFi、HTTP、MQTT 状态机 |
| `wirelessSendBleData` | BLE | BLE 协议握手后调用 | 写入 BLE notify 待发送缓存 |
| `wirelessSendWifiData` | WiFi/MQTT | MQTT ready 后调用 | 使用 `AT+MQTTPUBRAW` 发送二进制 CPR 协议帧 |
| `wirelessSetWifiCredentials` | WiFi | SSID 长度不超过 `WIRELESS_WIFI_SSID_MAX_LEN` | 更新入网参数并重新进入 WiFi join 流程 |
| `wirelessSetBleEnabled` | BLE | RTT 或管理层调用 | 打开时开始广播，关闭时断开连接并停止广播 |
| `wirelessSetWifiEnabled` | WiFi | RTT 或管理层调用 | 打开时进入等待连接指令状态，关闭时先停 MQTT 再断开 WiFi |
| `wirelessSetMqttEnabled` | MQTT | WiFi connected 后打开 | 打开时主动连接 MQTT，关闭时主动清理 MQTT 连接 |
| `wirelessGetMacAddress` | BLE | ESP32-C5 ready 后调用 | 返回缓存 BLE MAC，用于 CPR 握手密钥 |

## 3. 状态机

| 状态机 | 输入 | 输出 |
| --- | --- | --- |
| BLE | `esp32c5` ready、`+BLECONN`、`+BLEDISCONN`、`+WRITE` | BLE link runtime、协议帧转发、notify 发送 |
| WiFi | 存储 SSID/password、`WIFI GOT IP`、断线 URC | WiFi link runtime、HTTP/MQTT 允许条件 |
| HTTP | WiFi ready、缺少 MQTT key | `AT+HTTPCPOST` prompt 请求、缓存 key |
| MQTT | key ready、broker URC、订阅 URC | MQTT ready、订阅主机下发 topic、发布上行 topic |

## 4. 测试入口

| 脚本 | 用途 |
| --- | --- |
| `test/esp32c5_ble_tester.py` | BLE GATT 基础收发和 MTU 检查 |
| `test/local_iot_workflow.py` | 本地 HTTP + MQTT + RTT 工作流 |
| `test/concurrent_ble_mqtt_protocol_test.py` | BLE 与 MQTT 同时收发 CPR 协议帧、连续多包丢包检查 |
| `test/wireless_switch_rtt_test.py` | RTT 执行 BLE/WiFi/MQTT 开关独立性组合测试 |
| `test/wireless_switch_heartbeat_stress_test.py` | BLE/MQTT 心跳持续发送时交叉执行 BLE、MQTT、WiFi 开关并验证重开后心跳恢复 |

RTT 调试命令：`wireless status`、`wireless ble_on`、`wireless ble_off`、`wireless wifi_on`、`wireless wifi_off`、`wireless wifi_connect <SSID> <PASSWORD>`、`wireless mqtt_on`、`wireless mqtt_off`。

## 5. 使用示例

1. 固件周期调用 `wirelessProcess()`。
2. 手机或测试脚本通过 BLE 完成 CPR handshake 后发送 BLE 协议帧。
3. 本地 IoT 测试脚本等待 WiFi/MQTT ready 后，向固件订阅的 MQTT command topic 发布 CPR 协议帧。
4. 固件通过 BLE notify 和 MQTT raw publish 分别返回协议响应。
