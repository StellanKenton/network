---
doc_role: user-manager-doc
layer: user
module: iotmanager
status: draft
portability: project-bound
public_headers:
    - iotmanager.h
core_files:
    - iotmanager.c
    - protcolmgr.c
    - cprsensor_protocol.c
port_files: []
debug_files:
    - iotmanager_debug.c
depends_on:
    - ../wireless/wireless.h
    - ../cellular/cellular.h
    - ../ethernet/ethernet.h
forbidden_depends_on:
    - 把 BLE 本地链路和云链路混成同一个发送选择维度
    - 把 4G 和 5G 直接建成两个可并存的主链路枚举
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
    - iotmanager.h
    - iotmanager.c
    - protcolmgr.h
    - protcolmgr.c
    - cprsensor_protocol.h
    - cprsensor_protocol.c
read_next:
    - cprsensor_protocol.md
---

# iotmanager 规划

这是当前目录中 `iotmanager` 的权威规划文档。

本文只解决 `iotmanager.h` 应该如何规划的问题：

- 怎么区分 WiFi、BLE、蜂窝、网口这几类链路。
- 4G 和 5G 只有一种会装机时，枚举和结构体应该怎么设计。
- HTTP、MQTT、TCP Server、BLE 本地链路的发送选择应该怎么做。
- 各自状态应该怎么体现，避免继续用一个过于粗的 `ReadyStatus[]`。

`cprsensor_protocol.md` 只描述 BLE/协议帧，不负责链路路由和业务切换。

当前实现中，链路状态、业务路由和设备管理保留在 `iotmanager.c`，协议收包、解包、应答和协议轮询下沉到 `protcolmgr.c`。

## 调试补充

- `iotmanager_debug.c` 提供可裁剪的调试入口，当前先承载“选择网络”和“查询当前网络状态”。
- 现阶段建议先通过 console 命令做人工切换与观察，后续如果补 WiFi 连接、凭据下发或网络恢复命令，继续收敛在该文件内，不要把调试解析逻辑塞回 `iotmanager.c`。

## 1. 设计结论

先给结论，避免后面看完还是回到旧结构：

1. `iotmanager` 里不要把 `4G` 和 `5G` 直接设计成两个并列主接口。
2. `iotmanager` 的主链路维度应该是 `BLE / WIFI / CELLULAR / ETHERNET`。
3. `4G` 和 `5G` 应该作为 `CELLULAR` 的子类型存在，即“蜂窝模块类型”，而不是“路由层主接口”。
4. 发送时不要只维护一个 `ActiveInterface`，而是至少拆成：`BLE 本地链路`、`MQTT 路由`、`TCP Server 路由`。
5. 状态不要只保留一个 `ready`，而要分成：`安装存在`、`模块初始化`、`网络就绪`、`业务服务就绪`、`当前是否被选中`。
6. `HTTP` 在当前产品里不是长期业务通道，而是 `MQTT` 启动前用于获取登录密钥、证书或 token 的一次性引导服务。

原因很直接：

- 4G 和 5G 不会同时安装，因此把它们做成两个同级接口，只会让数组、状态表、切换逻辑都多出一半空槽。
- `MQTT` 是长期在线业务，`HTTP` 只是启动期或刷新期的凭据获取动作，二者的生命周期不能混在一起。
- BLE 只连手机/电脑，不参与云平台，因此 BLE 不应该和 HTTP/MQTT 共用一套“云发送通道”选择逻辑。
- WiFi 和网口除了云平台链路外，还要支持 `TCP Server` 被其他设备主动连接，因此链路能力里必须单独表达 `TCP Server`。

## 2. 推荐抽象分层

建议在 `iotmanager.h` 中把概念拆成 4 层。

### 2.1 物理/逻辑链路层

这一层回答“现在系统里有哪些可用承载”。

- `BLE`
- `WIFI`
- `CELLULAR`
- `ETHERNET`

这里的 `CELLULAR` 表示“蜂窝链路入口”，不区分 4G/5G。

### 2.2 蜂窝模块类型层

这一层只回答“当前装的是哪种蜂窝模块”。

- `NONE`
- `4G`
- `5G`

它属于 `CELLULAR` 的硬件属性，不属于业务路由主键。

### 2.3 业务服务层

这一层回答“发送的到底是什么业务”。

