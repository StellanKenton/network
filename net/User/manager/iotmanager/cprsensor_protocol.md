# BLE 通信协议总结

本文基于 `User/App/app_wireless.c`、`User/Lib/lib_comm.h`、`User/App/app_update.c`、`User/App/app_memory.c` 的实现整理，仅覆盖 `app_wireless` 中的 BLE 通信协议。

## 1. BLE 链路与特征

- GATT Service: `0xFE60`
- 手机写入特征: `0xFE61`
- 设备通知特征: `0xFE62`
- 设备通过 `fe62,...` 主动上报数据。
- BLE 模式下，帧负载会先做 AES 加密，长度按 16 字节对齐。

## 2. 通用帧格式

BLE 收发共用同一套帧头，区别只在命令字和负载内容。

```c
typedef struct {
    uint8_t head0;         // 固定 0xFA
    uint8_t head1;         // 固定 0xFC
    uint8_t version;       // 当前固定 0x01
    uint8_t cmd;           // 命令字，见下文
    uint16_t data_len_be;  // 大端，BLE 模式下为 AES 对齐后的密文长度
    uint8_t data[];        // BLE 模式下为 AES 密文
    uint16_t crc16_be;     // 大端，CRC16 计算范围: [cmd .. data末尾]
} BleCipherFrame;
```

说明：

- `data_len_be` 在 BLE 模式下不是明文长度，而是对齐后的密文长度。
- 普通业务命令中的多字节字段大多按大端拼装。
- OTA 命令负载内部字段改用小端解析，这一点和普通命令不同。
- 接收流程为：校验帧头 -> 读取 `data_len_be` -> 校验 CRC -> 解密 `data` -> 按 `cmd` 分发。

## 3. 命令字总表

```c
typedef enum {
    E_CMD_HANDSHAKE    = 0x01,
    E_CMD_HEARTBEAT    = 0x03,
    E_CMD_DISCONNECT   = 0x04,
    E_CMD_SELF_CHECK   = 0x05,

    E_CMD_DEV_INFO     = 0x11,
    E_CMD_BLE_INFO     = 0x13,
    E_CMD_WIFI_SETTING = 0x14,
    E_CMD_COMM_SETTING = 0x15,
    E_CMD_TCP_SETTING  = 0x16,

    E_CMD_UPLOAD_METHOD = 0x30,
    E_CMD_CPR_DATA      = 0x31,
    E_CMD_TIME_SYCN     = 0x33,
    E_CMD_BATTERY       = 0x34,
    E_CMD_LANGUAGE      = 0x35,
    E_CMD_VOLUME        = 0x36,
    E_CMD_CPR_RAW_DATA  = 0x37,
    E_CMD_CLEAR_MEMORY  = 0x38,
    E_CMD_BOOT_TIME     = 0x39,
    E_CMD_METRONOME     = 0x3A,
    E_CMD_UTC_SETTING   = 0x3B,
    E_CMD_LOG_DATA      = 0x40,
    E_CMD_HISTORY_DATA  = 0x41,
} WirelessCmd;
```

## 4. 手机下发到设备的帧

这些命令由手机写入 `FE61`，设备解密后进入 `Data_Unpack_Handle()`。

### 4.1 握手 `0x01`

用途：手机回传设备 MAC，用于连接确认。

```c
typedef struct {
    uint8_t mac[6];  // 明文有效载荷，BLE 传输时会被补零到 16 字节后再加密
} BleHandshakePayload;
```

备注：设备收到后与本机 `MAC_ADRESS` 比较，成功后进入已连接状态，并主动回发握手、语言、音量、节拍器、WiFi、UTC 等信息。

### 4.2 心跳请求 `0x03`

用途：手机请求设备立即回复心跳。

```c
typedef struct {
    /* 无 payload */
} BleHeartbeatRequestPayload;
```

### 4.3 断开连接 `0x04`

用途：手机请求设备进入断连流程。

```c
typedef struct {
    /* 无 payload */
} BleDisconnectRequestPayload;
```

### 4.4 查询设备信息 `0x11`

```c
typedef struct {
    /* 无 payload */
} BleDevInfoRequestPayload;
```

### 4.5 查询蓝牙模块信息 `0x13`

```c
typedef struct {
    /* 无 payload */
} BleModuleInfoRequestPayload;
```

### 4.6 设置 WiFi 参数 `0x14`

```c
typedef struct {
    uint8_t ssid_len;
    uint8_t ssid[ssid_len];
    uint8_t pwd_len;
    uint8_t pwd[pwd_len];
} BleWifiSettingPayload;
```

备注：设备收到后会保存 `SSID/PWD`，并置位保存标志。

### 4.7 设置通信优先级 `0x15`

```c
typedef enum {
    BLE_PRIORITY  = 0,
    WIFI_PRIORITY = 1,
} CommPriority;

typedef struct {
    uint8_t priority;  // CommPriority
} BleCommSettingPayload;
```

### 4.8 设置 TCP 参数 `0x16`

```c
typedef struct {
    uint8_t ip_len;
    uint8_t ip[ip_len];
    uint8_t port_len;
    uint8_t port[port_len];
} BleTcpSettingPayload;
```