- `BLE_LOCAL`：本地手机/电脑配置与交互
- `MQTT`
- `MQTT_AUTH_HTTP`：用于获取 MQTT 登录密钥的 HTTP 引导流程
- `TCP_SERVER`

后续如果还有 OTA、NTP、WebSocket，也应该加在这一层，而不是继续往链路枚举里塞。

### 2.4 路由层

这一层回答“某个业务当前绑定到哪条链路”。

至少要单独维护：

- `bleRoute`
- `mqttRoute`
- `mqttAuthRoute`
- `tcpServerRoute`

这样 `MQTT` 在线状态、`MQTT_AUTH_HTTP` 引导状态、`TCP Server` 监听状态就不会互相覆盖。

## 3. 枚举建议

### 3.1 链路标识枚举

```c
typedef enum eIotManagerLinkId {
    IOT_MANAGER_LINK_NONE = 0,
    IOT_MANAGER_LINK_BLE,
    IOT_MANAGER_LINK_WIFI,
    IOT_MANAGER_LINK_CELLULAR,
    IOT_MANAGER_LINK_ETHERNET,
    IOT_MANAGER_LINK_MAX,
} eIotManagerLinkId;
```

这比当前的 `WIRELESS/CELLULAR/ETHERNET` 更清楚，因为 `wirelessmgr` 里同时有 WiFi 和 BLE，而这两个在业务角色上根本不是同一类东西。

### 3.2 蜂窝模块类型枚举

```c
typedef enum eIotManagerCellularType {
    IOT_MANAGER_CELLULAR_NONE = 0,
    IOT_MANAGER_CELLULAR_4G,
    IOT_MANAGER_CELLULAR_5G,
    IOT_MANAGER_CELLULAR_MAX,
} eIotManagerCellularType;
```

这个枚举的职责只有一个：标识当前 `CELLULAR` 对应的是 4G 还是 5G。

### 3.3 业务服务枚举

```c
typedef enum eIotManagerServiceId {
    IOT_MANAGER_SERVICE_NONE = 0,
    IOT_MANAGER_SERVICE_BLE_LOCAL,
    IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP,
    IOT_MANAGER_SERVICE_MQTT,
    IOT_MANAGER_SERVICE_TCP_SERVER,
    IOT_MANAGER_SERVICE_MAX,
} eIotManagerServiceId;
```

之后凡是“要把数据发出去”，优先传这个枚举，而不是先传接口。

### 3.4 链路运行状态枚举

```c
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
```

这不是给单个驱动层内部状态机完全复用的，而是给 `iotmanager` 用来描述“站在上层看，这条链路当前能不能承载业务”。

### 3.5 服务状态枚举

```c
typedef enum eIotManagerServiceState {
    IOT_MANAGER_SERVICE_STATE_IDLE = 0,
    IOT_MANAGER_SERVICE_STATE_WAIT_LINK,
    IOT_MANAGER_SERVICE_STATE_CONNECTING,
    IOT_MANAGER_SERVICE_STATE_READY,
    IOT_MANAGER_SERVICE_STATE_BACKOFF,
    IOT_MANAGER_SERVICE_STATE_ERROR,
    IOT_MANAGER_SERVICE_STATE_MAX,
} eIotManagerServiceState;
```

`MQTT_AUTH_HTTP`、`MQTT`、`TCP_SERVER` 建议各自保留一份服务状态，不要共用一个“云状态”。

### 3.6 发送策略枚举

```c
typedef enum eIotManagerSendPolicy {
    IOT_MANAGER_SEND_POLICY_AUTO = 0,
    IOT_MANAGER_SEND_POLICY_FIXED,
    IOT_MANAGER_SEND_POLICY_FORCE_LINK,
    IOT_MANAGER_SEND_POLICY_MAX,
} eIotManagerSendPolicy;
```

含义建议如下：

- `AUTO`：按当前路由策略自动选链路。
- `FIXED`：业务固定绑定到一个链路，除非显式切换。
- `FORCE_LINK`：本次发送强制指定链路，只用于特殊维护或调试。

## 4. 结构体建议

### 4.1 链路能力结构体

```c
typedef struct stIotManagerLinkCaps {
    bool supportBleLocal;
    bool supportMqttAuthHttp;
    bool supportMqtt;
    bool supportTcpServer;
} stIotManagerLinkCaps;
```

推荐默认能力表：

- `BLE`: `supportBleLocal = true`
- `WIFI`: `supportMqttAuthHttp = true`, `supportMqtt = true`, `supportTcpServer = true`
- `CELLULAR`: `supportMqttAuthHttp = true`, `supportMqtt = true`
- `ETHERNET`: `supportMqttAuthHttp = true`, `supportMqtt = true`, `supportTcpServer = true`

### 4.2 单链路运行状态结构体

```c
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
```

字段说明：

- `installed`：板子上是否装了该链路对应的硬件。
- `enabled`：软件配置是否允许启用。
- `selected`：当前是否被选为某个业务的活动路由。
- `moduleReady`：模块或驱动初始化是否完成。
- `netReady`：是否已经具备上网能力。对 BLE 来说通常恒为 `false`。
- `peerConnected`：BLE 是否连上手机/电脑，或上层需要表达“点对点连接已建立”的场景。
- `mqttAuthReady`：当前链路是否已经具备发起 HTTP 鉴权请求的条件。它表示“可拉取 MQTT 凭据”，不是表示系统要长期跑 HTTP。
- `mqttReady`：MQTT 业务是否已经在线。
- `tcpServerListening` / `tcpClientConnected`：仅对 WiFi 和 Ethernet 有意义，用于表达 TCP Server 的监听和会话状态。

重点是：不要再用 `ReadyStatus[IOT_MANAGER_INTERFACE_MAX]` 这种没有语义的数组。后续你自己再读代码时，根本看不出这个 ready 到底是“模块 ready”“网络 ready”还是“平台 ready”。

### 4.3 服务路由结构体

```c
typedef struct stIotManagerServiceRoute {
    eIotManagerServiceId serviceId;
    eIotManagerLinkId activeLink;
    eIotManagerLinkId preferredLink;
    eIotManagerServiceState state;
    eIotManagerSendPolicy policy;
} stIotManagerServiceRoute;
```

建议固定维护三份：

- `bleLocalRoute`
- `mqttRoute`
- `mqttAuthRoute`
- `tcpServerRoute`

### 4.4 总状态结构体

```c
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
```

这里的：

- `cloudAnyReady` 表示当前是否至少存在一条可承载 `MQTT` 的云通路。
- `mqttAuthDone` 表示 MQTT 所需凭据是否已经通过 HTTP 引导流程拿到并缓存。

它们都只是便捷聚合状态，不能替代每条链路和每个服务自己的状态。

## 5. 发送接口建议

当前的：

```c
bool iotManagerSendByInterface(eIotManagerInterface interfaceType, const uint8_t *buffer, uint16_t length);
```

只适合底层调试，不适合作为长期主入口。建议保留一个“强制按链路发”的低层接口，再新增一个“按业务发”的主入口。

### 5.1 推荐主入口

```c
bool iotManagerSend(eIotManagerServiceId serviceId, const uint8_t *buffer, uint16_t length);
```

行为建议：

- `BLE_LOCAL`：只允许走 `BLE`。
- `MQTT_AUTH_HTTP`：只允许走 `WIFI / CELLULAR / ETHERNET`。
- `MQTT`：只允许走 `WIFI / CELLULAR / ETHERNET`。
- `TCP_SERVER`：只允许走 `WIFI / ETHERNET`。

如果对应服务当前没有活动链路，函数内部按路由策略自动选链路，而不是让调用方自己猜该发到哪个 manager。

### 5.2 保留低层强制接口

```c
bool iotManagerSendByLink(eIotManagerLinkId linkId, const uint8_t *buffer, uint16_t length);
```

这个接口只做两件事：

- 用于 `iotmanager` 内部最终派发。
- 用于调试或特殊维护场景。

业务代码不应默认直接调用它。

## 6. 链路选择规则建议

## 6.1 BLE 本地链路

- 仅服务于手机/电脑连接。
- 不参与云平台 HTTP/MQTT 选择。
- 只有 `wirelessmgr` 的 BLE 状态满足“广播/连接/可收发”时，`BLE_LOCAL` 才能进入 `READY`。

## 6.2 MQTT_AUTH_HTTP 和 MQTT 的候选链路

候选范围固定为：

- `WIFI`
- `CELLULAR`
- `ETHERNET`

`BLE` 永远不加入候选集合。

说明：

- `MQTT_AUTH_HTTP` 只负责获取 MQTT 登录凭据。
- `MQTT` 才是与 IoT 平台长期保持会话和传输数据的主业务。