### 4.9 设置上传方式 `0x30`

```c
typedef struct {
    uint8_t upload_method;  // CPR_Upload_Method_EnumDef
} BleUploadMethodPayload;
```

### 4.10 时间同步 `0x33`

大端时间戳。

```c
typedef struct {
    uint8_t world_time_be[4];
} BleTimeSyncPayload;
```

### 4.11 查询电池信息 `0x34`

```c
typedef struct {
    /* 无 payload */
} BleBatteryRequestPayload;
```

### 4.12 设置语言 `0x35`

```c
typedef struct {
    uint8_t language;  // Audio_Language_EnumDef
} BleLanguagePayload;
```

### 4.13 设置音量 `0x36`

```c
typedef struct {
    uint8_t volume;
} BleVolumePayload;
```

### 4.14 清空历史数据 `0x38`

```c
typedef struct {
    /* 无 payload */
} BleClearMemoryRequestPayload;
```
 
设备收到后会调用 `Set_Memory_Clear(0x01)`，随后再回报清空结果。

### 4.15 设置节拍器频率 `0x3A`

```c
typedef struct {
    uint8_t metronome_freq;
} BleMetronomePayload;
```

### 4.16 设置 UTC 偏移 `0x3B`

代码固定收 6 字节。

```c
typedef struct {
    char utc_offset[6];  // 例如 "+08:00"，代码按 6 字节原样保存
} BleUtcSettingPayload;
```

### 4.17 OTA 请求 `0xEA`

注意：OTA 负载内部字段按小端解析。

```c
typedef struct {
    uint16_t requested_packet_len_le;
} BleOtaRequestPayload;
```

### 4.18 OTA 文件信息 `0xEB`

```c
typedef struct {
    uint8_t version[4];     // 主.次.修订.构建
    uint32_t image_size_le;
    uint32_t image_crc32_le;
} BleOtaFileInfoPayload;
```

### 4.19 OTA 断点查询 `0xEC`

```c
typedef struct {
    /* 无 payload */
} BleOtaOffsetQueryPayload;
```

### 4.20 OTA 数据包 `0xED`

```c
typedef struct {
    uint16_t packet_no_le;
    uint16_t chunk_len_le;
    uint16_t chunk_crc16_le;
    uint8_t chunk[chunk_len_le];
    /* BLE 层面可能补零到 16 字节对齐 */
} BleOtaDataPayload;
```

说明：

- `chunk_len_le` 最大 42 字节。
- `chunk_crc16_le` 仅覆盖 `chunk`。

### 4.21 OTA 结束校验 `0xEE`

```c
typedef struct {
    /* 无 payload */
} BleOtaResultPayload;
```

## 5. 设备上报到手机的帧

这些命令由设备通过 `FE62 Notify` 发送。

### 5.1 握手回包 `0x01`

```c
typedef struct {
    uint8_t mac[6];  // BLE 模式下实际加密前会补零到 16 字节
} BleHandshakeReplyPayload;
```

### 5.2 心跳回包 `0x03`

```c
typedef struct {
    /* 无 payload */
} BleHeartbeatReplyPayload;
```

### 5.3 断连通知 `0x04`

```c
typedef struct {
    /* 无 payload */
} BleDisconnectReplyPayload;
```

### 5.4 自检结果 `0x05`

```c
typedef struct {
    uint8_t feedback_self_check;
    uint8_t power_self_check;
    uint8_t audio_self_check;
    uint8_t wireless_self_check;
    uint8_t memory_self_check;
    uint8_t timestamp_be[4];
} BleSelfCheckReplyPayload;
```

### 5.5 设备信息 `0x11`

```c
typedef struct {
    uint8_t device_type;
    uint8_t device_sn[13];
    uint8_t protocol_or_flag;   // 固定写入 0x01
    uint8_t sw_version;
    uint8_t sw_sub_version;
    uint8_t sw_build_version;
} BleDevInfoReplyPayload;
```

BLE 模式下该结构会补零后加密成 32 字节。

### 5.6 蓝牙模块版本 `0x13`

```c
typedef struct {
    uint8_t ble_version[33];
} BleModuleInfoReplyPayload;
```

BLE 模式下会补齐并加密成 48 字节。

### 5.7 WiFi 设置回显 `0x14`

```c
typedef struct {
    uint8_t ssid_len;
    uint8_t ssid[ssid_len];
    uint8_t pwd_len;
    uint8_t pwd[pwd_len];
} BleWifiSettingReplyPayload;
```

### 5.8 通信优先级回显 `0x15`

```c
typedef struct {
    uint8_t priority;
} BleCommSettingReplyPayload;
```

### 5.9 TCP 设置回显 `0x16`

当前实现只发空负载，未真正回显 IP/Port。

```c
typedef struct {
    /* 当前实现无 payload */
} BleTcpSettingReplyPayload;
```

### 5.10 上传方式状态 `0x30`

```c
typedef struct {
    uint8_t upload_status;  // Memory_SendState.Status
} BleUploadMethodReplyPayload;
```