## 6.3 TCP Server 的候选链路

候选范围固定为：

- `WIFI`
- `ETHERNET`

`CELLULAR` 默认不加入 `TCP Server` 候选，除非后续产品明确要求蜂窝链路也提供监听模式。

状态判断建议：

- `tcpServerListening = true`：已进入监听状态，可以等待其他设备接入。
- `tcpClientConnected = true`：已有客户端接入，可以进行会话收发。
- 当 `tcpClientConnected = false` 且 `tcpServerListening = true` 时，仍应认为服务可用，只是当前没有活动会话。

## 6.4 4G/5G 的处理方式

推荐规则：

- 路由层只看到 `CELLULAR`。
- `CELLULAR` 的底层实现再根据 `installedCellularType` 走 4G 或 5G 驱动流程。
- 日志、状态上报、诊断界面里通过 `cellularType` 展示“当前是 4G 还是 5G”。

这样做的收益是：

- 路由切换逻辑不需要为“4G/5G 二选一装机”写双份分支。
- 如果以后从 4G 模块切到 5G 模块，业务层头文件和状态表基本不用改。

## 6.5 建议的自动选择优先级

如果你暂时没有产品定义上的特殊要求，建议默认优先级如下：

1. `ETHERNET`
2. `WIFI`
3. `CELLULAR`

原因：

- 网口最稳定，适合固定设备。
- WiFi 通常成本低、吞吐稳定。
- 蜂窝适合作为兜底或移动场景。

如果你们产品逻辑更偏向“优先蜂窝，WiFi 只做配置”，那就把优先级表做成可配置字段，不要硬编码进单个 `if-else`。

## 7. 状态体现建议

建议把状态分成四类，不要混着记。

### 7.1 链路存在/使能状态

用于回答“这个通道理论上能不能用”。

- 是否安装
- 是否允许启用
- 是否初始化完成

### 7.2 网络/连接状态

用于回答“这条通路物理上是否已经通”。

- WiFi 是否联网
- 以太网是否拿到链路/IP
- 蜂窝是否注册网络、数据拨号是否成功
- BLE 是否已连接手机/电脑

### 7.3 服务状态

用于回答“上层业务能不能发”。

- MQTT 凭据 HTTP 引导是否 ready
- MQTT 是否 online
- BLE_LOCAL 是否处于可交互状态
- TCP Server 是否已监听、是否已有客户端接入

### 7.4 启动阶段状态

用于回答“MQTT 能不能开始连接”。

- HTTP 引导是否已经完成
- MQTT 登录密钥、证书、token 是否已缓存
- 凭据是否过期，需要重新获取

这三类状态必须能同时存在。比如：

- `CELLULAR` 可能 `moduleReady = true`，但 `netReady = false`。
- `WIFI` 可能 `netReady = true`，但 `mqttReady = false`。
- `BLE` 可能 `moduleReady = true`，但 `peerConnected = false`。

同时还可能出现：

- `ETHERNET` 已经 `netReady = true`，`tcpServerListening = true`，但 `mqttAuthDone = false`。
- `WIFI` 已经 `mqttAuthReady = true`，但 `mqttReady = false`，说明凭据已经拿到但 MQTT 还没连上。

## 8. 头文件组织建议

建议把 `iotmanager.h` 重新整理成下面顺序：

1. 基础枚举：`linkId / cellularType / serviceId / linkState / serviceState / sendPolicy`
2. 能力结构体：`stIotManagerLinkCaps`
3. 运行状态结构体：`stIotManagerLinkRuntime`
4. 路由结构体：`stIotManagerServiceRoute`
5. 总状态结构体：`stIotManagerState`
6. 查询/上报/发送接口声明

建议骨架如下：

```c
typedef enum eIotManagerLinkId { ... } eIotManagerLinkId;
typedef enum eIotManagerCellularType { ... } eIotManagerCellularType;
typedef enum eIotManagerServiceId { ... } eIotManagerServiceId;
typedef enum eIotManagerLinkState { ... } eIotManagerLinkState;
typedef enum eIotManagerServiceState { ... } eIotManagerServiceState;
typedef enum eIotManagerSendPolicy { ... } eIotManagerSendPolicy;

typedef struct stIotManagerLinkCaps { ... } stIotManagerLinkCaps;
typedef struct stIotManagerLinkRuntime { ... } stIotManagerLinkRuntime;
typedef struct stIotManagerServiceRoute { ... } stIotManagerServiceRoute;
typedef struct stIotManagerState { ... } stIotManagerState;

bool iotManagerSend(eIotManagerServiceId serviceId, const uint8_t *buffer, uint16_t length);
bool iotManagerSendByLink(eIotManagerLinkId linkId, const uint8_t *buffer, uint16_t length);
bool iotManagerUpdateLinkState(eIotManagerLinkId linkId, const stIotManagerLinkRuntime *runtime);
bool iotManagerSelectRoute(eIotManagerServiceId serviceId, eIotManagerLinkId linkId);
const stIotManagerState *iotManagerGetState(void);
void iotManagerProcess(void);
```

其中：

- `iotManagerSend()` 是主接口。
- `iotManagerSendByLink()` 是底层派发接口。
- `iotManagerUpdateLinkState()` 由 `wireless/cellular/ethernet` 等 manager 上报状态。
- `iotManagerSelectRoute()` 用于手动切换业务链路。
- `iotManagerGetState()` 给上层查询当前总状态。

如果你希望把“获取 MQTT 凭据”做得更明确，可以再加一个专门的接口：

```c
bool iotManagerRequestMqttAuth(eIotManagerLinkId preferredLink);
```

它的职责比 `iotManagerSend(IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP, ...)` 更清晰，因为这不是普通数据通道，而是一个引导动作。

## 9. 对当前版本的直接改造建议

如果你下一步真的要改 `iotmanager.h`，建议按下面顺序做，不要一步到位大改实现。

### 9.1 先改枚举

- 把当前 `IOT_MANAGER_INTERFACE_WIRELESS` 拆成 `BLE` 和 `WIFI` 两个独立链路。
- 保留一个 `CELLULAR`，不要直接拆成 `4G` 和 `5G`。
- 新增 `eIotManagerCellularType`。
- 新增 `eIotManagerServiceId`。
- 给 `serviceId` 增加 `MQTT_AUTH_HTTP` 和 `TCP_SERVER`。

### 9.2 再改状态结构体

- 删除 `ReadyStatus[]`。
- 删除单一 `ActiveInterface` / `TargetInterface` 思路。
- 改成“链路状态表 + 服务路由表”。
- 链路状态中补上 `mqttAuthReady`、`tcpServerListening`、`tcpClientConnected`。

### 9.3 最后改发送接口

- 新增 `iotManagerSend(serviceId, ...)`。
- 原接口可以临时保留为兼容层，但内部最终统一走 `SendByLink()`。
- `MQTT_AUTH_HTTP` 建议优先做成单独引导接口，而不是长期普通发送通道。

## 10. 不建议继续沿用的设计

下面这些方案短期看省事，长期都会拖累你：

### 10.1 `4G`、`5G` 直接作为同级接口

问题：

- 装机互斥，却强行占两个数组槽。
- 路由切换需要写很多永远只会命中一个的分支。
- 后续如果模块继续细分，会越来越乱。

### 10.2 所有业务共用一个 `ActiveInterface`

问题：

- BLE 本地链路和云链路职责完全不同。
- MQTT 引导 HTTP、MQTT 在线链路、TCP Server 监听链路的生命周期都不同。
- 一条链路短暂抖动时，可能误伤所有业务选择。

### 10.3 用 `ready` 一把梭

问题：

- 看不出到底是硬件 ready、联网 ready，还是业务 ready。
- 出问题时日志和状态查询没有诊断价值。

## 11. 推荐的最小落地版本

如果你现在想先做一个“不重构太大、但方向正确”的版本，建议最少做到下面几点：

1. `linkId` 改成 `BLE/WIFI/CELLULAR/ETHERNET`。
2. 增加 `cellularType`，区分 4G/5G。
3. 增加 `serviceId`，至少有 `BLE_LOCAL/MQTT_AUTH_HTTP/MQTT/TCP_SERVER`。
4. 总状态里增加 `mqttAuthRoute`、`mqttRoute`、`tcpServerRoute`，不要只保留一个 `txInterface`。
5. 每条链路至少增加 `installed/moduleReady/netReady/mqttAuthReady/mqttReady/tcpServerListening/tcpClientConnected/peerConnected` 这些字段。

这样即使后面再迭代，也不会再回到当前这种“链路、业务、状态全部揉在一起”的头文件结构。