### 5.11 CPR 实时数据 `0x31`

```c
typedef struct {
    uint8_t timestamp_be[4];
    uint8_t freq_be[2];
    uint8_t depth;
    uint8_t realse_depth;
    uint8_t interval;
    uint8_t boot_timestamp_be[4];
} BleCprDataReplyPayload;
```

### 5.12 时间同步回显 `0x33`

```c
typedef struct {
    uint8_t world_time_be[4];
} BleTimeSyncReplyPayload;
```

### 5.13 电池数据 `0x34`

```c
typedef struct {
    uint8_t bat_percent;
    uint8_t bat_mv_be[2];
    uint8_t charge_state;
} BleBatteryReplyPayload;
```

### 5.14 语言回显 `0x35`

```c
typedef struct {
    uint8_t language;
} BleLanguageReplyPayload;
```

### 5.15 音量回显 `0x36`

```c
typedef struct {
    uint8_t volume;
} BleVolumeReplyPayload;
```

### 5.16 波形数据 `0x37`

当前代码通过 UART 直发 8 字节原始数据，未走 BLE 加密通知路径。

```c
typedef struct {
    uint8_t raw_wave[8];
} BleCprRawDataPayload;
```

### 5.17 清空历史结果 `0x38`

```c
typedef struct {
    uint8_t clear_result;  // Get_Memory_Clear()
} BleClearMemoryReplyPayload;
```

### 5.18 开机时间 `0x39`

```c
typedef struct {
    uint8_t boot_time_be[4];
} BleBootTimeReplyPayload;
```

### 5.19 节拍器频率回显 `0x3A`

```c
typedef struct {
    uint8_t metronome_freq;
} BleMetronomeReplyPayload;
```

### 5.20 UTC 设置回显 `0x3B`

```c
typedef struct {
    char utc_offset[6];
} BleUtcSettingReplyPayload;
```

### 5.21 设备日志 `0x40`

```c
typedef struct {
    uint8_t timestamp_be[4];
    uint8_t charge_status;
    uint8_t dc_voltage_div10;
    uint8_t bat_voltage_div10;
    uint8_t v5_voltage_div10;
    uint8_t v33_voltage_div10;
    uint8_t ble_state;
    uint8_t wifi_state;
    uint8_t cpr_state;
} BleLogReplyPayload;
```

### 5.22 历史数据打包上传 `0x41`

该命令的 payload 不是单一结构体，而是一串子记录拼接后整体加密。

```c
typedef struct {
    uint8_t records[];  // 多个 HistorySubRecord 连续拼接
} BleHistoryReplyPayload;

typedef struct {
    uint8_t record_len;  // 不含自身长度字节
    uint8_t record_cmd;  // 见下方子记录类型
    uint8_t record_data[record_len - 1];
} HistorySubRecord;
```

历史子记录有以下几种：

```c
typedef struct {
    uint8_t record_len;      // 固定 5
    uint8_t record_cmd;      // E_CMD_TIME_SYCN
    uint8_t timestamp_be[4];
} HistoryTimeSyncRecord;

typedef struct {
    uint8_t record_len;      // 固定 5
    uint8_t record_cmd;      // E_CMD_BOOT_TIME
    uint8_t timestamp_be[4];
} HistoryBootTimeRecord;

typedef struct {
    uint8_t record_len;      // 固定 14
    uint8_t record_cmd;      // E_CMD_CPR_DATA
    uint8_t timestamp_be[4];
    uint8_t freq_be[2];
    uint8_t depth;
    uint8_t realse_depth;
    uint8_t interval;
    uint8_t boot_timestamp_be[4];
} HistoryCprRecord;

typedef struct {
    uint8_t record_len;      // 固定 10
    uint8_t record_cmd;      // E_CMD_SELF_CHECK
    uint8_t feedback_self_check;
    uint8_t power_self_check;
    uint8_t audio_self_check;
    uint8_t wireless_self_check;
    uint8_t memory_self_check;
    uint8_t timestamp_be[4];
} HistorySelfCheckRecord;

typedef struct {
    uint8_t record_len;      // 固定 13
    uint8_t record_cmd;      // E_CMD_LOG_DATA
    uint8_t timestamp_be[4];
    uint8_t charge_status;
    uint8_t dc_voltage_div10;
    uint8_t bat_voltage_div10;
    uint8_t v5_voltage_div10;
    uint8_t v33_voltage_div10;
    uint8_t ble_state;
    uint8_t wifi_state;
    uint8_t cpr_state;
} HistoryLogRecord;
```

## 7. 实现要点

1. BLE 模式下几乎所有业务帧都会把明文补零到 16 字节倍数后再 AES 加密，长度字段写入的是补齐后的长度。
2. `0x37` 波形数据当前没有走 BLE Notify 封装，仍是 UART 直发，和其它 BLE 命令不一致。
3. `0x16` TCP 设置回包当前实现为空 payload，只返回命令确认帧。
4. 历史数据 `0x41` 是“帧中套多条子记录”的批量上传格式，抓包解析时需要二次拆包。
